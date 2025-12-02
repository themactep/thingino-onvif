/*
 * Copyright (c) 2025 Thingino
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "xml_logger.h"

#include "log.h"
#include "onvif_simple_server.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

extern service_context_t service_ctx;

// Cached state to avoid repeated checks
static int xml_logging_enabled = -1;     // -1 = not initialized, 0 = disabled, 1 = enabled
static char current_timestamp[32] = {0}; // Shared timestamp for request/response pair

/**
 * Initialize XML logging system
 */
void xml_logger_init(void)
{
    xml_logging_enabled = -1; // Reset state
    current_timestamp[0] = '\0';
}

/**
 * Check if XML logging is enabled
 * @return 1 if enabled, 0 if disabled
 */
int xml_logger_is_enabled(void)
{
    // Check if already determined
    if (xml_logging_enabled != -1) {
        return xml_logging_enabled;
    }

    // Check if debug level is enabled (level 5 = DEBUG)
    if (service_ctx.loglevel < LOG_LVL_DEBUG) {
        log_debug("XML logging disabled: debug level not enabled (current: %d, required: %d)", service_ctx.loglevel, LOG_LVL_DEBUG);
        xml_logging_enabled = 0;
        return 0;
    }

    // Check if raw_log_directory is configured
    if (service_ctx.raw_log_directory == NULL || service_ctx.raw_log_directory[0] == '\0') {
        log_debug("XML logging disabled: log_directory not configured");
        xml_logging_enabled = 0;
        return 0;
    }

    // Check if directory exists and is writable
    struct stat st;
    if (stat(service_ctx.raw_log_directory, &st) != 0) {
        log_warn("XML logging disabled: log_directory '%s' does not exist: %s", service_ctx.raw_log_directory, strerror(errno));
        xml_logging_enabled = 0;
        return 0;
    }

    if (!S_ISDIR(st.st_mode)) {
        log_warn("XML logging disabled: log_directory '%s' is not a directory", service_ctx.raw_log_directory);
        xml_logging_enabled = 0;
        return 0;
    }

    // Check if directory is writable
    if (access(service_ctx.raw_log_directory, W_OK) != 0) {
        log_warn("XML logging disabled: log_directory '%s' is not writable: %s", service_ctx.raw_log_directory, strerror(errno));
        xml_logging_enabled = 0;
        return 0;
    }

    log_debug("XML logging enabled: log_directory='%s'", service_ctx.raw_log_directory);
    xml_logging_enabled = 1;
    return 1;
}
// ------------- Error destination readiness (external mount) -------------

static int is_external_mount_ready(const char *dir)
{
    if (!dir || !dir[0])
        return 0;

    struct stat st;
    if (stat(dir, &st) != 0) {
        return 0;
    }
    if (access(dir, W_OK) != 0) {
        return 0;
    }

    // Resolve to absolute path
    char resolved[PATH_MAX];
    if (!realpath(dir, resolved)) {
        return 0;
    }

    // Parse /proc/mounts and find the best (longest) matching mount point
    FILE *fp = fopen("/proc/mounts", "r");
    if (!fp)
        return 0;

    char dev[256], mnt[512], fstype[128], opts[256];
    int best_len = -1;
    char best_mnt[512] = {0};
    char best_fstype[128] = {0};

    while (fscanf(fp, "%255s %511s %127s %255s %*d %*d\n", dev, mnt, fstype, opts) == 4) {
        size_t mlen = strlen(mnt);
        if (mlen <= 0)
            continue;
        if (strncmp(resolved, mnt, mlen) == 0 && (resolved[mlen] == '\0' || resolved[mlen] == '/')) {
            if ((int) mlen > best_len) {
                best_len = (int) mlen;
                strncpy(best_mnt, mnt, sizeof(best_mnt) - 1);
                strncpy(best_fstype, fstype, sizeof(best_fstype) - 1);
            }
        }
    }
    fclose(fp);

    if (best_len < 0)
        return 0;

    // Disallow rootfs-like or in-memory/overlay filesystems
    if (strcmp(best_mnt, "/") == 0)
        return 0;

    const char *disallow[] = {"overlay", "tmpfs", "ramfs", "rootfs"};
    for (size_t i = 0; i < sizeof(disallow) / sizeof(disallow[0]); ++i) {
        if (strcmp(best_fstype, disallow[i]) == 0)
            return 0;
    }

    // Consider it external
    return 1;
}

