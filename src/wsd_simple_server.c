/*
 * Copyright (c) 2024 roleo.
 * Copyright (c) 2025 Thingino project.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "log.h"
#include "mxml_wrapper.h"
#include "utils.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <ifaddrs.h>
#include <json_config.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#define WSD_MULTICAST_ADDRESS "239.255.255.250"
#define WSD_MULTICAST_ADDRESS_V6 "FF02::C"
#define WSD_PORT 3702

/* Largest UDP payload that fits one datagram at the IPv6 minimum MTU (1280). */
#define WSD_MAX_MSG_LEN 1232

#define WSD_NS_NETWORK "http://www.onvif.org/ver10/network/wsdl"
#define WSD_NS_DEVICE "http://www.onvif.org/ver10/device/wsdl"
#define WSD_TYPE_NVT "NetworkVideoTransmitter"
#define WSD_TYPE_DEVICE "Device"

#define WSD_DEFAULT_CONFIG "/etc/onvif.json"
#define WSD_DEFAULT_TEMPLATE_DIR "/var/www/onvif/wsd_files"
#define WSD_STATE_FILE "/tmp/onvif_discovery_mode"

#define WSD_RECV_BUFFER_LEN 4096
#define WSD_DUP_CACHE_SIZE 16
#define WSD_DUP_CACHE_TTL 5

#define PID_SIZE 32

char template_dir[1024];
char config_file[1024];
char *interface_name;

char address[INET_ADDRSTRLEN];
char address6[INET6_ADDRSTRLEN];
char netmask[16];
char xaddr[1024];
char xaddr6[1024];
char xaddr_s[1024];
char xaddr6_s[1024];

char model[64];
char hardware[64];
char serial[128];
char mac_address[18];
char scopes[2048];
char scopes_xml[4096];

char uuid[UUID_LEN + 1];
char msg_uuid[UUID_LEN + 1];

int debug;
int sock = -1;
int sock_family = AF_UNSPEC;
int have_v4;
int have_v6;
unsigned int if6_idx;
int exit_main;
volatile sig_atomic_t reload_requested;
int non_discoverable;
unsigned int instance_id;
unsigned int msg_number;
unsigned int metadata_version = 1;

int model_set;
int hardware_set;
int scopes_set;

struct sockaddr_storage mcast4;
socklen_t mcast4_len;
struct sockaddr_in6 mcast6;

struct dup_entry {
    char id[128];
    time_t seen;
};

static struct dup_entry dup_cache[WSD_DUP_CACHE_SIZE];
static int dup_cache_next;

