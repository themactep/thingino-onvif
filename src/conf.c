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

#include "log.h"
#include "onvif_simple_server.h"
#include "utils.h"

#include <errno.h>
#include <fcntl.h>
#include <json_config.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

extern service_context_t service_ctx;

static void get_string_from_json(char **var, JsonValue *j, const char *name);

static char *dup_cstring(const char *src)
{
    if (!src)
        return NULL;
    size_t len = strlen(src) + 1;
    char *copy = (char *) malloc(len);
    if (copy)
        memcpy(copy, src, len);
    return copy;
}

static void free_imaging_string_list(imaging_string_list_t *list)
{
    if (!list || list->count == 0 || list->items == NULL)
        return;
    for (int i = 0; i < list->count; i++) {
        free(list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
}

static void parse_string_array(JsonValue *array, imaging_string_list_t *list)
{
    if (!array || !list || array->type != JSON_ARRAY)
        return;

    int array_len = get_array_size(array);
    if (array_len <= 0)
        return;

    char **items = (char **) calloc((size_t) array_len, sizeof(char *));
    if (!items)
        return;

    int copied = 0;
    for (int i = 0; i < array_len; i++) {
        JsonValue *node = get_array_item(array, i);
        if (!node || node->type != JSON_STRING || node->value.string == NULL)
            continue;
        char *dup = dup_cstring(node->value.string);
        if (!dup)
            continue;
        items[copied++] = dup;
    }

    if (copied == 0) {
        free(items);
        return;
    }

    list->items = items;
    list->count = copied;
}

static void parse_float_value(JsonValue *node, imaging_float_value_t *target)
{
    if (!node || !target)
        return;

    if (node->type == JSON_NUMBER) {
        target->present = 1;
        target->value = (float) node->value.number;
        target->has_value = 1;
        return;
    }

    if (node->type != JSON_OBJECT)
        return;

    target->present = 1;

    JsonValue *value = get_object_item(node, "value");
    if (value && value->type == JSON_NUMBER) {
        target->value = (float) value->value.number;
        target->has_value = 1;
    }

    JsonValue *min = get_object_item(node, "min");
    if (min && min->type == JSON_NUMBER) {
        target->min = (float) min->value.number;
        target->has_min = 1;
    }

    JsonValue *max = get_object_item(node, "max");
    if (max && max->type == JSON_NUMBER) {
        target->max = (float) max->value.number;
        target->has_max = 1;
    }

    if (!target->has_value && !target->has_min && !target->has_max)
        target->present = 0;
}

static void parse_mode_level(JsonValue *node, imaging_mode_level_t *target)
{
    if (!node || !target)
        return;

    if (node->type == JSON_STRING && node->value.string) {
        target->mode = dup_cstring(node->value.string);
        if (target->mode)
            target->present = 1;
        return;
    }

    if (node->type != JSON_OBJECT)
        return;

    target->present = 1;

    JsonValue *mode = get_object_item(node, "mode");
    if (mode && mode->type == JSON_STRING && mode->value.string)
        target->mode = dup_cstring(mode->value.string);

    parse_string_array(get_object_item(node, "modes"), &target->modes);
    parse_float_value(get_object_item(node, "level"), &target->level);
}

static void parse_focus_config(JsonValue *node, imaging_focus_config_t *target)
{
    if (!node || !target || node->type != JSON_OBJECT)
        return;

    target->present = 1;
    JsonValue *mode = get_object_item(node, "mode");
    if (mode && mode->type == JSON_STRING && mode->value.string)
        target->mode = dup_cstring(mode->value.string);

    parse_string_array(get_object_item(node, "modes"), &target->modes);
    parse_float_value(get_object_item(node, "default_speed"), &target->default_speed);
    parse_float_value(get_object_item(node, "near_limit"), &target->near_limit);
    parse_float_value(get_object_item(node, "far_limit"), &target->far_limit);
}

static void parse_focus_absolute(JsonValue *node, imaging_focus_absolute_move_t *target)
{
    if (!node || !target || node->type != JSON_OBJECT)
        return;

    get_string_from_json(&target->command, node, "command");
    parse_float_value(get_object_item(node, "position"), &target->position);
    parse_float_value(get_object_item(node, "speed"), &target->speed);
    if (target->command)
        target->supported = 1;
}

static void parse_focus_relative(JsonValue *node, imaging_focus_relative_move_t *target)
{
    if (!node || !target || node->type != JSON_OBJECT)
        return;

    get_string_from_json(&target->command, node, "command");
    parse_float_value(get_object_item(node, "distance"), &target->distance);
    parse_float_value(get_object_item(node, "speed"), &target->speed);
    if (target->command)
        target->supported = 1;
}

static void parse_focus_continuous(JsonValue *node, imaging_focus_continuous_move_t *target)
{
    if (!node || !target || node->type != JSON_OBJECT)
        return;

    get_string_from_json(&target->command, node, "command");
    parse_float_value(get_object_item(node, "speed"), &target->speed);
    if (target->command)
        target->supported = 1;
}

static void parse_focus_move(JsonValue *node, imaging_focus_move_config_t *target)
{
    if (!node || !target || node->type != JSON_OBJECT)
        return;

    parse_focus_absolute(get_object_item(node, "absolute"), &target->absolute);
    parse_focus_relative(get_object_item(node, "relative"), &target->relative);
    parse_focus_continuous(get_object_item(node, "continuous"), &target->continuous);
    get_string_from_json(&target->cmd_stop, node, "stop_command");
}

static void parse_white_balance(JsonValue *node, imaging_white_balance_config_t *target)
{
    if (!node || !target || node->type != JSON_OBJECT)
        return;

    target->present = 1;
    JsonValue *mode = get_object_item(node, "mode");
    if (mode && mode->type == JSON_STRING && mode->value.string)
        target->mode = dup_cstring(mode->value.string);

    parse_string_array(get_object_item(node, "modes"), &target->modes);
    parse_float_value(get_object_item(node, "cr_gain"), &target->cr_gain);
    parse_float_value(get_object_item(node, "cb_gain"), &target->cb_gain);
}

static void parse_exposure(JsonValue *node, imaging_exposure_config_t *target)
{
    if (!node || !target || node->type != JSON_OBJECT)
        return;

    target->present = 1;

    JsonValue *mode = get_object_item(node, "mode");
    if (mode && mode->type == JSON_STRING && mode->value.string)
        target->mode = dup_cstring(mode->value.string);

    parse_string_array(get_object_item(node, "modes"), &target->modes);

    JsonValue *priority = get_object_item(node, "priority");
    if (priority && priority->type == JSON_STRING && priority->value.string)
        target->priority = dup_cstring(priority->value.string);

    parse_string_array(get_object_item(node, "priority_modes"), &target->priorities);
    parse_float_value(get_object_item(node, "min_exposure_time"), &target->min_exposure_time);
    parse_float_value(get_object_item(node, "max_exposure_time"), &target->max_exposure_time);
    parse_float_value(get_object_item(node, "exposure_time"), &target->exposure_time);
    parse_float_value(get_object_item(node, "min_gain"), &target->min_gain);
    parse_float_value(get_object_item(node, "max_gain"), &target->max_gain);
    parse_float_value(get_object_item(node, "gain"), &target->gain);
    parse_float_value(get_object_item(node, "min_iris"), &target->min_iris);
    parse_float_value(get_object_item(node, "max_iris"), &target->max_iris);
    parse_float_value(get_object_item(node, "iris"), &target->iris);
}

static void parse_ircut_auto_adjust(JsonValue *node, imaging_ircut_auto_adjustment_t *target)
{
    if (!node || !target || node->type != JSON_OBJECT)
        return;

    target->present = 1;
    JsonValue *boundary_type = get_object_item(node, "boundary_type");
    if (boundary_type && boundary_type->type == JSON_STRING && boundary_type->value.string)
        target->boundary_type = dup_cstring(boundary_type->value.string);

    parse_string_array(get_object_item(node, "boundary_types"), &target->boundary_types);
    parse_float_value(get_object_item(node, "boundary_offset"), &target->boundary_offset);
    parse_float_value(get_object_item(node, "response_time"), &target->response_time);
}

static void free_mode_level(imaging_mode_level_t *target)
{
    if (!target)
        return;
    free(target->mode);
    target->mode = NULL;
    free_imaging_string_list(&target->modes);
}

static void free_focus_config(imaging_focus_config_t *target)
{
    if (!target)
        return;
    free(target->mode);
    target->mode = NULL;
    free_imaging_string_list(&target->modes);
}

static void free_white_balance_config(imaging_white_balance_config_t *target)
{
    if (!target)
        return;
    free(target->mode);
    target->mode = NULL;
    free_imaging_string_list(&target->modes);
}

static void free_exposure_config(imaging_exposure_config_t *target)
{
    if (!target)
        return;
    free(target->mode);
    target->mode = NULL;
    free(target->priority);
    target->priority = NULL;
    free_imaging_string_list(&target->modes);
    free_imaging_string_list(&target->priorities);
}

static void free_ircut_auto_adjust(imaging_ircut_auto_adjustment_t *target)
{
    if (!target)
        return;
    free(target->boundary_type);
    target->boundary_type = NULL;
    free_imaging_string_list(&target->boundary_types);
}

// Remember to free the string
static void get_string_from_json(char **var, JsonValue *j, const char *name)
{
    JsonValue *s = get_object_item(j, name);

    if (s && s->type == JSON_STRING) {
        const char *str_val = s->value.string;
        if (str_val != NULL) {
            *var = (char *) malloc((strlen(str_val) + 1) * sizeof(char));
            strcpy(*var, str_val);
        }
    }
}

void get_int_from_json(int *var, JsonValue *j, char *name)
{
    JsonValue *i = get_object_item(j, name);

    if (i && i->type == JSON_NUMBER) {
        *var = (int) i->value.number;
    }
}

void get_double_from_json(double *var, JsonValue *j, char *name)
{
    JsonValue *d = get_object_item(j, name);

    if (d && d->type == JSON_NUMBER) {
        *var = d->value.number;
    }
}

static int apply_loglevel_from_json(int *var, JsonValue *j, const char *name)
{
    JsonValue *l = get_object_item(j, name);

    if (!l)
        return 0;

    if (l->type == JSON_NUMBER) {
        // Handle numeric log level (backward compatibility)
        int level = (int) l->value.number;
        if (level >= LOG_LVL_FATAL && level <= LOG_LVL_TRACE) {
            *var = level;
        } else {
            log_warn("Invalid numeric log level %d in config key '%s', using default", level, name ? name : "log_level");
        }
    } else if (l->type == JSON_STRING) {
        // Handle textual log level
        const char *level_str = l->value.string;
        int level = log_level_from_string(level_str);
        if (level >= 0) {
            *var = level;
        } else {
            log_warn("Invalid textual log level '%s' in config key '%s', using default", level_str, name ? name : "log_level");
        }
    } else {
        log_warn("Invalid log level type for config key '%s', expected string or number", name ? name : "log_level");
    }

    return 1;
}

static int apply_bool_from_json(int *var, JsonValue *j, const char *name)
{
    JsonValue *b = get_object_item(j, name);

    if (!b)
        return 0;

    if (b->type == JSON_BOOL) {
        *var = b->value.boolean ? 1 : 0;
    } else {
        log_warn("Invalid boolean value for config key '%s', using default", name ? name : "unnamed");
    }

    return 1;
}

void get_bool_from_json(int *var, JsonValue *j, char *name)
{
    JsonValue *b = get_object_item(j, name);

    if (b && b->type == JSON_BOOL) {
        *var = b->value.boolean ? 1 : 0;
    } else {
        *var = 0;
    }
}

static ircut_mode_t parse_ircut_mode_string(const char *value)
{
    if (!value)
        return IRCUT_MODE_UNSPECIFIED;

    if (strcasecmp(value, "ON") == 0 || strcasecmp(value, "On") == 0) {
        return IRCUT_MODE_ON;
    } else if (strcasecmp(value, "OFF") == 0 || strcasecmp(value, "Off") == 0) {
        return IRCUT_MODE_OFF;
    } else if (strcasecmp(value, "AUTO") == 0 || strcasecmp(value, "Auto") == 0) {
        return IRCUT_MODE_AUTO;
    }

    return IRCUT_MODE_UNSPECIFIED;
}

int process_json_conf_file(char *file)
{
    JsonValue *value, *item;
    char *tmp;
    JsonValue *json_file;

    int i;
    char stmp[MAX_LEN];

    json_file = load_config(file);
    if (json_file == NULL) {
        log_error("Failed to parse JSON configuration file");
        return -1;
    }

    JsonValue *camera_section = NULL;
    JsonValue *server_section = NULL;

    JsonValue *tmp_section = get_object_item(json_file, "camera");
    if (tmp_section && tmp_section->type == JSON_OBJECT)
        camera_section = tmp_section;

    tmp_section = get_object_item(json_file, "server");
    if (tmp_section && tmp_section->type == JSON_OBJECT)
        server_section = tmp_section;

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
    service_ctx.events_min_interval_ms = 0;
    service_ctx.loglevel = 0;
    service_ctx.raw_log_directory = NULL;
    service_ctx.raw_log_on_error_only = 0;
    service_ctx.imaging = NULL;
    service_ctx.imaging_num = 0;
    memset(&service_ctx.audio, 0, sizeof(service_ctx.audio));
    service_ctx.audio.backchannel.output_level = 1;
    service_ctx.audio.backchannel.output_level_min = 1;
    service_ctx.audio.backchannel.output_level_max = 1;

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
    service_ctx.ptz_node.get_presets = NULL;
    service_ctx.ptz_node.max_preset_tours = 0;
    service_ctx.ptz_node.start_tracking = NULL;
    service_ctx.ptz_node.preset_tour_start = NULL;
    service_ctx.ptz_node.preset_tour_stop = NULL;
    service_ctx.ptz_node.preset_tour_pause = NULL;
    service_ctx.ptz_node.jump_to_abs_speed = NULL;
    service_ctx.ptz_node.jump_to_rel_speed = NULL;
    service_ctx.ptz_node.continuous_move = NULL;

    if (camera_section && get_object_item(camera_section, "model"))
        get_string_from_json(&(service_ctx.model), camera_section, "model");
    else
        get_string_from_json(&(service_ctx.model), json_file, "model");

    if (camera_section && get_object_item(camera_section, "manufacturer"))
        get_string_from_json(&(service_ctx.manufacturer), camera_section, "manufacturer");
    else
        get_string_from_json(&(service_ctx.manufacturer), json_file, "manufacturer");

    if (camera_section && get_object_item(camera_section, "firmware_ver"))
        get_string_from_json(&(service_ctx.firmware_ver), camera_section, "firmware_ver");
    else
        get_string_from_json(&(service_ctx.firmware_ver), json_file, "firmware_ver");

    if (camera_section && get_object_item(camera_section, "hardware_id"))
        get_string_from_json(&(service_ctx.hardware_id), camera_section, "hardware_id");
    else
        get_string_from_json(&(service_ctx.hardware_id), json_file, "hardware_id");

    if (camera_section && get_object_item(camera_section, "serial_num"))
        get_string_from_json(&(service_ctx.serial_num), camera_section, "serial_num");
    else
        get_string_from_json(&(service_ctx.serial_num), json_file, "serial_num");

    if (server_section && get_object_item(server_section, "ifs"))
        get_string_from_json(&(service_ctx.ifs), server_section, "ifs");
    else
        get_string_from_json(&(service_ctx.ifs), json_file, "ifs");

    if (server_section && get_object_item(server_section, "port"))
        get_int_from_json(&(service_ctx.port), server_section, "port");
    else
        get_int_from_json(&(service_ctx.port), json_file, "port");

    int loglevel_set = 0;
    if (server_section) {
        if (apply_loglevel_from_json(&(service_ctx.loglevel), server_section, "log_level")) {
            loglevel_set = 1;
        } else if (apply_loglevel_from_json(&(service_ctx.loglevel), server_section, "loglevel")) {
            loglevel_set = 1;
        }
    }
    if (!loglevel_set) {
        if (!apply_loglevel_from_json(&(service_ctx.loglevel), json_file, "log_level")) {
            apply_loglevel_from_json(&(service_ctx.loglevel), json_file, "loglevel");
        }
    }

    if (server_section && get_object_item(server_section, "log_directory")) {
        get_string_from_json(&(service_ctx.raw_log_directory), server_section, "log_directory");
    } else if (server_section && get_object_item(server_section, "raw_log_directory")) {
        get_string_from_json(&(service_ctx.raw_log_directory), server_section, "raw_log_directory");
    } else if (get_object_item(json_file, "log_directory")) {
        get_string_from_json(&(service_ctx.raw_log_directory), json_file, "log_directory");
    } else {
        get_string_from_json(&(service_ctx.raw_log_directory), json_file, "raw_log_directory");
    }

    int log_error_only_set = 0;
    if (server_section) {
        if (apply_bool_from_json(&(service_ctx.raw_log_on_error_only), server_section, "log_on_error_only")) {
            log_error_only_set = 1;
        } else if (apply_bool_from_json(&(service_ctx.raw_log_on_error_only), server_section, "raw_log_on_error_only")) {
            log_error_only_set = 1;
        }
    }
    if (!log_error_only_set) {
        if (!apply_bool_from_json(&(service_ctx.raw_log_on_error_only), json_file, "log_on_error_only")) {
            if (!apply_bool_from_json(&(service_ctx.raw_log_on_error_only), json_file, "raw_log_on_error_only")) {
                service_ctx.raw_log_on_error_only = 0;
            }
        }
    }

    value = get_object_item(json_file, "scopes");
    if (value && value->type == JSON_ARRAY) {
        int array_len = get_array_size(value);
        for (int i = 0; i < array_len; i++) {
            item = get_array_item(value, i);
            if (item && item->type == JSON_STRING) {
                service_ctx.scopes_num++;
                service_ctx.scopes = (char **) realloc(service_ctx.scopes, service_ctx.scopes_num * sizeof(char *));
                const char *str_val = item->value.string;
                service_ctx.scopes[service_ctx.scopes_num - 1] = (char *) malloc((strlen(str_val) + 1) * sizeof(char));
                strcpy(service_ctx.scopes[service_ctx.scopes_num - 1], str_val);
            }
        }
    }
    if (server_section && get_object_item(server_section, "username"))
        get_string_from_json(&(service_ctx.username), server_section, "username");
    else
        get_string_from_json(&(service_ctx.username), json_file, "username");

    if (server_section && get_object_item(server_section, "password"))
        get_string_from_json(&(service_ctx.password), server_section, "password");
    else
        get_string_from_json(&(service_ctx.password), json_file, "password");
    get_bool_from_json(&(service_ctx.adv_enable_media2), json_file, "adv_enable_media2");
    get_bool_from_json(&(service_ctx.adv_fault_if_unknown), json_file, "adv_fault_if_unknown");
    get_bool_from_json(&(service_ctx.adv_fault_if_set), json_file, "adv_fault_if_set");
    get_bool_from_json(&(service_ctx.adv_synology_nvr), json_file, "adv_synology_nvr");

    // Apply sane defaults for required fields if not provided in config
    if (service_ctx.manufacturer == NULL) {
        service_ctx.manufacturer = (char *) malloc(strlen(DEFAULT_MANUFACTURER) + 1);
        if (service_ctx.manufacturer)
            strcpy(service_ctx.manufacturer, DEFAULT_MANUFACTURER);
    }
    if (service_ctx.model == NULL) {
        service_ctx.model = (char *) malloc(strlen(DEFAULT_MODEL) + 1);
        if (service_ctx.model)
            strcpy(service_ctx.model, DEFAULT_MODEL);
    }
    if (service_ctx.firmware_ver == NULL) {
        service_ctx.firmware_ver = (char *) malloc(strlen(DEFAULT_FW_VER) + 1);
        if (service_ctx.firmware_ver)
            strcpy(service_ctx.firmware_ver, DEFAULT_FW_VER);
    }
    if (service_ctx.serial_num == NULL) {
        service_ctx.serial_num = (char *) malloc(strlen(DEFAULT_SERIAL_NUM) + 1);
        if (service_ctx.serial_num)
            strcpy(service_ctx.serial_num, DEFAULT_SERIAL_NUM);
    }
    if (service_ctx.hardware_id == NULL) {
        service_ctx.hardware_id = (char *) malloc(strlen(DEFAULT_HW_ID) + 1);
        if (service_ctx.hardware_id)
            strcpy(service_ctx.hardware_id, DEFAULT_HW_ID);
    }
    if (service_ctx.ifs == NULL) {
        service_ctx.ifs = (char *) malloc(strlen(DEFAULT_IFS) + 1);
        if (service_ctx.ifs)
            strcpy(service_ctx.ifs, DEFAULT_IFS);
    }

    // Print debug
    log_debug("model: %s", service_ctx.model);
    log_debug("manufacturer: %s", service_ctx.manufacturer);
    log_debug("firmware_ver: %s", service_ctx.firmware_ver);
    log_debug("hardware_id: %s", service_ctx.hardware_id);
    log_debug("serial_num: %s", service_ctx.serial_num);
    log_debug("ifs: %s", service_ctx.ifs);
    log_debug("port: %d", service_ctx.port);
    log_debug("log_directory: %s", service_ctx.raw_log_directory ? service_ctx.raw_log_directory : "(disabled)");
    log_debug("log_on_error_only: %d", service_ctx.raw_log_on_error_only);
    log_debug("scopes:");
    for (i = 0; i < service_ctx.scopes_num; i++) {
        log_debug("\t%s", service_ctx.scopes[i]);
    }
    if (service_ctx.username != NULL) {
        log_debug("username: %s", service_ctx.username);
        log_debug("password: %s", service_ctx.password ? service_ctx.password : "(null)");
    }
    // Load profiles from main configuration file
    value = get_object_item(json_file, "profiles");
    if (value && value->type == JSON_OBJECT) {
        JsonKeyValue *kv = value->value.object_head;
        while (kv) {
            const char *key = kv->key;
            item = kv->value;

            service_ctx.profiles_num++;
            service_ctx.profiles = (stream_profile_t *) realloc(service_ctx.profiles, service_ctx.profiles_num * sizeof(stream_profile_t));
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
            char *tmp = NULL;
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
            log_debug("Profile %s (%d): %s %dx%d",
                      key,
                      service_ctx.profiles_num - 1,
                      service_ctx.profiles[service_ctx.profiles_num - 1].name,
                      service_ctx.profiles[service_ctx.profiles_num - 1].width,
                      service_ctx.profiles[service_ctx.profiles_num - 1].height);

            kv = kv->next;
        }
    }

    value = get_object_item(json_file, "audio");
    if (value && value->type == JSON_OBJECT) {
        get_bool_from_json(&(service_ctx.audio.output_enabled), value, "output_enabled");

        JsonValue *backchannel = get_object_item(value, "backchannel");
        JsonValue *audio_cfg = (backchannel && backchannel->type == JSON_OBJECT) ? backchannel : value;

        get_string_from_json(&(service_ctx.audio.backchannel.name), audio_cfg, "name");
        get_string_from_json(&(service_ctx.audio.backchannel.token), audio_cfg, "token");
        get_string_from_json(&(service_ctx.audio.backchannel.configuration_token), audio_cfg, "configuration_token");
        get_string_from_json(&(service_ctx.audio.backchannel.receive_token), audio_cfg, "receive_token");
        get_string_from_json(&(service_ctx.audio.backchannel.uri), audio_cfg, "uri");
        get_string_from_json(&(service_ctx.audio.backchannel.transport), audio_cfg, "transport");
        get_int_from_json(&(service_ctx.audio.backchannel.output_level), audio_cfg, "output_level");

        JsonValue *level_range = get_object_item(audio_cfg, "output_level_range");
        if (level_range && level_range->type == JSON_OBJECT) {
            get_int_from_json(&(service_ctx.audio.backchannel.output_level_min), level_range, "min");
            get_int_from_json(&(service_ctx.audio.backchannel.output_level_max), level_range, "max");
        }
    }

    if (service_ctx.audio.backchannel.name == NULL)
        service_ctx.audio.backchannel.name = dup_cstring(DEFAULT_AUDIO_OUTPUT_NAME);
    if (service_ctx.audio.backchannel.token == NULL)
        service_ctx.audio.backchannel.token = dup_cstring(DEFAULT_AUDIO_OUTPUT_TOKEN);
    if (service_ctx.audio.backchannel.configuration_token == NULL)
        service_ctx.audio.backchannel.configuration_token = dup_cstring(DEFAULT_AUDIO_OUTPUT_CONFIGURATION_TOKEN);
    if (service_ctx.audio.backchannel.receive_token == NULL)
        service_ctx.audio.backchannel.receive_token = dup_cstring(DEFAULT_AUDIO_OUTPUT_RECEIVE_TOKEN);
    if (service_ctx.audio.backchannel.transport == NULL)
        service_ctx.audio.backchannel.transport = dup_cstring(DEFAULT_AUDIO_BACKCHANNEL_TRANSPORT);
    if (service_ctx.audio.backchannel.uri == NULL && service_ctx.profiles_num > 0 && service_ctx.profiles[0].url != NULL) {
        service_ctx.audio.backchannel.uri = dup_cstring(service_ctx.profiles[0].url);
    }
    if (service_ctx.audio.backchannel.uri == NULL)
        service_ctx.audio.backchannel.uri = dup_cstring("rtsp://%s/ch0");

    if (service_ctx.audio.backchannel.output_level_min == 0)
        service_ctx.audio.backchannel.output_level_min = 1;
    if (service_ctx.audio.backchannel.output_level_max == 0)
        service_ctx.audio.backchannel.output_level_max = service_ctx.audio.backchannel.output_level_min;
    if (service_ctx.audio.backchannel.output_level == 0)
        service_ctx.audio.backchannel.output_level = service_ctx.audio.backchannel.output_level_min;

    log_debug("audio.output_enabled: %d", service_ctx.audio.output_enabled);
    log_debug("audio.backchannel token: %s", service_ctx.audio.backchannel.token);
    log_debug("audio.backchannel uri: %s", service_ctx.audio.backchannel.uri);

    // Load PTZ configuration from main configuration file
    value = get_object_item(json_file, "ptz");
    if (value) {
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
        // Extensions
        get_int_from_json(&(service_ctx.ptz_node.max_preset_tours), value, "max_preset_tours");
        get_string_from_json(&(service_ctx.ptz_node.start_tracking), value, "start_tracking");
        get_string_from_json(&(service_ctx.ptz_node.preset_tour_start), value, "preset_tour_start");
        get_string_from_json(&(service_ctx.ptz_node.preset_tour_stop), value, "preset_tour_stop");
        get_string_from_json(&(service_ctx.ptz_node.preset_tour_pause), value, "preset_tour_pause");
        get_string_from_json(&(service_ctx.ptz_node.jump_to_abs_speed), value, "jump_to_abs_speed");
        get_string_from_json(&(service_ctx.ptz_node.jump_to_rel_speed), value, "jump_to_rel_speed");
        get_string_from_json(&(service_ctx.ptz_node.continuous_move), value, "continuous_move");
    }

    // Load relays configuration from main configuration file
    value = get_object_item(json_file, "relays");
    if (value && value->type == JSON_ARRAY) {
        int array_len = get_array_size(value);
        log_debug("Found %d relay entries in configuration", array_len);
        for (int i = 0; i < array_len; i++) {
            item = get_array_item(value, i);
            if (!item)
                continue;

            service_ctx.relay_outputs_num++;
            log_debug("Processing relay %zu, relay_outputs_num now: %d", i, service_ctx.relay_outputs_num);
            if (service_ctx.relay_outputs_num >= MAX_RELAY_OUTPUTS) {
                log_error("Too many relay outputs, max is: %d", MAX_RELAY_OUTPUTS);
                service_ctx.relay_outputs_num--;
                continue;
            }

            service_ctx.relay_outputs = (relay_output_t *) realloc(service_ctx.relay_outputs, service_ctx.relay_outputs_num * sizeof(relay_output_t));
            service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].idle_state = IDLE_STATE_CLOSE;
            service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].close = NULL;
            service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].open = NULL;
            char *tmp = NULL;
            get_string_from_json(&tmp, item, "idle_state");
            if (tmp) {
                if (!strcasecmp(tmp, "open"))
                    service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].idle_state = IDLE_STATE_OPEN;
                free(tmp);
            }
            get_string_from_json(&(service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].close), item, "close");
            get_string_from_json(&(service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].open), item, "open");
            log_debug("Relay %d configured - close: %s, open: %s",
                      service_ctx.relay_outputs_num - 1,
                      service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].close
                          ? service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].close
                          : "(null)",
                      service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].open
                          ? service_ctx.relay_outputs[service_ctx.relay_outputs_num - 1].open
                          : "(null)");
        }
        log_debug("Finished loading relays. Total relay_outputs_num: %d", service_ctx.relay_outputs_num);
    }

    // Load events configuration from main configuration file
    get_int_from_json(&(service_ctx.events_enable), json_file, "events_enable");
    // Optional global debounce for events (milliseconds); 0 disables
    get_int_from_json(&(service_ctx.events_min_interval_ms), json_file, "events_min_interval_ms");
    value = get_object_item(json_file, "events");
    if (value && value->type == JSON_ARRAY) {
        int array_len = get_array_size(value);
        for (int i = 0; i < array_len; i++) {
            item = get_array_item(value, i);
            if (!item)
                continue;

            service_ctx.events_num++;
            if (service_ctx.events_num > MAX_EVENTS) {
                log_error("Too many events, max is: %d", MAX_EVENTS);
                service_ctx.events_num--;
                break;
            }
            service_ctx.events = (event_t *) realloc(service_ctx.events, service_ctx.events_num * sizeof(event_t));
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

    value = get_object_item(json_file, "imaging");
    if (value && value->type == JSON_ARRAY) {
        int array_len = get_array_size(value);
        for (int i = 0; i < array_len; i++) {
            item = get_array_item(value, i);
            if (!item || item->type != JSON_OBJECT)
                continue;

            if (service_ctx.imaging_num >= MAX_IMAGING_ENTRIES) {
                log_warn("Ignoring imaging entry %d: max %d reached", i, MAX_IMAGING_ENTRIES);
                break;
            }

            service_ctx.imaging_num++;
            service_ctx.imaging = (imaging_entry_t *) realloc(service_ctx.imaging, service_ctx.imaging_num * sizeof(imaging_entry_t));
            imaging_entry_t *entry = &service_ctx.imaging[service_ctx.imaging_num - 1];
            memset(entry, 0, sizeof(*entry));
            entry->ircut_mode = IRCUT_MODE_AUTO;
            entry->focus_state = IMAGING_FOCUS_STATE_IDLE;

            get_string_from_json(&(entry->video_source_token), item, "video_source_token");
            if (entry->video_source_token == NULL) {
                entry->video_source_token = (char *) malloc(strlen("VideoSourceToken") + 1);
                if (entry->video_source_token)
                    strcpy(entry->video_source_token, "VideoSourceToken");
            }

            char *tmp_state = NULL;
            get_string_from_json(&tmp_state, item, "ircut_state");
            if (tmp_state != NULL) {
                ircut_mode_t parsed = parse_ircut_mode_string(tmp_state);
                if (parsed != IRCUT_MODE_UNSPECIFIED) {
                    entry->ircut_mode = parsed;
                }
                free(tmp_state);
            }

            JsonValue *modes = get_object_item(item, "ircut_modes");
            if (modes && modes->type == JSON_ARRAY) {
                int modes_len = get_array_size(modes);
                for (int j = 0; j < modes_len; j++) {
                    JsonValue *mode_val = get_array_item(modes, j);
                    if (mode_val && mode_val->type == JSON_STRING) {
                        ircut_mode_t parsed = parse_ircut_mode_string(mode_val->value.string);
                        if (parsed == IRCUT_MODE_ON)
                            entry->supports_ircut_on = 1;
                        else if (parsed == IRCUT_MODE_OFF)
                            entry->supports_ircut_off = 1;
                        else if (parsed == IRCUT_MODE_AUTO)
                            entry->supports_ircut_auto = 1;
                    }
                }
            }

            if (!entry->supports_ircut_on && !entry->supports_ircut_off && !entry->supports_ircut_auto) {
                // Default to manual day/night if no explicit modes provided
                entry->supports_ircut_on = 1;
                entry->supports_ircut_off = 1;
            }

            get_string_from_json(&(entry->cmd_ircut_on), item, "cmd_ircut_on");
            get_string_from_json(&(entry->cmd_ircut_off), item, "cmd_ircut_off");
            get_string_from_json(&(entry->cmd_ircut_auto), item, "cmd_ircut_auto");

            if (entry->ircut_mode == IRCUT_MODE_UNSPECIFIED) {
                if (entry->supports_ircut_auto)
                    entry->ircut_mode = IRCUT_MODE_AUTO;
                else if (entry->supports_ircut_on)
                    entry->ircut_mode = IRCUT_MODE_ON;
                else if (entry->supports_ircut_off)
                    entry->ircut_mode = IRCUT_MODE_OFF;
                else
                    entry->ircut_mode = IRCUT_MODE_AUTO;
            }

            parse_mode_level(get_object_item(item, "backlight_compensation"), &entry->backlight);
            parse_float_value(get_object_item(item, "brightness"), &entry->brightness);
            parse_float_value(get_object_item(item, "color_saturation"), &entry->color_saturation);
            parse_float_value(get_object_item(item, "contrast"), &entry->contrast);
            parse_float_value(get_object_item(item, "sharpness"), &entry->sharpness);
            parse_exposure(get_object_item(item, "exposure"), &entry->exposure);
            parse_focus_config(get_object_item(item, "focus"), &entry->focus);
            parse_mode_level(get_object_item(item, "wide_dynamic_range"), &entry->wide_dynamic_range);
            parse_white_balance(get_object_item(item, "white_balance"), &entry->white_balance);
            parse_ircut_auto_adjust(get_object_item(item, "ircut_auto_adjustment"), &entry->ircut_auto_adjustment);
            parse_mode_level(get_object_item(item, "image_stabilization"), &entry->image_stabilization);
            parse_mode_level(get_object_item(item, "tone_compensation"), &entry->tone_compensation);
            parse_mode_level(get_object_item(item, "defogging"), &entry->defogging);
            parse_float_value(get_object_item(item, "noise_reduction"), &entry->noise_reduction);
            parse_focus_move(get_object_item(item, "focus_move"), &entry->focus_move);
            get_string_from_json(&(entry->cmd_apply_preset), item, "cmd_apply_preset");
            get_string_from_json(&(entry->default_preset_token), item, "default_preset_token");

            JsonValue *presets = get_object_item(item, "presets");
            if (presets && presets->type == JSON_ARRAY) {
                int preset_len = get_array_size(presets);
                if (preset_len > 0) {
                    entry->presets = (imaging_preset_entry_t *) calloc((size_t) preset_len, sizeof(imaging_preset_entry_t));
                    if (entry->presets) {
                        for (int j = 0; j < preset_len; j++) {
                            JsonValue *preset_node = get_array_item(presets, j);
                            if (!preset_node || preset_node->type != JSON_OBJECT)
                                continue;

                            imaging_preset_entry_t *preset = &entry->presets[entry->preset_count];
                            get_string_from_json(&preset->token, preset_node, "token");
                            if (!preset->token)
                                continue;
                            get_string_from_json(&preset->name, preset_node, "name");
                            get_string_from_json(&preset->type, preset_node, "type");
                            get_string_from_json(&preset->command, preset_node, "command");
                            entry->preset_count++;
                            if (entry->preset_count >= preset_len)
                                break;
                        }
                    }
                }
            }

            if (entry->default_preset_token != NULL) {
                entry->current_preset_token = dup_cstring(entry->default_preset_token);
            } else if (entry->preset_count > 0 && entry->presets && entry->presets[0].token != NULL) {
                entry->current_preset_token = dup_cstring(entry->presets[0].token);
            }

            log_debug("Imaging[%d] token=%s ircut=%d modes on:%d off:%d auto:%d",
                      service_ctx.imaging_num - 1,
                      entry->video_source_token ? entry->video_source_token : "(null)",
                      entry->ircut_mode,
                      entry->supports_ircut_on,
                      entry->supports_ircut_off,
                      entry->supports_ircut_auto);
        }
    }

    log_debug("adv_enable_media2: %d", service_ctx.adv_enable_media2);
    log_debug("adv_fault_if_unknown: %d", service_ctx.adv_fault_if_unknown);
    log_debug("adv_fault_if_set: %d", service_ctx.adv_fault_if_set);
    log_debug("adv_synology_nvr: %d", service_ctx.adv_synology_nvr);
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
            service_ctx.events = (event_t *) realloc(service_ctx.events, service_ctx.events_num * sizeof(event_t));
            service_ctx.events[service_ctx.events_num - 1].topic = (char *) malloc(strlen("tns1:Device/Trigger/Relay") + 1);
            strcpy(service_ctx.events[service_ctx.events_num - 1].topic, "tns1:Device/Trigger/Relay");
            log_debug("topic: tns1:Device/Trigger/Relay");
            service_ctx.events[service_ctx.events_num - 1].source_name = (char *) malloc(strlen("RelayToken") + 1);
            strcpy(service_ctx.events[service_ctx.events_num - 1].source_name, "RelayToken");
            log_debug("source_name: RelayToken");
            service_ctx.events[service_ctx.events_num - 1].source_type = (char *) malloc(strlen("tt:ReferenceToken") + 1);
            strcpy(service_ctx.events[service_ctx.events_num - 1].source_type, "tt:ReferenceToken");
            log_debug("source_type: tt:ReferenceToken");
            sprintf(stmp, "RelayOutputToken_%d", i);
            service_ctx.events[service_ctx.events_num - 1].source_value = (char *) malloc(strlen(stmp) + 1);
            strcpy(service_ctx.events[service_ctx.events_num - 1].source_value, stmp);
            log_debug("source_value: %s", stmp);
            sprintf(stmp, "/tmp/onvif_notify_server/relay_output_%d", i);
            service_ctx.events[service_ctx.events_num - 1].input_file = (char *) malloc(strlen(stmp) + 1);
            strcpy(service_ctx.events[service_ctx.events_num - 1].input_file, stmp);
            log_debug("input_file: %s", stmp);
        }
    }

    // Keep json_file alive - it will be freed by the system when the program exits
    // In a long-running program, you would store json_file globally and free it in cleanup

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

    if (service_ctx.audio.backchannel.name != NULL)
        free(service_ctx.audio.backchannel.name);
    if (service_ctx.audio.backchannel.token != NULL)
        free(service_ctx.audio.backchannel.token);
    if (service_ctx.audio.backchannel.configuration_token != NULL)
        free(service_ctx.audio.backchannel.configuration_token);
    if (service_ctx.audio.backchannel.receive_token != NULL)
        free(service_ctx.audio.backchannel.receive_token);
    if (service_ctx.audio.backchannel.uri != NULL)
        free(service_ctx.audio.backchannel.uri);
    if (service_ctx.audio.backchannel.transport != NULL)
        free(service_ctx.audio.backchannel.transport);

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
        if (service_ctx.ptz_node.get_presets != NULL)
            free(service_ctx.ptz_node.get_presets);
        if (service_ctx.ptz_node.start_tracking != NULL)
            free(service_ctx.ptz_node.start_tracking);
        if (service_ctx.ptz_node.preset_tour_start != NULL)
            free(service_ctx.ptz_node.preset_tour_start);
        if (service_ctx.ptz_node.preset_tour_stop != NULL)
            free(service_ctx.ptz_node.preset_tour_stop);
        if (service_ctx.ptz_node.preset_tour_pause != NULL)
            free(service_ctx.ptz_node.preset_tour_pause);
        if (service_ctx.ptz_node.jump_to_abs_speed != NULL)
            free(service_ctx.ptz_node.jump_to_abs_speed);
        if (service_ctx.ptz_node.jump_to_rel_speed != NULL)
            free(service_ctx.ptz_node.jump_to_rel_speed);
        if (service_ctx.ptz_node.continuous_move != NULL)
            free(service_ctx.ptz_node.continuous_move);
    }

    for (i = service_ctx.relay_outputs_num - 1; i >= 0; i--) {
        if (service_ctx.relay_outputs[i].open != NULL)
            free(service_ctx.relay_outputs[i].open);
        if (service_ctx.relay_outputs[i].close != NULL)
            free(service_ctx.relay_outputs[i].close);
    }
    if (service_ctx.relay_outputs != NULL)
        free(service_ctx.relay_outputs);

    for (i = service_ctx.imaging_num - 1; i >= 0; i--) {
        if (service_ctx.imaging[i].video_source_token != NULL)
            free(service_ctx.imaging[i].video_source_token);
        if (service_ctx.imaging[i].cmd_ircut_on != NULL)
            free(service_ctx.imaging[i].cmd_ircut_on);
        if (service_ctx.imaging[i].cmd_ircut_off != NULL)
            free(service_ctx.imaging[i].cmd_ircut_off);
        if (service_ctx.imaging[i].cmd_ircut_auto != NULL)
            free(service_ctx.imaging[i].cmd_ircut_auto);
        free_mode_level(&service_ctx.imaging[i].backlight);
        free_mode_level(&service_ctx.imaging[i].wide_dynamic_range);
        free_mode_level(&service_ctx.imaging[i].image_stabilization);
        free_mode_level(&service_ctx.imaging[i].tone_compensation);
        free_mode_level(&service_ctx.imaging[i].defogging);
        free_exposure_config(&service_ctx.imaging[i].exposure);
        free_focus_config(&service_ctx.imaging[i].focus);
        free_white_balance_config(&service_ctx.imaging[i].white_balance);
        free_ircut_auto_adjust(&service_ctx.imaging[i].ircut_auto_adjustment);
        if (service_ctx.imaging[i].focus_move.absolute.command)
            free(service_ctx.imaging[i].focus_move.absolute.command);
        if (service_ctx.imaging[i].focus_move.relative.command)
            free(service_ctx.imaging[i].focus_move.relative.command);
        if (service_ctx.imaging[i].focus_move.continuous.command)
            free(service_ctx.imaging[i].focus_move.continuous.command);
        if (service_ctx.imaging[i].focus_move.cmd_stop)
            free(service_ctx.imaging[i].focus_move.cmd_stop);
        if (service_ctx.imaging[i].cmd_apply_preset)
            free(service_ctx.imaging[i].cmd_apply_preset);
        if (service_ctx.imaging[i].default_preset_token)
            free(service_ctx.imaging[i].default_preset_token);
        if (service_ctx.imaging[i].current_preset_token)
            free(service_ctx.imaging[i].current_preset_token);
        if (service_ctx.imaging[i].presets) {
            for (int j = 0; j < service_ctx.imaging[i].preset_count; j++) {
                if (service_ctx.imaging[i].presets[j].token)
                    free(service_ctx.imaging[i].presets[j].token);
                if (service_ctx.imaging[i].presets[j].name)
                    free(service_ctx.imaging[i].presets[j].name);
                if (service_ctx.imaging[i].presets[j].type)
                    free(service_ctx.imaging[i].presets[j].type);
                if (service_ctx.imaging[i].presets[j].command)
                    free(service_ctx.imaging[i].presets[j].command);
            }
            free(service_ctx.imaging[i].presets);
        }
    }
    if (service_ctx.imaging != NULL)
        free(service_ctx.imaging);

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
    if (service_ctx.raw_log_directory != NULL)
        free(service_ctx.raw_log_directory);
}