int xml_error_log_destination_ready(int emit_warn)
{
    static time_t last_warn = 0;
    int ready = is_external_mount_ready(service_ctx.raw_log_directory);
    if (!ready && emit_warn) {
        time_t now = time(NULL);
        if (last_warn == 0 || (now - last_warn) >= 300) {
            last_warn = now;
            log_warn("XML error capture disabled: log_directory not ready or not external (dir='%s')",
                     service_ctx.raw_log_directory ? service_ctx.raw_log_directory : "");
        }
    }
    return ready;
}

/**
 * Generate timestamp for filename
 * Format: YYYYMMDD_HHMMSS
 * @param buffer Buffer to store timestamp (must be at least 16 bytes)
 */
static void generate_timestamp(char *buffer, size_t buffer_size)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, buffer_size, "%Y%m%d_%H%M%S", tm_info);
}

/**
 * Sanitize IP address for use in directory name
 * Replaces invalid characters with underscores
 * @param ip_addr The IP address to sanitize
 * @param buffer Buffer to store sanitized IP (must be at least as large as ip_addr)
 */
static void sanitize_ip_address(const char *ip_addr, char *buffer, size_t buffer_size)
{
    if (ip_addr == NULL || buffer == NULL || buffer_size == 0) {
        if (buffer && buffer_size > 0) {
            buffer[0] = '\0';
        }
        return;
    }

    size_t i;
    for (i = 0; i < buffer_size - 1 && ip_addr[i] != '\0'; i++) {
        char c = ip_addr[i];
        // Allow alphanumeric, dots, colons (for IPv6), and hyphens
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.' || c == ':' || c == '-') {
            buffer[i] = c;
        } else {
            buffer[i] = '_';
        }
    }
    buffer[i] = '\0';
}

/**
 * Create directory for IP address if it doesn't exist
 * @param base_dir Base directory path
 * @param ip_addr IP address for subdirectory
 * @param full_path Buffer to store full path (must be at least PATH_MAX bytes)
 * @return 0 on success, -1 on error
 */
static int create_ip_directory(const char *base_dir, const char *ip_addr, char *full_path, size_t path_size)
{
    char sanitized_ip[256];
    sanitize_ip_address(ip_addr, sanitized_ip, sizeof(sanitized_ip));

    // Build full path
    int ret = snprintf(full_path, path_size, "%s/%s", base_dir, sanitized_ip);
    if (ret < 0 || (size_t) ret >= path_size) {
        log_error("XML logging: path too long for IP directory");
        return -1;
    }

    // Check if directory exists
    struct stat st;
    if (stat(full_path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return 0; // Directory already exists
        } else {
            log_error("XML logging: path '%s' exists but is not a directory", full_path);
            return -1;
        }
    }

    // Create directory
    if (mkdir(full_path, 0755) != 0) {
        log_error("XML logging: failed to create directory '%s': %s", full_path, strerror(errno));
        return -1;
    }

    log_debug("XML logging: created directory '%s'", full_path);
    return 0;
}

/**
 * Write XML content to file
 * @param filepath Full path to the file
 * @param xml_content XML content to write
 * @param xml_size Size of XML content
 * @return 0 on success, -1 on error
 */