static int wsd_daemonize(int flags)
{
    int maxfd, fd;

    switch (fork()) {
    case -1:
        return -1;
    case 0:
        break;
    default:
        _exit(EXIT_SUCCESS);
    }

    if (setsid() == -1)
        return -1;

    switch (fork()) {
    case -1:
        return -1;
    case 0:
        break;
    default:
        _exit(EXIT_SUCCESS);
    }

    if (!(flags & 010))
        umask(0);
    if (!(flags & 01))
        chdir("/");
    if (!(flags & 02)) {
        maxfd = sysconf(_SC_OPEN_MAX);
        if (maxfd == -1)
            maxfd = 8192;
        for (fd = 0; fd < maxfd; fd++)
            close(fd);
    }
    if (!(flags & 04)) {
        close(STDIN_FILENO);
        fd = open("/dev/null", O_RDWR);
        if (fd != STDIN_FILENO)
            return -1;
        if (dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
            return -2;
        if (dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
            return -3;
    }

    return 0;
}

static int check_pid(const char *file_name)
{
    FILE *f;
    long pid;
    char pid_buffer[PID_SIZE];

    f = fopen(file_name, "r");
    if (f == NULL)
        return 0;
    if (fgets(pid_buffer, PID_SIZE, f) == NULL) {
        fclose(f);
        return 0;
    }
    fclose(f);

    if (sscanf(pid_buffer, "%ld", &pid) != 1)
        return 0;

    return kill(pid, 0) == 0 ? 1 : 0;
}

static int acquire_pid_file(char *file_name)
{
    int fd = -1;
    int attempt, len;
    char pid_buffer[PID_SIZE];

    for (attempt = 0; attempt < 2; attempt++) {
        fd = open(file_name, O_WRONLY | O_CREAT | O_EXCL, 0644);
        if (fd >= 0)
            break;
        if (errno != EEXIST)
            return -1;

        if (check_pid(file_name) == 1)
            return 1;

        log_warn("Removing stale pid file %s", file_name);
        if (unlink(file_name) < 0 && errno != ENOENT)
            return -1;
    }
    if (fd < 0)
        return -1;

    len = snprintf(pid_buffer, sizeof(pid_buffer), "%ld\n", (long) getpid());
    if (len < 0 || len >= (int) sizeof(pid_buffer) || write(fd, pid_buffer, len) != len) {
        close(fd);
        unlink(file_name);
        return -1;
    }
    close(fd);

    return 0;
}

static void release_pid_file(char *file_name)
{
    FILE *f;
    long pid = -1;

    f = fopen(file_name, "r");
    if (f == NULL)
        return;
    if (fscanf(f, "%ld", &pid) != 1)
        pid = -1;
    fclose(f);

    if (pid == (long) getpid())
        unlink(file_name);
}

static void signal_handler(int signal)
{
    if (signal == SIGHUP)
        reload_requested = 1;
    else
        exit_main = 1;
}

static void xml_escape(const char *in, char *out, size_t outlen)
{
    size_t used = 0;

    if (outlen == 0)
        return;
    out[0] = '\0';
    if (in == NULL)
        return;

    while (*in && used + 1 < outlen) {
        const char *rep = NULL;
        size_t len = 1;

        switch (*in) {
        case '&':
            rep = "&amp;";
            len = 5;
            break;
        case '<':
            rep = "&lt;";
            len = 4;
            break;
        case '>':
            rep = "&gt;";
            len = 4;
            break;
        case '"':
            rep = "&quot;";
            len = 6;
            break;
        case '\'':
            rep = "&apos;";
            len = 6;
            break;
        default:
            break;
        }

        if (used + len >= outlen)
            break;
        if (rep) {
            memcpy(out + used, rep, len);
            used += len;
        } else {
            out[used++] = *in;
        }
        in++;
    }
    out[used] = '\0';
}

static void trim_copy(char *dst, size_t dstlen, const char *src);

static const char *node_text(mxml_node_t *node)
{
    mxml_node_t *child;

    if (node == NULL)
        return NULL;

    for (child = mxmlGetFirstChild(node); child; child = mxmlGetNextSibling(child)) {
        if (mxmlGetType(child) == MXML_TYPE_TEXT)
            return mxmlGetText(child, NULL);
    }

    return NULL;
}

/*
 * mxml splits whitespace-separated text into multiple text nodes. Join all
 * child text nodes with single spaces so types and scopes lists parse whole.
 */
static void node_text_join(mxml_node_t *node, char *out, size_t outlen)
{
    mxml_node_t *child;
    size_t used = 0;

    if (outlen == 0)
        return;
    out[0] = '\0';
    if (node == NULL)
        return;

    for (child = mxmlGetFirstChild(node); child; child = mxmlGetNextSibling(child)) {
        const char *text;
        char token[512];
        size_t len;

        if (mxmlGetType(child) != MXML_TYPE_TEXT)
            continue;

        text = mxmlGetText(child, NULL);
        if (text == NULL)
            continue;
        trim_copy(token, sizeof(token), text);
        if (token[0] == '\0')
            continue;

        len = strlen(token);
        if (used + (used ? 1 : 0) + len >= outlen)
            break;
        if (used)
            out[used++] = ' ';
        memcpy(out + used, token, len);
        used += len;
        out[used] = '\0';
    }
}

static void trim_copy(char *dst, size_t dstlen, const char *src)
{
    const char *start;
    size_t len;

    if (dstlen == 0)
        return;
    dst[0] = '\0';
    if (src == NULL)
        return;

    while (*src && isspace((unsigned char) *src))
        src++;
    start = src;
    len = strlen(start);
    while (len > 0 && isspace((unsigned char) start[len - 1]))
        len--;
    if (len >= dstlen)
        len = dstlen - 1;
    memcpy(dst, start, len);
    dst[len] = '\0';
}

static const char *find_xmlns(mxml_node_t *node, const char *prefix)
{
    char attr[128];

    if (prefix && prefix[0])
        snprintf(attr, sizeof(attr), "xmlns:%s", prefix);
    else
        snprintf(attr, sizeof(attr), "xmlns");

    while (node) {
        const char *value;

        if (mxmlGetType(node) == MXML_TYPE_ELEMENT) {
            value = mxmlElementGetAttr(node, attr);
            if (value)
                return value;
        }
        node = mxmlGetParent(node);
    }

    return NULL;
}

static int resolve_qname(mxml_node_t *node, const char *qname, char *ns, size_t ns_len, char *local, size_t local_len)
{
    const char *colon;
    const char *value;
    char prefix[64];
    size_t plen;

    ns[0] = '\0';
    local[0] = '\0';

    colon = strchr(qname, ':');
    if (colon) {
        plen = (size_t) (colon - qname);
        if (plen == 0 || plen >= sizeof(prefix))
            return -1;
        memcpy(prefix, qname, plen);
        prefix[plen] = '\0';
        if (strchr(colon + 1, ':'))
            return -1;
        snprintf(local, local_len, "%s", colon + 1);

        if (strcmp(prefix, "xml") == 0) {
            snprintf(ns, ns_len, "http://www.w3.org/XML/1998/namespace");
            return 0;
        }

        value = find_xmlns(node, prefix);
        if (value == NULL)
            return -1;
        snprintf(ns, ns_len, "%s", value);
        return 0;
    }

    snprintf(local, local_len, "%s", qname);
    value = find_xmlns(node, NULL);
    if (value)
        snprintf(ns, ns_len, "%s", value);

    return 0;
}

static int qname_is_device_type(const char *ns, const char *local)
{
    if (strcmp(ns, WSD_NS_NETWORK) == 0 && strcmp(local, WSD_TYPE_NVT) == 0)
        return 1;
    if (strcmp(ns, WSD_NS_DEVICE) == 0 && strcmp(local, WSD_TYPE_DEVICE) == 0)
        return 1;

    return 0;
}

static int types_match(mxml_node_t *types_node)
{
    char text[1024];
    char *copy, *token, *saveptr;
    int matched = 1;

    if (types_node == NULL)
        return 1;

    node_text_join(types_node, text, sizeof(text));
    if (text[0] == '\0')
        return 1;

    copy = strdup(text);
    if (copy == NULL)
        return 0;

    for (token = strtok_r(copy, " \t\r\n", &saveptr); token; token = strtok_r(NULL, " \t\r\n", &saveptr)) {
        char ns[256];
        char local[128];

        if (resolve_qname(types_node, token, ns, sizeof(ns), local, sizeof(local)) < 0) {
            log_debug("Cannot resolve probe type QName %s", token);
            matched = 0;
            break;
        }
        if (!qname_is_device_type(ns, local)) {
            log_debug("Probe type does not match device: %s", token);
            matched = 0;
            break;
        }
    }

    free(copy);
    return matched;
}

typedef struct {
    char scheme[32];
    char authority[256];
    char segment[16][256];
    int segments;
} scope_uri_t;

static void lowercase_ascii(char *s)
{
    while (*s) {
        *s = (char) tolower((unsigned char) *s);
        s++;
    }
}

static void percent_decode(char *s)
{
    char *src = s;
    char *dst = s;

    while (*src) {
        if (src[0] == '%' && isxdigit((unsigned char) src[1]) && isxdigit((unsigned char) src[2])) {
            char hex[3] = {src[1], src[2], '\0'};
            *dst++ = (char) strtol(hex, NULL, 16);
            src += 3;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

static int parse_scope_uri(const char *uri, scope_uri_t *out)
{
    const char *p, *scheme_end, *authority_end, *path_start, *path_end;
    char authority[256];
    char path[1024];
    char *token, *saveptr;
    size_t len;

    memset(out, 0, sizeof(*out));

    scheme_end = strstr(uri, "://");
    if (scheme_end == NULL)
        return -1;

    len = (size_t) (scheme_end - uri);
    if (len == 0 || len >= sizeof(out->scheme))
        return -1;
    memcpy(out->scheme, uri, len);
    out->scheme[len] = '\0';
    lowercase_ascii(out->scheme);

    p = scheme_end + 3;
    authority_end = p;
    while (*authority_end && *authority_end != '/' && *authority_end != '?' && *authority_end != '#')
        authority_end++;

    len = (size_t) (authority_end - p);
    if (len >= sizeof(authority))
        return -1;
    memcpy(authority, p, len);
    authority[len] = '\0';
    lowercase_ascii(authority);
    snprintf(out->authority, sizeof(out->authority), "%s", authority);

    if (*authority_end != '/')
        return 0;

    path_start = authority_end;
    path_end = path_start;
    while (*path_end && *path_end != '?' && *path_end != '#')
        path_end++;

    len = (size_t) (path_end - path_start);
    if (len >= sizeof(path))
        len = sizeof(path) - 1;
    memcpy(path, path_start, len);
    path[len] = '\0';

    for (token = strtok_r(path, "/", &saveptr); token; token = strtok_r(NULL, "/", &saveptr)) {
        if (strcmp(token, ".") == 0 || strcmp(token, "..") == 0)
            return -1;
        if (out->segments >= (int) (sizeof(out->segment) / sizeof(out->segment[0])))
            return -1;
        snprintf(out->segment[out->segments], sizeof(out->segment[0]), "%s", token);
        percent_decode(out->segment[out->segments]);
        out->segments++;
    }

    return 0;
}

static int scope_match_uri(const char *probe_scope, const char *target_scope, const char *rule)
{
    scope_uri_t probe;
    scope_uri_t target;
    int i;

    if (rule && strstr(rule, "/strcmp0"))
        return strcmp(probe_scope, target_scope) == 0;
    if (rule && strstr(rule, "/uuid"))
        return strcasecmp(probe_scope, target_scope) == 0;

    if (parse_scope_uri(probe_scope, &probe) < 0)
        return 0;
    if (parse_scope_uri(target_scope, &target) < 0)
        return 0;

    if (strcasecmp(probe.scheme, target.scheme) != 0)
        return 0;
    if (strcasecmp(probe.authority, target.authority) != 0)
        return 0;
    if (probe.segments > target.segments)
        return 0;

    for (i = 0; i < probe.segments; i++) {
        if (strcmp(probe.segment[i], target.segment[i]) != 0)
            return 0;
    }

    return 1;
}

static int scopes_match(mxml_node_t *scopes_node)
{
    char text[2048];
    const char *match_by;
    char *probe_copy;
    char *device_copy;
    char *probe_token, *probe_save;
    int matched = 1;

    if (scopes_node == NULL)
        return 1;

    node_text_join(scopes_node, text, sizeof(text));
    if (text[0] == '\0')
        return 1;
    log_debug("Probe scopes text: %s", text);

    match_by = mxmlElementGetAttr(scopes_node, "MatchBy");
    if (match_by == NULL || match_by[0] == '\0')
        match_by = "http://schemas.xmlsoap.org/ws/2005/04/discovery/rfc2396";
    if (strstr(match_by, "/rfc3986") == NULL && strstr(match_by, "/rfc2396") == NULL && strstr(match_by, "/strcmp0") == NULL
        && strstr(match_by, "/uuid") == NULL) {
        log_debug("Unsupported scope matching rule %s", match_by);
        return 0;
    }

    probe_copy = strdup(text);
    device_copy = strdup(scopes);
    if (probe_copy == NULL || device_copy == NULL) {
        free(probe_copy);
        free(device_copy);
        return 0;
    }

    for (probe_token = strtok_r(probe_copy, " \t\r\n", &probe_save); probe_token; probe_token = strtok_r(NULL, " \t\r\n", &probe_save)) {
        char *device_token, *device_save;
        int token_matched = 0;

        for (device_token = strtok_r(device_copy, " \t\r\n", &device_save); device_token; device_token = strtok_r(NULL, " \t\r\n", &device_save)) {
            if (scope_match_uri(probe_token, device_token, match_by)) {
                token_matched = 1;
                break;
            }
        }

        log_debug("Probe scope %s: %s", probe_token, token_matched ? "match" : "no match");
        if (!token_matched) {
            matched = 0;
            break;
        }

        /* strtok_r consumed device_copy; rebuild it for the next probe scope. */
        free(device_copy);
        device_copy = strdup(scopes);
        if (device_copy == NULL) {
            matched = 0;
            break;
        }
    }

    free(probe_copy);
    free(device_copy);

    return matched;
}

static int message_seen(const char *id)
{
    time_t now = time(NULL);
    int i;

    if (id == NULL || id[0] == '\0')
        return 0;

    for (i = 0; i < WSD_DUP_CACHE_SIZE; i++) {
        if (dup_cache[i].id[0] && now - dup_cache[i].seen <= WSD_DUP_CACHE_TTL && strcmp(dup_cache[i].id, id) == 0)
            return 1;
    }

    snprintf(dup_cache[dup_cache_next].id, sizeof(dup_cache[dup_cache_next].id), "%s", id);
    dup_cache[dup_cache_next].seen = now;
    dup_cache_next = (dup_cache_next + 1) % WSD_DUP_CACHE_SIZE;

    return 0;
}

static void sanitize_scope_component(const char *in, char *out, size_t outlen)
{
    size_t used = 0;

    if (outlen == 0)
        return;

    while (*in && used + 1 < outlen) {
        unsigned char c = (unsigned char) *in;

        if (isspace(c) || c == '/')
            out[used++] = '_';
        else
            out[used++] = (char) c;
        in++;
    }
    out[used] = '\0';
}

static const char *json_string(JsonValue *obj, const char *key)
{
    JsonValue *value;

    if (obj == NULL)
        return NULL;
    value = get_object_item(obj, key);
    if (value == NULL || value->type != JSON_STRING || value->value.string == NULL)
        return NULL;

    return value->value.string;
}

static void read_os_release_model(void)
{
    FILE *f;
    char line[256];

    f = fopen("/etc/os-release", "r");
    if (f == NULL)
        return;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "IMAGE_ID=", 9) == 0) {
            char *val = line + 9;
            size_t len = strlen(val);

            while (len > 0 && (val[len - 1] == '\n' || val[len - 1] == '\r'))
                val[--len] = '\0';
            if (!model_set && len > 0 && len < sizeof(model))
                snprintf(model, sizeof(model), "%s", val);
            break;
        }
    }
    fclose(f);
}

static int read_discovery_mode_state(void)
{
    FILE *f;
    char line[64];
    int mode = 0;

    f = fopen(WSD_STATE_FILE, "r");
    if (f) {
        if (fgets(line, sizeof(line), f)) {
            size_t len = strlen(line);
            while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' || line[len - 1] == ' ' || line[len - 1] == '\t'))
                line[--len] = '\0';
            if (strcasecmp(line, "NonDiscoverable") == 0)
                mode = 1;
        }
        fclose(f);
        return mode;
    }

    return -1;
}

static void build_scopes_from_config(JsonValue *root)
{
    JsonValue *array;
    int count, i;
    size_t used = 0;

    array = get_object_item(root, "scopes");
    count = get_array_size(array);
    if (count <= 0)
        return;

    scopes[0] = '\0';
    for (i = 0; i < count; i++) {
        JsonValue *item = get_array_item(array, i);
        const char *value = json_object_get_string(item);
        int n;

        if (value == NULL || value[0] == '\0')
            continue;
        n = snprintf(scopes + used, sizeof(scopes) - used, "%s%s", used ? " " : "", value);
        if (n < 0 || (size_t) n >= sizeof(scopes) - used)
            break;
        used += (size_t) n;
    }
}

static void build_default_scopes(void)
{
    char hw[128], md[128], mac[32], sn[160];
    size_t used = 0;
    int n;

    sanitize_scope_component(hardware[0] ? hardware : "thingino", hw, sizeof(hw));
    sanitize_scope_component(model[0] ? model : "Thingino", md, sizeof(md));
    sanitize_scope_component(mac_address, mac, sizeof(mac));
    sanitize_scope_component(serial, sn, sizeof(sn));

    n = snprintf(scopes,
                 sizeof(scopes),
                 "onvif://www.onvif.org/type/video_encoder "
                 "onvif://www.onvif.org/Profile/Streaming "
                 "onvif://www.onvif.org/hardware/%s "
                 "onvif://www.onvif.org/name/%s",
                 hw,
                 md);
    if (n < 0 || (size_t) n >= sizeof(scopes))
        return;
    used = (size_t) n;

    if (mac[0] && used < sizeof(scopes)) {
        n = snprintf(scopes + used, sizeof(scopes) - used, " onvif://www.onvif.org/mac/%s", mac);
        if (n > 0 && (size_t) n < sizeof(scopes) - used)
            used += (size_t) n;
    }

    if (sn[0] && used < sizeof(scopes)) {
        n = snprintf(scopes + used, sizeof(scopes) - used, " onvif://www.onvif.org/SerialNumber/%s", sn);
        if (n > 0 && (size_t) n < sizeof(scopes) - used)
            used += (size_t) n;
    }

    if (used < sizeof(scopes))
        snprintf(scopes + used, sizeof(scopes) - used, " onvif://www.onvif.org/location/anywhere");
}

static void load_config_values(void)
{
    JsonValue *root = NULL;
    JsonValue *camera = NULL;
    const char *value;

    if (access(config_file, R_OK) == 0) {
        root = load_config(config_file);
        if (root == NULL)
            log_warn("Cannot parse config file %s", config_file);
    }

    if (root) {
        camera = get_object_item(root, "camera");

        if (!model_set) {
            value = json_string(camera, "model");
            if (value == NULL)
                value = json_string(root, "model");
            if (value && value[0])
                snprintf(model, sizeof(model), "%s", value);
        }

        if (!hardware_set) {
            value = json_string(camera, "hardware_id");
            if (value == NULL)
                value = json_string(root, "hardware_id");
            if (value && value[0])
                snprintf(hardware, sizeof(hardware), "%s", value);
        }

        if (!serial[0]) {
            value = json_string(camera, "serial_num");
            if (value == NULL)
                value = json_string(root, "serial_num");
            if (value && value[0])
                snprintf(serial, sizeof(serial), "%s", value);
        }

        if (!scopes_set) {
            build_scopes_from_config(root);
            if (scopes[0] == '\0')
                build_default_scopes();
        }
    }

    if (root)
        free_json_value(root);

    if (scopes[0] == '\0')
        build_default_scopes();

    xml_escape(scopes, scopes_xml, sizeof(scopes_xml));
}

static void read_discovery_mode(void)
{
    int state = read_discovery_mode_state();

    non_discoverable = state == 1;
}

static void format_xaddr(const char *template, const char *addr, int is_v6, const char *ifname, char *out, size_t outlen)
{
    char host[INET6_ADDRSTRLEN + 64];

    if (template == NULL || template[0] == '\0') {
        out[0] = '\0';
        return;
    }

    if (strstr(template, "%s") == NULL) {
        snprintf(out, outlen, "%s", template);
        return;
    }

    if (is_v6) {
        if (strncasecmp(addr, "fe80:", 5) == 0 && ifname && ifname[0])
            snprintf(host, sizeof(host), "[%s%%25%s]", addr, ifname);
        else
            snprintf(host, sizeof(host), "[%s]", addr);
    } else {
        snprintf(host, sizeof(host), "%s", addr);
    }

    snprintf(out, outlen, template, host);
}

static char *render_message(const char *file_name, const char *relates_to, const char *xaddr_str)
{
    char path[1200];
    char relates_escaped[256];
    char msg_uuid_local[UUID_LEN + 1];
    char instance_str[16];
    char number_str[16];
    char metadata_str[16];
    char *message;
    long size;

    snprintf(path, sizeof(path), "%s/%s", template_dir, file_name);
    msg_number++;
    gen_uuid(msg_uuid_local);
    snprintf(instance_str, sizeof(instance_str), "%u", instance_id);
    snprintf(number_str, sizeof(number_str), "%u", msg_number);
    snprintf(metadata_str, sizeof(metadata_str), "%u", metadata_version);
    xml_escape(relates_to ? relates_to : "", relates_escaped, sizeof(relates_escaped));

    size = cat(NULL,
               path,
               16,
               "%MSG_UUID%",
               msg_uuid_local,
               "%REL_TO_UUID%",
               relates_escaped,
               "%INSTANCE_ID%",
               instance_str,
               "%MSG_NUMBER%",
               number_str,
               "%UUID%",
               uuid,
               "%ADDRESS%",
               (char *) xaddr_str,
               "%SCOPES%",
               scopes_xml,
               "%METADATA_VERSION%",
               metadata_str);
    if (size < 0)
        return NULL;

    message = (char *) malloc((size_t) size + 1);
    if (message == NULL)
        return NULL;

    cat(message,
        path,
        16,
        "%MSG_UUID%",
        msg_uuid_local,
        "%REL_TO_UUID%",
        relates_escaped,
        "%INSTANCE_ID%",
        instance_str,
        "%MSG_NUMBER%",
        number_str,
        "%UUID%",
        uuid,
        "%ADDRESS%",
        (char *) xaddr_str,
        "%SCOPES%",
        scopes_xml,
        "%METADATA_VERSION%",
        metadata_str);

    return message;
}

static int send_wsd_message(int s, const char *message, const struct sockaddr *dst, socklen_t dstlen)
{
    size_t len = strlen(message);

    if (len > WSD_MAX_MSG_LEN)
        log_warn("WSD message is %zu bytes (limit %d) and may be fragmented", len, WSD_MAX_MSG_LEN);

    if (sendto(s, message, len, 0, dst, dstlen) < 0) {
        log_error("Cannot send WSD message: %s", strerror(errno));
        return -1;
    }

    return 0;
}

static void send_bye(const char *xaddr_str, const struct sockaddr *dst, socklen_t dstlen)
{
    char *message;

    if (sock < 0 || dst == NULL)
        return;

    message = render_message("Bye.xml", "", xaddr_str);
    if (message == NULL) {
        log_error("Cannot render Bye message");
        return;
    }

    if (send_wsd_message(sock, message, dst, dstlen) == 0)
        log_info("Bye sent");
    free(message);
}

static void send_hello(const char *xaddr_str, const struct sockaddr *dst, socklen_t dstlen)
{
    char *message;

    if (sock < 0 || dst == NULL)
        return;

    message = render_message("Hello.xml", "", xaddr_str);
    if (message == NULL) {
        log_error("Cannot render Hello message");
        return;
    }

    if (send_wsd_message(sock, message, dst, dstlen) == 0)
        log_info("Hello sent");
    free(message);
}

static void send_hello_all(void)
{
    if (have_v4)
        send_hello(xaddr, (struct sockaddr *) &mcast4, mcast4_len);
    if (have_v6)
        send_hello(xaddr6, (struct sockaddr *) &mcast6, sizeof(mcast6));
}

static void send_bye_all(void)
{
    if (have_v4)
        send_bye(xaddr, (struct sockaddr *) &mcast4, mcast4_len);
    if (have_v6)
        send_bye(xaddr6, (struct sockaddr *) &mcast6, sizeof(mcast6));
}

static int open_socket(void)
{
    int yes = 1;
    int no = 0;

    have_v4 = get_ip_address(address, netmask, interface_name) == 0 && address[0] != '\0';
    have_v6 = get_ip6_address(address6, sizeof(address6), interface_name) == 0 && address6[0] != '\0';
    if6_idx = have_v6 ? if_nametoindex(interface_name) : 0;

    if (!have_v4 && !have_v6)
        return -1;

    format_xaddr(xaddr_s, address, 0, interface_name, xaddr, sizeof(xaddr));
    if (have_v6)
        format_xaddr(xaddr6_s[0] ? xaddr6_s : xaddr_s, address6, 1, interface_name, xaddr6, sizeof(xaddr6));

    sock_family = have_v6 ? AF_INET6 : AF_INET;
    sock = socket(sock_family, SOCK_DGRAM, 0);
    if (sock < 0 && sock_family == AF_INET6) {
        log_warn("AF_INET6 socket unavailable (%s); falling back to IPv4 only", strerror(errno));
        sock_family = AF_INET;
        have_v6 = 0;
        sock = socket(AF_INET, SOCK_DGRAM, 0);
    }
    if (sock < 0)
        return -1;

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#ifdef SO_REUSEPORT
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));
#endif

    if (sock_family == AF_INET6) {
        struct sockaddr_in6 bind6;

        setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no));
        memset(&bind6, 0, sizeof(bind6));
        bind6.sin6_family = AF_INET6;
        bind6.sin6_port = htons(WSD_PORT);
        bind6.sin6_addr = in6addr_any;
        if (bind(sock, (struct sockaddr *) &bind6, sizeof(bind6)) < 0) {
            log_fatal("Unable to bind socket: %s", strerror(errno));
            close(sock);
            sock = -1;
            return -1;
        }
    } else {
        struct sockaddr_in bind4;

        memset(&bind4, 0, sizeof(bind4));
        bind4.sin_family = AF_INET;
        bind4.sin_port = htons(WSD_PORT);
        bind4.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(sock, (struct sockaddr *) &bind4, sizeof(bind4)) < 0) {
            log_fatal("Unable to bind socket: %s", strerror(errno));
            close(sock);
            sock = -1;
            return -1;
        }
    }

    if (have_v4) {
        struct ip_mreq mreq4;
        unsigned char ttl = 1;

        memset(&mreq4, 0, sizeof(mreq4));
        inet_pton(AF_INET, WSD_MULTICAST_ADDRESS, &mreq4.imr_multiaddr);
        mreq4.imr_interface.s_addr = inet_addr(address);
        if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq4, sizeof(mreq4)) < 0) {
            log_warn("Cannot join IPv4 multicast group: %s", strerror(errno));
            have_v4 = 0;
        } else {
            setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &mreq4.imr_interface, sizeof(mreq4.imr_interface));
            setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
        }
    }

    memset(&mcast4, 0, sizeof(mcast4));
    if (sock_family == AF_INET6) {
        struct sockaddr_in6 *m = (struct sockaddr_in6 *) &mcast4;

        m->sin6_family = AF_INET6;
        m->sin6_port = htons(WSD_PORT);
        inet_pton(AF_INET6, "::ffff:" WSD_MULTICAST_ADDRESS, &m->sin6_addr);
        mcast4_len = sizeof(struct sockaddr_in6);
    } else {
        struct sockaddr_in *m = (struct sockaddr_in *) &mcast4;

        m->sin_family = AF_INET;
        m->sin_port = htons(WSD_PORT);
        inet_pton(AF_INET, WSD_MULTICAST_ADDRESS, &m->sin_addr);
        mcast4_len = sizeof(struct sockaddr_in);
    }

    if (have_v6) {
        struct ipv6_mreq mreq6;
        unsigned int hops = 1;

        memset(&mreq6, 0, sizeof(mreq6));
        inet_pton(AF_INET6, WSD_MULTICAST_ADDRESS_V6, &mreq6.ipv6mr_multiaddr);
        mreq6.ipv6mr_interface = if6_idx;
        if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq6, sizeof(mreq6)) < 0) {
            log_warn("Cannot join IPv6 multicast group: %s", strerror(errno));
            have_v6 = 0;
        } else {
            if (if6_idx)
                setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_IF, &if6_idx, sizeof(if6_idx));
            setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &hops, sizeof(hops));

            memset(&mcast6, 0, sizeof(mcast6));
            mcast6.sin6_family = AF_INET6;
            mcast6.sin6_port = htons(WSD_PORT);
            inet_pton(AF_INET6, WSD_MULTICAST_ADDRESS_V6, &mcast6.sin6_addr);
            mcast6.sin6_scope_id = if6_idx;
        }
    }

    if (!have_v4 && !have_v6) {
        close(sock);
        sock = -1;
        return -1;
    }

    log_info("WSD socket ready (%s%s%s)", have_v4 ? "IPv4" : "", have_v4 && have_v6 ? " + " : "", have_v6 ? "IPv6" : "");
    return 0;
}

