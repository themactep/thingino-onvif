/*
 * Copyright (c) 2024 roleo.
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

#include "conf.h"

#include <json-c/json.h>
#include "log.h"
#include "onvif_simple_server.h"
#include "utils.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

extern service_context_t service_ctx;

// Remember to free the string
void get_string_from_json(char** var, json_object* j, char* name)
{
    json_object* s = NULL;

    if (json_object_object_get_ex(j, name, &s)) {
        if (json_object_is_type(s, json_type_string)) {
            const char* str_val = json_object_get_string(s);
            if (str_val != NULL) {
                *var = (char*) malloc((strlen(str_val) + 1) * sizeof(char));
                strcpy(*var, str_val);
            }
        }
    }
}

void get_int_from_json(int* var, json_object* j, char* name)
{
    json_object* i = NULL;

    if (json_object_object_get_ex(j, name, &i)) {
        if (json_object_is_type(i, json_type_int)) {
            *var = json_object_get_int(i);
        }
    }
}

void get_double_from_json(double* var, json_object* j, char* name)
{
    json_object* d = NULL;

    if (json_object_object_get_ex(j, name, &d)) {
        if (json_object_is_type(d, json_type_double)) {
            *var = json_object_get_double(d);
        }
    }
}
static json_object* load_json_file(const char* path)
{
    FILE* f = fopen(path, "r");
    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    char* buf = (char*) malloc((size_t) sz + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t rd = fread(buf, 1, (size_t) sz, f);
    fclose(f);
    if (rd != (size_t) sz) {
        free(buf);
        return NULL;
    }
    buf[sz] = '\0';
    json_object* j = json_tokener_parse(buf);
    free(buf);
    return j;
}

static void load_profiles_from_dir(const char* dir)
{
    char p[PATH_MAX];
    snprintf(p, sizeof(p), "%s/%s", dir, "profiles.json");
    json_object* value = load_json_file(p);
    if (!value) {
        log_debug("profiles.json not found in %s", dir);
        return;
    }
    if (!json_object_is_type(value, json_type_object)) {
        json_object_put(value);
        return;
    }
    json_object_object_foreach(value, key, item) {
        service_ctx.profiles_num++;
        service_ctx.profiles = (stream_profile_t*) realloc(service_ctx.profiles, service_ctx.profiles_num * sizeof(stream_profile_t));
        // Init defaults
        service_ctx.profiles[service_ctx.profiles_num - 1].name = NULL;
        service_ctx.profiles[service_ctx.profiles_num - 1].width = 0;
        service_ctx.profiles[service_ctx.profiles_num - 1].height = 0;
        service_ctx.profiles[service_ctx.profiles_num - 1].url = NULL;
        service_ctx.profiles[service_ctx.profiles_num - 1].snapurl = NULL;
        service_ctx.profiles[service_ctx.profiles_num - 1].type = H264;
        service_ctx.profiles[service_ctx.profiles_num - 1].audio_encoder = AAC;
        service_ctx.profiles[service_ctx.profiles_num - 1].audio_decoder = AUDIO_NONE;
        get_string_from_json(&(service_ctx.profiles[service_ctx.profiles_num - 1].name), item, "name");
        get_int_from_json(&(service_ctx.profiles[service_ctx.profiles_num - 1].width), item, "width");
        get_int_from_json(&(service_ctx.profiles[service_ctx.profiles_num - 1].height), item, "height");
        get_string_from_json(&(service_ctx.profiles[service_ctx.profiles_num - 1].url), item, "url");
        get_string_from_json(&(service_ctx.profiles[service_ctx.profiles_num - 1].snapurl), item, "snapurl");
        char* tmp = NULL;
        get_string_from_json(&tmp, item, "type");
        if (tmp) {
            if (!strcasecmp(tmp, "JPEG"))
                service_ctx.profiles[service_ctx.profiles_num - 1].type = JPEG;
            else if (!strcasecmp(tmp, "MPEG4"))
                service_ctx.profiles[service_ctx.profiles_num - 1].type = MPEG4;
            else if (!strcasecmp(tmp, "H264"))
                service_ctx.profiles[service_ctx.profiles_num - 1].type = H264;
            else if (!strcasecmp(tmp, "H265"))
                service_ctx.profiles[service_ctx.profiles_num - 1].type = H265;
            free(tmp);
        }
        tmp = NULL;
        get_string_from_json(&tmp, item, "audio_encoder");
        if (tmp) {
            if (!strcasecmp(tmp, "NONE"))
                service_ctx.profiles[service_ctx.profiles_num - 1].audio_encoder = AUDIO_NONE;
            else if (!strcasecmp(tmp, "G711"))
                service_ctx.profiles[service_ctx.profiles_num - 1].audio_encoder = G711;
            else if (!strcasecmp(tmp, "G726"))
                service_ctx.profiles[service_ctx.profiles_num - 1].audio_encoder = G726;
            else if (!strcasecmp(tmp, "AAC"))
                service_ctx.profiles[service_ctx.profiles_num - 1].audio_encoder = AAC;
            free(tmp);
        }
        tmp = NULL;
        get_string_from_json(&tmp, item, "audio_decoder");
        if (tmp) {
            if (!strcasecmp(tmp, "NONE"))
                service_ctx.profiles[service_ctx.profiles_num - 1].audio_decoder = AUDIO_NONE;
            else if (!strcasecmp(tmp, "G711"))
                service_ctx.profiles[service_ctx.profiles_num - 1].audio_decoder = G711;
            else if (!strcasecmp(tmp, "G726"))
                service_ctx.profiles[service_ctx.profiles_num - 1].audio_decoder = G726;
            else if (!strcasecmp(tmp, "AAC"))
                service_ctx.profiles[service_ctx.profiles_num - 1].audio_decoder = AAC;
            free(tmp);
        }
        log_debug("Profile %d: %s %dx%d",
                  service_ctx.profiles_num - 1,
                  service_ctx.profiles[service_ctx.profiles_num - 1].name,
                  service_ctx.profiles[service_ctx.profiles_num - 1].width,
                  service_ctx.profiles[service_ctx.profiles_num - 1].height);
    }
    json_object_put(value);
}

static void load_ptz_from_dir(const char* dir)
{
    char p[PATH_MAX];
    snprintf(p, sizeof(p), "%s/%s", dir, "ptz.json");
    json_object* value = load_json_file(p);
    if (!value) {
        log_debug("ptz.json not found in %s", dir);
        return;
    }
    get_int_from_json(&(service_ctx.ptz_node.enable), value, "enable");
    get_double_from_json(&(service_ctx.ptz_node.min_step_x), value, "min_step_x");
    get_double_from_json(&(service_ctx.ptz_node.max_step_x), value, "max_step_x");
    get_double_from_json(&(service_ctx.ptz_node.min_step_y), value, "min_step_y");
    get_double_from_json(&(service_ctx.ptz_node.max_step_y), value, "max_step_y");
    get_double_from_json(&(service_ctx.ptz_node.min_step_z), value, "min_step_z");
    get_double_from_json(&(service_ctx.ptz_node.max_step_z), value, "max_step_z");
    get_string_from_json(&(service_ctx.ptz_node.get_position), value, "get_position");
    get_string_from_json(&(service_ctx.ptz_node.is_moving), value, "is_moving");
    get_string_from_json(&(service_ctx.ptz_node.move_left), value, "move_left");
    get_string_from_json(&(service_ctx.ptz_node.move_right), value, "move_right");
    get_string_from_json(&(service_ctx.ptz_node.move_up), value, "move_up");
    get_string_from_json(&(service_ctx.ptz_node.move_down), value, "move_down");
    get_string_from_json(&(service_ctx.ptz_node.move_in), value, "move_in");
    get_string_from_json(&(service_ctx.ptz_node.move_out), value, "move_out");
    get_string_from_json(&(service_ctx.ptz_node.move_stop), value, "move_stop");
    get_string_from_json(&(service_ctx.ptz_node.move_preset), value, "move_preset");
    get_string_from_json(&(service_ctx.ptz_node.goto_home_position), value, "goto_home_position");
    get_string_from_json(&(service_ctx.ptz_node.set_preset), value, "set_preset");
    get_string_from_json(&(service_ctx.ptz_node.set_home_position), value, "set_home_position");
    get_string_from_json(&(service_ctx.ptz_node.remove_preset), value, "remove_preset");
    get_string_from_json(&(service_ctx.ptz_node.jump_to_abs), value, "jump_to_abs");
    get_string_from_json(&(service_ctx.ptz_node.jump_to_rel), value, "jump_to_rel");
    get_string_from_json(&(service_ctx.ptz_node.get_presets), value, "get_presets");
    json_object_put(value);
}

static void load_relays_from_dir(const char* dir)
{
    char p[PATH_MAX];
    snprintf(p, sizeof(p), "%s/%s", dir, "relays.json");
    log_debug("DEBUG: Attempting to load relays from: %s", p);
    json_object* value = load_json_file(p);
    if (!value) {
        log_debug("relays.json not found in %s", dir);
        return;
    }

    log_debug("DEBUG: relays.json successfully loaded from %s", dir);
    if (!json_object_is_type(value, json_type_array)) {
        log_error("DEBUG: relays.json is not a JSON array");
        json_object_put(value);
        return;
    }

    json_object* item;
    size_t array_len = json_object_array_length(value);
    log_debug("DEBUG: Found %zu relay entries in relays.json", array_len);
    for (size_t i = 0; i < array_len; i++) {
        item = json_object_array_get_idx(value, i);
        if (!item) continue;

        service_ctx.relay_outputs_num++;
        log_debug("DEBUG: Processing relay %zu, relay_outputs_num now: %d", i, service_ctx.relay_outputs_num);
        if (service_ctx.relay_outputs_num >= MAX_RELAY_OUTPUTS) {
            log_error("Too many relay outputs, max is: %d", MAX_RELAY_OUTPUTS);
            service_ctx.relay_outputs_num--;
            continue;
        }

        service_ctx.relay_outputs = (relay_output_t*) realloc(service_ctx.relay_outputs, service_ctx.relay_outputs_num * sizeof(relay_output_t));
        service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].idle_state = IDLE_STATE_CLOSE;
        service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].close = NULL;
        service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].open = NULL;
        char* tmp = NULL;
        get_string_from_json(&tmp, item, "idle_state");
        if (tmp) {
            if (!strcasecmp(tmp, "open"))
                service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].idle_state = IDLE_STATE_OPEN;
            free(tmp);
        }
        get_string_from_json(&(service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].close), item, "close");
        get_string_from_json(&(service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].open), item, "open");
        log_debug("DEBUG: Relay %d configured - close: %s, open: %s",
                  service_ctx.relay_outputs_num - 1,
                  service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].close ? service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].close : "(null)",
                  service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].open ? service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].open : "(null)");
    }
    log_debug("DEBUG: Finished loading relays. Total relay_outputs_num: %d", service_ctx.relay_outputs_num);
    json_object_put(value);
}

static void load_events_from_dir(const char* dir)
{
    char p[PATH_MAX];
    snprintf(p, sizeof(p), "%s/%s", dir, "events.json");
    json_object* root = load_json_file(p);

    if (!root) {
        log_debug("events.json not found in %s", dir);
        return;
    }
    get_int_from_json(&(service_ctx.events_enable), root, "enable");
    json_object* value = NULL;
    json_object_object_get_ex(root, "events", &value);
    if (value && json_object_is_type(value, json_type_array)) {
        json_object* item;
        size_t array_len = json_object_array_length(value);
        for (size_t i = 0; i < array_len; i++) {
            item = json_object_array_get_idx(value, i);
            if (!item) continue;

            service_ctx.events_num++;
            if (service_ctx.events_num > MAX_EVENTS) {
                log_error("Too many events, max is: %d", MAX_EVENTS);
                service_ctx.events_num--;
                break;
            }
            service_ctx.events = (event_t*) realloc(service_ctx.events, service_ctx.events_num * sizeof(event_t));
            service_ctx.events[service_ctx.events_num - 1].topic = NULL;
            service_ctx.events[service_ctx.events_num - 1].source_name = NULL;
            service_ctx.events[service_ctx.events_num - 1].source_type = NULL;
            service_ctx.events[service_ctx.events_num - 1].source_value = NULL;
            service_ctx.events[service_ctx.events_num - 1].input_file = NULL;
            get_string_from_json(&(service_ctx.events[service_ctx.events_num - 1].topic), item, "topic");
            get_string_from_json(&(service_ctx.events[service_ctx.events_num - 1].source_name), item, "source_name");
            get_string_from_json(&(service_ctx.events[service_ctx.events_num - 1].source_type), item, "source_type");
            get_string_from_json(&(service_ctx.events[service_ctx.events_num - 1].source_value), item, "source_value");
            get_string_from_json(&(service_ctx.events[service_ctx.events_num - 1].input_file), item, "input_file");
        }
    }
    json_object_put(root);
}

void get_bool_from_json(int* var, json_object* j, char* name)
{
    json_object* b = NULL;

    if (json_object_object_get_ex(j, name, &b)) {
        if (json_object_is_type(b, json_type_boolean)) {
            *var = json_object_get_boolean(b) ? 1 : 0;
        } else {
            *var = 0;
        }
    } else {
        *var = 0;
    }
}

int process_json_conf_file(char* file)
{
    FILE* fF;
    json_object *value, *item;
    char *buffer, *tmp;
    json_object* json_file;

    int i, json_size;
    char stmp[MAX_LEN];

    fF = fopen(file, "r");
    if (fF == NULL)
        return -1;

    fseek(fF, 0L, SEEK_END);
    json_size = ftell(fF);
    fseek(fF, 0L, SEEK_SET);

    buffer = (char*) malloc((json_size + 1) * sizeof(char));
    if (fread(buffer, 1, json_size, fF) != json_size) {
        fclose(fF);
        return -1;
    }
    fclose(fF);

    json_file = json_tokener_parse(buffer);
    if (json_file == NULL) {
        log_error("Failed to parse JSON configuration file");
        free(buffer);
        return -2;
    }

    // Init variables before reading
    service_ctx.port = 80;
    service_ctx.username = NULL;
    service_ctx.password = NULL;
    service_ctx.manufacturer = NULL;
    service_ctx.model = NULL;
    service_ctx.firmware_ver = NULL;
    service_ctx.serial_num = NULL;
    service_ctx.hardware_id = NULL;
    service_ctx.ifs = NULL;
    service_ctx.adv_enable_media2 = 0;
    service_ctx.adv_fault_if_unknown = 0;
    service_ctx.adv_fault_if_set = 0;
    service_ctx.adv_synology_nvr = 0;
    service_ctx.profiles = NULL;
    service_ctx.profiles_num = 0;
    service_ctx.scopes = NULL;
    service_ctx.scopes_num = 0;
    service_ctx.ptz_node.enable = 0;
    service_ctx.relay_outputs = NULL;
    service_ctx.relay_outputs_num = 0;
    service_ctx.events = NULL;
    service_ctx.events_enable = EVENTS_NONE;
    service_ctx.events_num = 0;
    service_ctx.loglevel = 0;
    service_ctx.raw_xml_log_file = NULL;

    service_ctx.ptz_node.min_step_x = 0;
    service_ctx.ptz_node.max_step_x = 360.0;
    service_ctx.ptz_node.min_step_y = 0;
    service_ctx.ptz_node.max_step_y = 180.0;
    service_ctx.ptz_node.min_step_z = 0.0;
    service_ctx.ptz_node.max_step_z = 0.0;
    service_ctx.ptz_node.get_position = NULL;
    service_ctx.ptz_node.is_moving = NULL;
    service_ctx.ptz_node.move_left = NULL;
    service_ctx.ptz_node.move_right = NULL;
    service_ctx.ptz_node.move_up = NULL;
    service_ctx.ptz_node.move_down = NULL;
    service_ctx.ptz_node.move_in = NULL;
    service_ctx.ptz_node.move_out = NULL;
    service_ctx.ptz_node.move_stop = NULL;
    service_ctx.ptz_node.move_preset = NULL;
    service_ctx.ptz_node.goto_home_position = NULL;
    service_ctx.ptz_node.set_preset = NULL;
    service_ctx.ptz_node.set_home_position = NULL;
    service_ctx.ptz_node.remove_preset = NULL;
    service_ctx.ptz_node.jump_to_abs = NULL;
    service_ctx.ptz_node.jump_to_rel = NULL;

    get_string_from_json(&(service_ctx.model), json_file, "model");
    get_string_from_json(&(service_ctx.manufacturer), json_file, "manufacturer");
    get_string_from_json(&(service_ctx.firmware_ver), json_file, "firmware_ver");
    get_string_from_json(&(service_ctx.hardware_id), json_file, "hardware_id");
    get_string_from_json(&(service_ctx.serial_num), json_file, "serial_num");
    get_string_from_json(&(service_ctx.ifs), json_file, "ifs");
    get_int_from_json(&(service_ctx.port), json_file, "port");
    get_int_from_json(&(service_ctx.loglevel), json_file, "loglevel");
    get_string_from_json(&(service_ctx.raw_xml_log_file), json_file, "raw_xml_log_file");
    if (service_ctx.raw_xml_log_file == NULL) {
        const char* defpath = "/tmp/onvif_raw.log";
        service_ctx.raw_xml_log_file = (char*) malloc(strlen(defpath) + 1);
        if (service_ctx.raw_xml_log_file)
            strcpy(service_ctx.raw_xml_log_file, defpath);
    }

    if (json_object_object_get_ex(json_file, "scopes", &value)) {
        if (json_object_is_type(value, json_type_array)) {
            size_t array_len = json_object_array_length(value);
            for (size_t i = 0; i < array_len; i++) {
                item = json_object_array_get_idx(value, i);
                if (item && json_object_is_type(item, json_type_string)) {
                    service_ctx.scopes_num++;
                    service_ctx.scopes = (char**) realloc(service_ctx.scopes, service_ctx.scopes_num * sizeof(char*));
                    const char* str_val = json_object_get_string(item);
                    service_ctx.scopes[service_ctx.scopes_num - 1] = (char*) malloc((strlen(str_val) + 1) * sizeof(char));
                    strcpy(service_ctx.scopes[service_ctx.scopes_num - 1], str_val);
                }
            }
        }
    }
    get_string_from_json(&(service_ctx.username), json_file, "username");
    get_string_from_json(&(service_ctx.password), json_file, "password");
    get_bool_from_json(&(service_ctx.adv_enable_media2), json_file, "adv_enable_media2");
    get_bool_from_json(&(service_ctx.adv_fault_if_unknown), json_file, "adv_fault_if_unknown");
    get_bool_from_json(&(service_ctx.adv_fault_if_set), json_file, "adv_fault_if_set");
    get_bool_from_json(&(service_ctx.adv_synology_nvr), json_file, "adv_synology_nvr");

    // Apply sane defaults for required fields if not provided in config
    if (service_ctx.manufacturer == NULL) {
        service_ctx.manufacturer = (char*) malloc(strlen(DEFAULT_MANUFACTURER) + 1);
        if (service_ctx.manufacturer) strcpy(service_ctx.manufacturer, DEFAULT_MANUFACTURER);
    }
    if (service_ctx.model == NULL) {
        service_ctx.model = (char*) malloc(strlen(DEFAULT_MODEL) + 1);
        if (service_ctx.model) strcpy(service_ctx.model, DEFAULT_MODEL);
    }
    if (service_ctx.firmware_ver == NULL) {
        service_ctx.firmware_ver = (char*) malloc(strlen(DEFAULT_FW_VER) + 1);
        if (service_ctx.firmware_ver) strcpy(service_ctx.firmware_ver, DEFAULT_FW_VER);
    }
    if (service_ctx.serial_num == NULL) {
        service_ctx.serial_num = (char*) malloc(strlen(DEFAULT_SERIAL_NUM) + 1);
        if (service_ctx.serial_num) strcpy(service_ctx.serial_num, DEFAULT_SERIAL_NUM);
    }
    if (service_ctx.hardware_id == NULL) {
        service_ctx.hardware_id = (char*) malloc(strlen(DEFAULT_HW_ID) + 1);
        if (service_ctx.hardware_id) strcpy(service_ctx.hardware_id, DEFAULT_HW_ID);
    }
    if (service_ctx.ifs == NULL) {
        service_ctx.ifs = (char*) malloc(strlen(DEFAULT_IFS) + 1);
        if (service_ctx.ifs) strcpy(service_ctx.ifs, DEFAULT_IFS);
    }

    // Print debug
    log_debug("model: %s", service_ctx.model);
    log_debug("manufacturer: %s", service_ctx.manufacturer);
    log_debug("firmware_ver: %s", service_ctx.firmware_ver);
    log_debug("hardware_id: %s", service_ctx.hardware_id);
    log_debug("serial_num: %s", service_ctx.serial_num);
    log_debug("ifs: %s", service_ctx.ifs);
    log_debug("port: %d", service_ctx.port);
    log_debug("scopes:");
    for (i = 0; i < service_ctx.scopes_num; i++) {
        log_debug("\t%s", service_ctx.scopes[i]);
    }
    if (service_ctx.username != NULL) {
        log_debug("username: %s", service_ctx.username);
        log_debug("password: %s", service_ctx.password ? service_ctx.password : "(null)");
    }
    // Modular config: load additional configuration from conf_dir (default /etc/onvif.d)
    char* conf_dir = NULL;
    get_string_from_json(&conf_dir, json_file, "conf_dir");
    if (conf_dir == NULL) {
        const char* defdir = DEFAULT_CONF_DIR;
        conf_dir = (char*) malloc(strlen(defdir) + 1);
        if (conf_dir)
            strcpy(conf_dir, defdir);
    }
    if (conf_dir && conf_dir[0] != '\0') {
        log_info("Loading modular configuration from %s", conf_dir);
        load_profiles_from_dir(conf_dir);
        load_ptz_from_dir(conf_dir);
        load_relays_from_dir(conf_dir);
        load_events_from_dir(conf_dir);
    }
    if (conf_dir)
        free(conf_dir);

    log_debug("adv_enable_media2: %d", service_ctx.adv_enable_media2);
    log_debug("adv_fault_if_unknown: %d", service_ctx.adv_fault_if_unknown);
    log_debug("adv_fault_if_set: %d", service_ctx.adv_fault_if_set);
    log_debug("adv_synology_nvr: %d", service_ctx.adv_synology_nvr);
    log_debug("raw_xml_log_file: %s", service_ctx.raw_xml_log_file ? service_ctx.raw_xml_log_file : "(null)");
    log_debug("");

    if (service_ctx.relay_outputs != NULL) {
        if (service_ctx.events_enable == EVENTS_NONE)
            service_ctx.events_enable = EVENTS_PULLPOINT;

        for (i = 0; i < service_ctx.relay_outputs_num; i++) {
            service_ctx.events_num++;
            if (service_ctx.events_num > MAX_EVENTS) {
                log_error("Unable to add relay event, too many events, max is: %d", MAX_EVENTS);
                return -2;
            }
            log_debug("Adding event for relay output %d", i);
            service_ctx.events = (event_t*) realloc(service_ctx.events, service_ctx.events_num * sizeof(event_t));
            service_ctx.events[service_ctx.events_num - 1].topic = (char*) malloc(strlen("tns1:Device/Trigger/Relay") + 1);
            strcpy(service_ctx.events[service_ctx.events_num - 1].topic, "tns1:Device/Trigger/Relay");
            log_debug("topic: tns1:Device/Trigger/Relay");
            service_ctx.events[service_ctx.events_num - 1].source_name = (char*) malloc(strlen("RelayToken") + 1);
            strcpy(service_ctx.events[service_ctx.events_num - 1].source_name, "RelayToken");
            log_debug("source_name: RelayToken");
            service_ctx.events[service_ctx.events_num - 1].source_type = (char*) malloc(strlen("tt:ReferenceToken") + 1);
            strcpy(service_ctx.events[service_ctx.events_num - 1].source_type, "tt:ReferenceToken");
            log_debug("source_type: tt:ReferenceToken");
            sprintf(stmp, "RelayOutputToken_%d", i);
            service_ctx.events[service_ctx.events_num - 1].source_value = (char*) malloc(strlen(stmp) + 1);
            strcpy(service_ctx.events[service_ctx.events_num - 1].source_value, stmp);
            log_debug("source_value: %s", stmp);
            sprintf(stmp, "/tmp/onvif_notify_server/relay_output_%d", i);
            service_ctx.events[service_ctx.events_num - 1].input_file = (char*) malloc(strlen(stmp) + 1);
            strcpy(service_ctx.events[service_ctx.events_num - 1].input_file, stmp);
            log_debug("input_file: %s", stmp);
        }
    }

    json_object_put(json_file);
    free(buffer);

    return 0;
}

void free_conf_file()
{
    int i;

    if (service_ctx.events_enable == 1) {
        for (i = service_ctx.events_num - 1; i >= 0; i--) {
            if (service_ctx.events[i].input_file != NULL)
                free(service_ctx.events[i].input_file);
            if (service_ctx.events[i].source_value != NULL)
                free(service_ctx.events[i].source_value);
            if (service_ctx.events[i].source_type != NULL)
                free(service_ctx.events[i].source_type);
            if (service_ctx.events[i].source_name != NULL)
                free(service_ctx.events[i].source_name);
            if (service_ctx.events[i].topic != NULL)
                free(service_ctx.events[i].topic);
        }
    }

    if (service_ctx.ptz_node.enable == 1) {
        if (service_ctx.ptz_node.jump_to_rel != NULL)
            free(service_ctx.ptz_node.jump_to_rel);
        if (service_ctx.ptz_node.jump_to_abs != NULL)
            free(service_ctx.ptz_node.jump_to_abs);
        if (service_ctx.ptz_node.remove_preset != NULL)
            free(service_ctx.ptz_node.remove_preset);
        if (service_ctx.ptz_node.set_home_position != NULL)
            free(service_ctx.ptz_node.set_home_position);
        if (service_ctx.ptz_node.set_preset != NULL)
            free(service_ctx.ptz_node.set_preset);
        if (service_ctx.ptz_node.goto_home_position != NULL)
            free(service_ctx.ptz_node.goto_home_position);
        if (service_ctx.ptz_node.move_preset != NULL)
            free(service_ctx.ptz_node.move_preset);
        if (service_ctx.ptz_node.move_stop != NULL)
            free(service_ctx.ptz_node.move_stop);
        if (service_ctx.ptz_node.move_out != NULL)
            free(service_ctx.ptz_node.move_out);
        if (service_ctx.ptz_node.move_in != NULL)
            free(service_ctx.ptz_node.move_in);
        if (service_ctx.ptz_node.move_down != NULL)
            free(service_ctx.ptz_node.move_down);
        if (service_ctx.ptz_node.move_up != NULL)
            free(service_ctx.ptz_node.move_up);
        if (service_ctx.ptz_node.move_right != NULL)
            free(service_ctx.ptz_node.move_right);
        if (service_ctx.ptz_node.move_left != NULL)
            free(service_ctx.ptz_node.move_left);
        if (service_ctx.ptz_node.is_moving != NULL)
            free(service_ctx.ptz_node.is_moving);
        if (service_ctx.ptz_node.get_position != NULL)
            free(service_ctx.ptz_node.get_position);
    }

    for (i = service_ctx.relay_outputs_num - 1; i >= 0; i--) {
        if (service_ctx.relay_outputs[i].open != NULL)
            free(service_ctx.relay_outputs[i].open);
        if (service_ctx.relay_outputs[i].close != NULL)
            free(service_ctx.relay_outputs[i].close);
    }
    if (service_ctx.relay_outputs != NULL)
        free(service_ctx.relay_outputs);

    for (i = service_ctx.profiles_num - 1; i >= 0; i--) {
        if (service_ctx.profiles[i].snapurl != NULL)
            free(service_ctx.profiles[i].snapurl);
        if (service_ctx.profiles[i].url != NULL)
            free(service_ctx.profiles[i].url);
        if (service_ctx.profiles[i].name != NULL)
            free(service_ctx.profiles[i].name);
    }
    if (service_ctx.profiles != NULL)
        free(service_ctx.profiles);

    for (i = service_ctx.scopes_num - 1; i >= 0; i--) {
        free(service_ctx.scopes[i]);
    }
    if (service_ctx.scopes != NULL)
        free(service_ctx.scopes);

    if (service_ctx.events != NULL)
        free(service_ctx.events);
    if (service_ctx.ifs != NULL)
        free(service_ctx.ifs);
    if (service_ctx.hardware_id != NULL)
        free(service_ctx.hardware_id);
    if (service_ctx.serial_num != NULL)
        free(service_ctx.serial_num);
    if (service_ctx.firmware_ver != NULL)
        free(service_ctx.firmware_ver);
    if (service_ctx.model != NULL)
        free(service_ctx.model);
    if (service_ctx.manufacturer != NULL)
        free(service_ctx.manufacturer);
    if (service_ctx.password != NULL)
        free(service_ctx.password);
    if (service_ctx.username != NULL)
        free(service_ctx.username);
    if (service_ctx.raw_xml_log_file != NULL)
        free(service_ctx.raw_xml_log_file);
}