static int write_xml_file(const char *filepath, const char *xml_content, int xml_size)
{
    FILE *fp = fopen(filepath, "w");
    if (fp == NULL) {
        log_error("XML logging: failed to open file '%s' for writing: %s", filepath, strerror(errno));
        return -1;
    }

    size_t written = fwrite(xml_content, 1, xml_size, fp);
    if (written != (size_t) xml_size) {
        log_error("XML logging: failed to write complete XML to '%s': wrote %zu of %d bytes", filepath, written, xml_size);
        fclose(fp);
        return -1;
    }

    if (fclose(fp) != 0) {
        log_error("XML logging: failed to close file '%s': %s", filepath, strerror(errno));
        return -1;
    }

    log_debug("XML logging: wrote %d bytes to '%s'", xml_size, filepath);
    return 0;
}

/**
 * Log raw XML request to external storage
 */
int log_xml_request(const char *xml_content, int xml_size, const char *remote_addr)
{
    // Check if logging is enabled
    if (!xml_logger_is_enabled()) {
        return 0; // Not an error, just disabled
    }

    // Validate inputs
    if (xml_content == NULL || xml_size <= 0) {
        log_debug("XML logging: skipping request log (empty content)");
        return 0;
    }

    if (remote_addr == NULL || remote_addr[0] == '\0') {
        remote_addr = "unknown";
    }

    // Generate timestamp for this request/response pair
    generate_timestamp(current_timestamp, sizeof(current_timestamp));

    // Create IP-specific directory
    char ip_dir[1024];
    if (create_ip_directory(service_ctx.raw_log_directory, remote_addr, ip_dir, sizeof(ip_dir)) != 0) {
        log_warn("XML logging: failed to create IP directory, skipping request log");
        return -1;
    }

    // Build filename
    char filepath[1280];
    int ret = snprintf(filepath, sizeof(filepath), "%s/%s_request.xml", ip_dir, current_timestamp);
    if (ret < 0 || (size_t) ret >= sizeof(filepath)) {
        log_error("XML logging: filepath too long for request");
        return -1;
    }

    // Write XML to file
    return write_xml_file(filepath, xml_content, xml_size);
}

/**
 * Log raw XML response to external storage
 */
int log_xml_response(const char *xml_content, int xml_size, const char *remote_addr)
{
    // Check if logging is enabled
    if (!xml_logger_is_enabled()) {
        return 0; // Not an error, just disabled
    }

    // Validate inputs
    if (xml_content == NULL || xml_size <= 0) {
        log_debug("XML logging: skipping response log (empty content)");
        return 0;
    }

    if (remote_addr == NULL || remote_addr[0] == '\0') {
        remote_addr = "unknown";
    }

    // Use the timestamp from the request (if available)
    if (current_timestamp[0] == '\0') {
        // No request was logged, generate new timestamp
        generate_timestamp(current_timestamp, sizeof(current_timestamp));
    }

    // Create IP-specific directory
    char ip_dir[1024];
    if (create_ip_directory(service_ctx.raw_log_directory, remote_addr, ip_dir, sizeof(ip_dir)) != 0) {
        log_warn("XML logging: failed to create IP directory, skipping response log");
        return -1;
    }

    // Build filename
    char filepath[1280];
    int ret = snprintf(filepath, sizeof(filepath), "%s/%s_response.xml", ip_dir, current_timestamp);
    if (ret < 0 || (size_t) ret >= sizeof(filepath)) {
        log_error("XML logging: filepath too long for response");
        return -1;
    }

    // Write XML to file
    return write_xml_file(filepath, xml_content, xml_size);
}

// --- Error-only logging with fallbacks ---

#define MAX_ERROR_XML_SIZE (2 * 1024 * 1024)

static void utc_iso8601(char *buf, size_t n)
{
    time_t now = time(NULL);
    struct tm tm_utc;
#if defined(_POSIX_THREAD_SAFE_FUNCTIONS)
    gmtime_r(&now, &tm_utc);
#else
    struct tm *tmp = gmtime(&now);
    if (tmp)
        tm_utc = *tmp;
    else
        memset(&tm_utc, 0, sizeof(tm_utc));
#endif
    strftime(buf, n, "%Y%m%dT%H%M%SZ", &tm_utc);
}