static void close_socket(void)
{
    if (sock >= 0) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
        sock = -1;
    }
}

static void handle_probe(const char *relates_to, const struct sockaddr *dst, socklen_t dstlen, const char *xaddr_str)
{
    char *message;

    usleep((50 + rand() % 200) * 1000);
    message = render_message("ProbeMatches.xml", relates_to, xaddr_str);
    if (message == NULL) {
        log_error("Cannot render ProbeMatches message");
        return;
    }

    if (send_wsd_message(sock, message, dst, dstlen) == 0)
        log_info("ProbeMatches sent");
    free(message);
}

static void handle_resolve(const char *relates_to, const struct sockaddr *dst, socklen_t dstlen, const char *xaddr_str)
{
    char *message;

    message = render_message("ResolveMatches.xml", relates_to, xaddr_str);
    if (message == NULL) {
        log_error("Cannot render ResolveMatches message");
        return;
    }

    if (send_wsd_message(sock, message, dst, dstlen) == 0)
        log_info("ResolveMatches sent");
    free(message);
}

static int resolve_matches(mxml_node_t *resolve_node)
{
    mxml_node_t *epr;
    mxml_node_t *addr_node;
    const char *text;
    const char *value;

    epr = get_element_ptr(resolve_node, "EndpointReference", NULL);
    if (epr == NULL)
        return 0;
    if (get_element_in_element_ptr("ReferenceParameters", epr) != NULL || get_element_in_element_ptr("ReferenceProperties", epr) != NULL)
        return 0;
    addr_node = get_element_in_element_ptr("Address", epr);
    if (addr_node == NULL)
        return 0;

    text = node_text(addr_node);
    if (text == NULL)
        return 0;

    value = text;
    if (strncasecmp(value, "urn:uuid:", 9) == 0)
        value += 9;
    else if (strncasecmp(value, "uuid:", 5) == 0)
        value += 5;

    return strcasecmp(value, uuid) == 0;
}