static int ensure_dir_exists(const char *dir)
{
    struct stat st;
    if (stat(dir, &st) == 0) {
        return S_ISDIR(st.st_mode) ? 0 : -1;
    }
    if (mkdir(dir, 0755) != 0) {
        return -1;
    }
    return 0;
}

int log_xml_error_request(const char *xml_content,
                          int xml_size,
                          const char *client_ip,
                          const char *service,
                          const char *method,
                          const char *reason,
                          const char *request_uri,
                          const char *query_string)
{
    // Only write when destination is ready per external mount policy
    if (!xml_error_log_destination_ready(1)) {
        return 0;
    }

    // Validate inputs
    if (xml_content == NULL || xml_size <= 0) {
        log_warn("XML error logging: no request body available, skipping file save");
        return 0;
    }

    const char *ip = (client_ip && client_ip[0]) ? client_ip : "unknown";

    // Determine base directory (must be configured)
    const char *base_dir = service_ctx.raw_log_directory;
    if (!base_dir || !base_dir[0]) {
        return 0;
    }

    // Build filename
    char ts[32];
    utc_iso8601(ts, sizeof(ts));

    char filepath[1536];
    int ret = snprintf(filepath,
                       sizeof(filepath),
                       "%s/%s_client-%s_svc-%s_method-%s_error.xml",
                       base_dir,
                       ts,
                       ip,
                       service ? service : "unknown",
                       method ? method : "unknown");
    if (ret < 0 || (size_t) ret >= sizeof(filepath)) {
        log_warn("XML error logging: filepath too long, skipping");
        return 0;
    }

    // Ensure uniqueness under concurrency: append pid and counter on collision
    pid_t pid = getpid();
    int attempt = 0;
    char unique_path[1700];
    while (attempt < 100) {
        if (attempt == 0) {
            strncpy(unique_path, filepath, sizeof(unique_path) - 1);
            unique_path[sizeof(unique_path) - 1] = '\0';
        } else {
            snprintf(unique_path,
                     sizeof(unique_path),
                     "%s/%s_client-%s_svc-%s_method-%s_error_pid-%d_%02d.xml",
                     base_dir,
                     ts,
                     ip,
                     service ? service : "unknown",
                     method ? method : "unknown",
                     (int) pid,
                     attempt);
        }
        if (access(unique_path, F_OK) != 0) {
            break; // available
        }
        attempt++;
    }

    // Truncate if needed
    int write_size = (xml_size > MAX_ERROR_XML_SIZE) ? MAX_ERROR_XML_SIZE : xml_size;

    // Write to file
    int rc = -1;
    FILE *fp = fopen(unique_path, "w");
    if (!fp) {
        log_warn(
            "Malformed %s.%s: reason='%s', client=%s, URI='%s', QUERY_STRING='%s' 			 		 			 		 	 	 		 		 		 		 (failed to open file)",
            service ? service : "service",
            method ? method : "method",
            reason ? reason : "(none)",
            ip,
            request_uri ? request_uri : "",
            query_string ? query_string : "");
        return 0;
    }

    size_t w = fwrite(xml_content, 1, (size_t) write_size, fp);
    if (w != (size_t) write_size) {
        // continue anyway
    }
    if (xml_size > MAX_ERROR_XML_SIZE) {
        const char *tr = "\n[TRUNCATED]\n";
        fwrite(tr, 1, strlen(tr), fp);
    }
    fclose(fp);

    // One-line error referencing saved file
    log_error("Malformed %s.%s: reason='%s', client=%s, URI='%s', QUERY_STRING='%s' -> raw XML: %s",
              service ? service : "service",
              method ? method : "method",
              reason ? reason : "(none)",
              ip,
              request_uri ? request_uri : "",
              query_string ? query_string : "",
              unique_path);

    // Optional debug with first 120 chars
    char preview[121];
    size_t preview_len = (size_t) ((write_size < 120) ? write_size : 120);
    memcpy(preview, xml_content, preview_len);
    preview[preview_len] = '\0';
    log_debug("XML error preview: %s", preview);
    rc = 0;
    return rc;
}