static const char *choose_xaddr(const struct sockaddr_storage *sender)
{
    int is_v4;

    is_v4 = sender->ss_family == AF_INET
            || (sender->ss_family == AF_INET6 && IN6_IS_ADDR_V4MAPPED(&((const struct sockaddr_in6 *) sender)->sin6_addr));

    if (is_v4)
        return have_v4 ? xaddr : NULL;

    return have_v6 ? xaddr6 : NULL;
}

static void process_message(char *buffer, const struct sockaddr_storage *sender, socklen_t sender_len)
{
    mxml_node_t *body_node = NULL;
    mxml_node_t *probe_node = NULL;
    mxml_node_t *resolve_node = NULL;
    mxml_node_t *scopes_node;
    mxml_node_t *types_node;
    const char *method;
    const char *message_id_text;
    const char *xaddr_str;
    char message_id[128];

    init_xml(buffer, (int) strlen(buffer));
    method = get_method(1);
    if (method == NULL) {
        close_xml();
        return;
    }

    if (strcasecmp(method, "Resolve") == 0) {
        body_node = get_element_ptr(NULL, "Resolve", "Body");
        resolve_node = body_node;
    } else if (strstr(method, "Probe") != NULL && strstr(method, "ProbeMatches") == NULL) {
        /* Accept standard Probe plus the NetdevProbe/UniviewProbe variants. */
        body_node = get_element_ptr(NULL, (char *) method, "Body");
        probe_node = body_node;
    } else {
        log_debug("Ignoring WS-Discovery method %s", method);
        close_xml();
        return;
    }

    if (body_node == NULL) {
        log_debug("Cannot find %s body", method);
        close_xml();
        return;
    }

    message_id_text = get_element("MessageID", "Header");
    trim_copy(message_id, sizeof(message_id), message_id_text);

    xaddr_str = choose_xaddr(sender);
    if (xaddr_str == NULL) {
        log_debug("No matching address family for incoming %s", method);
        close_xml();
        return;
    }

    if (probe_node) {
        if (message_seen(message_id)) {
            log_debug("Duplicate Probe %s ignored", message_id);
            close_xml();
            return;
        }

        types_node = get_element_in_element_ptr("Types", probe_node);
        scopes_node = get_element_in_element_ptr("Scopes", probe_node);

        if (!types_match(types_node) || !scopes_match(scopes_node)) {
            log_debug("Probe does not match this device");
            close_xml();
            return;
        }

        if (message_id[0] == '\0') {
            log_debug("Probe without MessageID ignored");
            close_xml();
            return;
        }

        log_info("Probe matched, sending ProbeMatches");
        close_xml();
        handle_probe(message_id, (const struct sockaddr *) sender, sender_len, xaddr_str);
        return;
    }

    if (resolve_node) {
        if (message_seen(message_id)) {
            log_debug("Duplicate Resolve %s ignored", message_id);
            close_xml();
            return;
        }

        if (!resolve_matches(resolve_node)) {
            log_debug("Resolve endpoint reference does not match");
            close_xml();
            return;
        }

        if (message_id[0] == '\0') {
            log_debug("Resolve without MessageID ignored");
            close_xml();
            return;
        }

        log_info("Resolve matched, sending ResolveMatches");
        close_xml();
        handle_resolve(message_id, (const struct sockaddr *) sender, sender_len, xaddr_str);
        return;
    }

    close_xml();
}

static void reload_discovery(void)
{
    char old_scopes[sizeof(scopes)];
    int old_mode = non_discoverable;

    snprintf(old_scopes, sizeof(old_scopes), "%s", scopes);

    load_config_values();
    read_discovery_mode();

    if (non_discoverable != old_mode) {
        if (non_discoverable) {
            log_info("Entering non-discoverable mode");
            send_bye_all();
            close_socket();
        } else {
            log_info("Entering discoverable mode");
            have_v4 = have_v6 = 0;
            if (open_socket() == 0) {
                sleep(1);
                send_hello_all();
            } else {
                log_error("Cannot open WSD socket; staying non-discoverable");
                non_discoverable = 1;
            }
        }
        return;
    }

    if (!non_discoverable && strcmp(old_scopes, scopes) != 0) {
        metadata_version++;
        log_info("Discovery metadata changed, sending updated Hello");
        send_hello_all();
    }
}

static void print_usage(char *progname)
{
    fprintf(
        stderr,
        "\nUsage: %s -i INTERFACE -x XADDR [-6 XADDR6] [-m MODEL] [-n HARDWARE] [-s SCOPES] [-c CONFIG] -p PID_FILE [-t TEMPLATE_DIR] [-f] [-d LEVEL]\n\n",
        progname);
    fprintf(stderr, "\t-i, --if_name\n");
    fprintf(stderr, "\t\tnetwork interface\n");
    fprintf(stderr, "\t-x, --xaddr\n");
    fprintf(stderr, "\t\tIPv4 resource address template (use %%s for the address)\n");
    fprintf(stderr, "\t-6, --xaddr6\n");
    fprintf(stderr, "\t\tIPv6 resource address template (defaults to -x with brackets)\n");
    fprintf(stderr, "\t-m, --model\n");
    fprintf(stderr, "\t\tmodel name\n");
    fprintf(stderr, "\t-n, --hardware\n");
    fprintf(stderr, "\t\thardware manufacturer\n");
    fprintf(stderr, "\t-s, --scopes\n");
    fprintf(stderr, "\t\tspace-separated scope list (overrides config)\n");
    fprintf(stderr, "\t-c, --config\n");
    fprintf(stderr, "\t\tJSON config file (default: %s)\n", WSD_DEFAULT_CONFIG);
    fprintf(stderr, "\t-p, --pid_file\n");
    fprintf(stderr, "\t\tpid file\n");
    fprintf(stderr, "\t-t, --template_dir\n");
    fprintf(stderr, "\t\ttemplate directory (default: %s)\n", WSD_DEFAULT_TEMPLATE_DIR);
    fprintf(stderr, "\t-f, --foreground\n");
    fprintf(stderr, "\t\tdon't daemonize\n");
    fprintf(stderr, "\t-d LEVEL, --debug LEVEL\n");
    fprintf(stderr, "\t\tverbosity 0..5 (0=FATAL only, 5=TRACE; default 0)\n");
    fprintf(stderr, "\t-h, --help\n");
    fprintf(stderr, "\t\tprint this help\n");
}

int main(int argc, char **argv)
{
    int c, ret;
    char *if_name = NULL;
    char *pid_file = NULL;
    char *endptr;
    char *opt_xaddr = NULL;
    int foreground = 0;
    char recv_buffer[WSD_RECV_BUFFER_LEN];
    struct sigaction sa;

    snprintf(template_dir, sizeof(template_dir), "%s", WSD_DEFAULT_TEMPLATE_DIR);
    snprintf(config_file, sizeof(config_file), "%s", WSD_DEFAULT_CONFIG);
    snprintf(model, sizeof(model), "%s", "MODEL_NAME");
    snprintf(hardware, sizeof(hardware), "%s", "thingino");
    debug = 0;

    while (1) {
        static struct option long_options[] = {{"if_name", required_argument, 0, 'i'},
                                               {"xaddr", required_argument, 0, 'x'},
                                               {"xaddr6", required_argument, 0, '6'},
                                               {"model", required_argument, 0, 'm'},
                                               {"hardware", required_argument, 0, 'n'},
                                               {"scopes", required_argument, 0, 's'},
                                               {"config", required_argument, 0, 'c'},
                                               {"pid_file", required_argument, 0, 'p'},
                                               {"template_dir", required_argument, 0, 't'},
                                               {"foreground", no_argument, 0, 'f'},
                                               {"debug", required_argument, 0, 'd'},
                                               {"help", no_argument, 0, 'h'},
                                               {0, 0, 0, 0}};
        int option_index = 0;

        c = getopt_long(argc, argv, "i:x:6:m:n:s:c:p:t:fd:h", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'i':
            if_name = optarg;
            break;
        case 'x':
            opt_xaddr = optarg;
            snprintf(xaddr_s, sizeof(xaddr_s), "%s", optarg);
            break;
        case '6':
            snprintf(xaddr6_s, sizeof(xaddr6_s), "%s", optarg);
            break;
        case 'm':
            snprintf(model, sizeof(model), "%s", optarg);
            model_set = 1;
            break;
        case 'n':
            snprintf(hardware, sizeof(hardware), "%s", optarg);
            hardware_set = 1;
            break;
        case 's':
            snprintf(scopes, sizeof(scopes), "%s", optarg);
            scopes_set = 1;
            break;
        case 'c':
            snprintf(config_file, sizeof(config_file), "%s", optarg);
            break;
        case 'p':
            pid_file = optarg;
            break;
        case 't':
            snprintf(template_dir, sizeof(template_dir), "%s", optarg);
            break;
        case 'f':
            foreground = 1;
            break;
        case 'd':
            debug = (int) strtol(optarg, &endptr, 10);
            if (endptr == optarg || debug < 0 || debug > 5) {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 'h':
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);
        default:
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);
        }
    }

    if (if_name == NULL || opt_xaddr == NULL || pid_file == NULL) {
        print_usage(argv[0]);
        exit(EXIT_SUCCESS);
    }

    interface_name = if_name;

    sigemptyset(&sa.sa_mask);
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0; /* no SA_RESTART: interrupt select/pause for reload */
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    if (!foreground) {
        ret = wsd_daemonize(0);
        if (ret) {
            fprintf(stderr, "Error starting daemon.\n");
            exit(EXIT_FAILURE);
        }
    }

    log_init("wsd_simple_server", LOG_DAEMON, debug, foreground);
    log_info("Starting program");

    read_os_release_model();

    if (get_mac_address(mac_address, if_name) != 0) {
        log_warn("Unable to get MAC address for %s", if_name);
        mac_address[0] = '\0';
    }

    /* Load config once so -m/-n/-s defaults and discovery mode are known. */
    load_config_values();
    read_discovery_mode();

    {
        char persistent_mac[32];
        char identity[192];

        get_persistent_mac(persistent_mac, sizeof(persistent_mac));
        if (serial[0])
            snprintf(identity, sizeof(identity), "serial:%s", serial);
        else if (persistent_mac[0])
            snprintf(identity, sizeof(identity), "mac:%s", persistent_mac);
        else if (mac_address[0])
            snprintf(identity, sizeof(identity), "mac:%s", mac_address);
        else
            identity[0] = '\0';

        if (identity[0])
            gen_uuid_v5(identity, uuid);
        else
            gen_uuid(uuid);
    }

    instance_id = (unsigned int) time(NULL);
    if (instance_id == 0)
        instance_id = 1;
    log_debug("Argument if_name = %s", if_name);
    log_debug("Argument pid_file = %s", pid_file);
    log_debug("Argument xaddr = %s", opt_xaddr);
    log_debug("Argument scopes = %s", scopes);
    log_debug("Device UUID = %s", uuid);
    log_debug("Discovery mode = %s", non_discoverable ? "NonDiscoverable" : "Discoverable");

    ret = acquire_pid_file(pid_file);
    if (ret == 1) {
        log_fatal("Program is already running.");
        exit(EXIT_FAILURE);
    } else if (ret != 0) {
        log_fatal("Error creating pid file %s", pid_file);
        exit(EXIT_FAILURE);
    }

    if (access(template_dir, F_OK) == -1) {
        log_fatal("Unable to open directory %s", template_dir);
        release_pid_file(pid_file);
        exit(EXIT_FAILURE);
    }
    {
        DIR *dirptr = opendir(template_dir);
        if (dirptr == NULL) {
            log_fatal("Unable to open directory %s", template_dir);
            release_pid_file(pid_file);
            exit(EXIT_FAILURE);
        }
        closedir(dirptr);
    }

    srand((unsigned int) time(NULL));
    exit_main = 0;

    if (!non_discoverable) {
        if (open_socket() != 0) {
            log_fatal("No usable IPv4 or IPv6 address found on %s", if_name);
            release_pid_file(pid_file);
            exit(EXIT_FAILURE);
        }
        sleep(1);
        send_hello_all();
    } else {
        log_info("Non-discoverable mode: not opening the discovery socket");
    }

    while (!exit_main) {
        if (non_discoverable) {
            pause();
            if (reload_requested) {
                reload_requested = 0;
                reload_discovery();
            }
            continue;
        }

        if (sock >= 0) {
            fd_set rfds;
            struct timeval tv;
            int ready;

            FD_ZERO(&rfds);
            FD_SET(sock, &rfds);
            tv.tv_sec = 1;
            tv.tv_usec = 0;

            ready = select(sock + 1, &rfds, NULL, NULL, &tv);
            if (ready < 0) {
                if (errno == EINTR) {
                    if (reload_requested) {
                        reload_requested = 0;
                        reload_discovery();
                    }
                    continue;
                }
                log_error("select failed: %s", strerror(errno));
                break;
            }
            if (ready == 0) {
                if (reload_requested) {
                    reload_requested = 0;
                    reload_discovery();
                }
                continue;
            }

            if (FD_ISSET(sock, &rfds)) {
                struct sockaddr_storage sender;
                socklen_t sender_len = sizeof(sender);
                ssize_t received;

                memset(recv_buffer, 0, sizeof(recv_buffer));
                received = recvfrom(sock, recv_buffer, sizeof(recv_buffer) - 1, 0, (struct sockaddr *) &sender, &sender_len);
                if (received > 0) {
                    recv_buffer[received] = '\0';
                    process_message(recv_buffer, &sender, sender_len);
                }
            }
        } else {
            pause();
        }

        if (reload_requested) {
            reload_requested = 0;
            reload_discovery();
        }
    }

    if (!non_discoverable && sock >= 0) {
        log_info("Sending Bye before exit");
        send_bye_all();
    }
    close_socket();

    release_pid_file(pid_file);
    log_info("Terminating program");

    return 0;
}
