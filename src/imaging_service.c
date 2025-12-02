/*
 * Copyright (c) 2025 Thingino
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

#include "imaging_service.h"

#include "fault.h"
#include "log.h"
#include "mxml_wrapper.h"
#include "onvif_simple_server.h"
#include "prudynt_bridge.h"
#include "utils.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define IMAGING_XML_BUFFER 16384
#define IMAGING_COMMAND_BUFFER 1024

static imaging_entry_t *find_imaging_entry(const char *token);
static void execute_backend_command(const char *command);

static const char *focus_move_status_to_string(imaging_focus_state_t state)
{
    switch (state) {
    case IMAGING_FOCUS_STATE_MOVING:
        return "MOVING";
    case IMAGING_FOCUS_STATE_IDLE:
        return "IDLE";
    case IMAGING_FOCUS_STATE_UNKNOWN:
    default:
        return "UNKNOWN";
    }
}

static char *dup_string(const char *src)
{
    if (!src)
        return NULL;
    size_t len = strlen(src) + 1;
    char *copy = (char *) malloc(len);
    if (!copy)
        return NULL;
    memcpy(copy, src, len);
    return copy;
}

static int parse_float_text(const char *text, float *out_value)
{
    if (!text || !out_value)
        return 0;

    char *end = NULL;
    float value = strtof(text, &end);
    if (text == end)
        return 0;

    *out_value = value;
    return 1;
}

static int value_within_range(const imaging_float_value_t *range, float value)
{
    if (!range)
        return 1;
    if (range->has_min && value < range->min)
        return 0;
    if (range->has_max && value > range->max)
        return 0;
    return 1;
}

static imaging_entry_t *require_imaging_entry(const char *token)
{
    imaging_entry_t *entry = find_imaging_entry(token);
    if (entry)
        return entry;

    send_fault("imaging_service",
               "Sender",
               "ter:InvalidArgVal",
               "ter:NoSource",
               "Unknown video source",
               "The requested VideoSourceToken does not exist");
    return NULL;
}

static int focus_move_capable(const imaging_focus_move_config_t *focus_move)
{
    if (!focus_move)
        return 0;
    return focus_move->absolute.supported || focus_move->relative.supported || focus_move->continuous.supported;
}

static int execute_formatted_command(const char *template, float arg1, float arg2)
{
    if (!template || template[0] == '\0')
        return -1;

    char command[IMAGING_COMMAND_BUFFER];
    int written = snprintf(command, sizeof(command), template, arg1, arg2);
    if (written < 0 || (size_t) written >= sizeof(command)) {
        log_error("Imaging command template '%s' overflow", template);
        return -1;
    }

    execute_backend_command(command);
    return 0;
}

extern service_context_t service_ctx;

static imaging_entry_t *find_imaging_entry(const char *token)
{
    if (service_ctx.imaging_num == 0)
        return NULL;

    if (token == NULL || token[0] == '\0') {
        return &service_ctx.imaging[0];
    }

    for (int i = 0; i < service_ctx.imaging_num; i++) {
        if (service_ctx.imaging[i].video_source_token != NULL && strcasecmp(token, service_ctx.imaging[i].video_source_token) == 0) {
            return &service_ctx.imaging[i];
        }
    }

    return NULL;
}

static const char *ircut_mode_to_string(ircut_mode_t mode)
{
    switch (mode) {
    case IRCUT_MODE_ON:
        return "On";
    case IRCUT_MODE_OFF:
        return "Off";
    case IRCUT_MODE_AUTO:
        return "Auto";
    default:
        return "Auto";
    }
}

static ircut_mode_t ircut_mode_from_string(const char *value)
{
    if (!value)
        return IRCUT_MODE_UNSPECIFIED;

    if (strcasecmp(value, "ON") == 0 || strcasecmp(value, "On") == 0)
        return IRCUT_MODE_ON;
    if (strcasecmp(value, "OFF") == 0 || strcasecmp(value, "Off") == 0)
        return IRCUT_MODE_OFF;
    if (strcasecmp(value, "AUTO") == 0 || strcasecmp(value, "Auto") == 0)
        return IRCUT_MODE_AUTO;

    return IRCUT_MODE_UNSPECIFIED;
}

static const char *find_child_value(mxml_node_t *node, const char *tag)
{
    if (!node || !tag)
        return NULL;

    if (mxmlGetType(node) == MXML_TYPE_ELEMENT) {
        const char *name = mxmlGetElement(node);
        if (name && strcmp(name, tag) == 0) {
            mxml_node_t *text = mxmlGetFirstChild(node);
            while (text) {
                if (mxmlGetType(text) == MXML_TYPE_TEXT)
                    return mxmlGetText(text, NULL);
                text = mxmlGetNextSibling(text);
            }
        }
    }

    mxml_node_t *child = mxmlGetFirstChild(node);
    while (child) {
        const char *result = find_child_value(child, tag);
        if (result)
            return result;
        child = mxmlGetNextSibling(child);
    }

    return NULL;
}

static void apply_prudynt_float(imaging_float_value_t *target, const prudynt_field_state_t *source)
{
    if (!target || !source || !source->present)
        return;
    target->present = 1;
    target->has_value = 1;
    target->value = source->value;
    target->has_min = 1;
    target->min = source->min;
    target->has_max = 1;
    target->max = source->max;
}

static void apply_prudynt_mode_level(imaging_mode_level_t *target, const prudynt_field_state_t *source)
{
    if (!target || !source || !source->present)
        return;
    target->present = 1;
    apply_prudynt_float(&target->level, source);
}

static void merge_prudynt_state(imaging_entry_t *entry, const prudynt_imaging_state_t *state)
{
    if (!entry || !state)
        return;
    apply_prudynt_float(&entry->brightness, &state->brightness);
    apply_prudynt_float(&entry->color_saturation, &state->saturation);
    apply_prudynt_float(&entry->contrast, &state->contrast);
    apply_prudynt_float(&entry->sharpness, &state->sharpness);
    apply_prudynt_float(&entry->noise_reduction, &state->noise_reduction);
    apply_prudynt_mode_level(&entry->backlight, &state->backlight);
    apply_prudynt_mode_level(&entry->wide_dynamic_range, &state->wide_dynamic_range);
    apply_prudynt_mode_level(&entry->tone_compensation, &state->tone);
    apply_prudynt_mode_level(&entry->defogging, &state->defog);
}

static int parse_imaging_value(const char *text, float *out_value, int *out_is_normalized)
{
    if (!text || !out_value)
        return 0;

    if (out_is_normalized)
        *out_is_normalized = 0;

    char *end = NULL;
    double value = strtod(text, &end);
    if (text == end)
        return 0;

    if (end && *end == '%') {
        value /= 100.0;
        if (out_is_normalized)
            *out_is_normalized = 1;
    } else if (value >= 0.0 && value <= 1.0) {
        if (out_is_normalized)
            *out_is_normalized = 1;
    }

    *out_value = (float) value;
    return 1;
}

static float clamp01f(float value)
{
    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

static const prudynt_field_state_t *lookup_state_field(const prudynt_imaging_state_t *state, const char *key)
{
    if (!state || !key)
        return NULL;

    if (strcmp(key, "brightness") == 0)
        return &state->brightness;
    if (strcmp(key, "contrast") == 0)
        return &state->contrast;
    if (strcmp(key, "saturation") == 0)
        return &state->saturation;
    if (strcmp(key, "sharpness") == 0)
        return &state->sharpness;
    if (strcmp(key, "backlight") == 0)
        return &state->backlight;
    if (strcmp(key, "wide_dynamic_range") == 0)
        return &state->wide_dynamic_range;
    if (strcmp(key, "tone") == 0)
        return &state->tone;
    if (strcmp(key, "defog") == 0)
        return &state->defog;
    if (strcmp(key, "noise_reduction") == 0)
        return &state->noise_reduction;

    return NULL;
}

static void fallback_range_for_key(const char *key, float *out_min, float *out_max)
{
    if (!out_min || !out_max)
        return;

    *out_min = 0.0f;

    if (key && strcmp(key, "backlight") == 0) {
        *out_max = 10.0f;
        return;
    }

    *out_max = 255.0f;
}

static float normalize_with_state(const char *key, float value, int value_is_normalized, const prudynt_imaging_state_t *state)
{
    if (value_is_normalized)
        return clamp01f(value);

    const prudynt_field_state_t *field = lookup_state_field(state, key);
    if (field && field->present && field->max > field->min) {
        float span = field->max - field->min;
        if (span > 0.0f)
            return clamp01f((value - field->min) / span);
    }

    float fallback_min = 0.0f;
    float fallback_max = 1.0f;
    fallback_range_for_key(key, &fallback_min, &fallback_max);
    float span = fallback_max - fallback_min;
    if (span <= 0.0f)
        return clamp01f(value);

    return clamp01f((value - fallback_min) / span);
}

typedef struct {
    char *start;
    char *cursor;
    size_t remaining;
} xml_builder_t;

static void xml_builder_init(xml_builder_t *builder, char *buffer, size_t buffer_len)
{
    if (!builder)
        return;
    builder->start = buffer;
    builder->cursor = buffer;
    builder->remaining = buffer_len;
    if (buffer_len > 0 && buffer)
        buffer[0] = '\0';
}

static void xml_builder_append(xml_builder_t *builder, const char *fmt, ...)
{
    if (!builder || !builder->cursor || builder->remaining == 0)
        return;

    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(builder->cursor, builder->remaining, fmt, args);
    va_end(args);

    if (written < 0)
        return;

    if ((size_t) written >= builder->remaining) {
        builder->cursor += builder->remaining - 1;
        builder->remaining = 0;
        builder->cursor[0] = '\0';
        return;
    }

    builder->cursor += written;
    builder->remaining -= (size_t) written;
}

static void append_string_element(xml_builder_t *builder, const char *tag, const char *value)
{
    if (!builder || !tag || !value || value[0] == '\0')
        return;
    xml_builder_append(builder, "<tt:%s>%s</tt:%s>\n", tag, value, tag);
}

static void append_float_element(xml_builder_t *builder, const char *tag, const imaging_float_value_t *value)
{
    if (!builder || !tag || !value || !value->present || !value->has_value)
        return;
    xml_builder_append(builder, "<tt:%s>%.6f</tt:%s>\n", tag, value->value, tag);
}

static void append_float_range(xml_builder_t *builder, const char *tag, const imaging_float_value_t *value)
{
    if (!builder || !tag || !value)
        return;
    if (!value->has_min && !value->has_max)
        return;
    xml_builder_append(builder, "<tt:%s>\n", tag);
    if (value->has_min)
        xml_builder_append(builder, "<tt:Min>%.6f</tt:Min>", value->min);
    if (value->has_max)
        xml_builder_append(builder, "<tt:Max>%.6f</tt:Max>", value->max);
    xml_builder_append(builder, "</tt:%s>\n", tag);
}

static void append_string_list(xml_builder_t *builder, const char *tag, const imaging_string_list_t *list)
{
    if (!builder || !tag || !list || list->count == 0 || list->items == NULL)
        return;
    for (int i = 0; i < list->count; i++) {
        if (!list->items[i])
            continue;
        xml_builder_append(builder, "<tt:%s>%s</tt:%s>\n", tag, list->items[i], tag);
    }
}

static void append_mode_level_setting(xml_builder_t *builder, const char *outer_tag, const imaging_mode_level_t *config)
{
    if (!builder || !outer_tag || !config || !config->present)
        return;

    xml_builder_append(builder, "<tt:%s>\n", outer_tag);
    append_string_element(builder, "Mode", config->mode);
    append_float_element(builder, "Level", &config->level);
    xml_builder_append(builder, "</tt:%s>\n", outer_tag);
}

static void append_mode_level_options(xml_builder_t *builder, const char *options_tag, const imaging_mode_level_t *config)
{
    if (!builder || !options_tag || !config)
        return;

    int has_content = (config->modes.count > 0 && config->modes.items != NULL) || config->level.has_min || config->level.has_max;
    if (!has_content)
        return;

    xml_builder_append(builder, "<tt:%s>\n", options_tag);
    append_string_list(builder, "Mode", &config->modes);
    append_float_range(builder, "Level", &config->level);
    xml_builder_append(builder, "</tt:%s>\n", options_tag);
}

static void append_exposure_settings(xml_builder_t *builder, const imaging_exposure_config_t *config)
{
    if (!builder || !config || !config->present)
        return;

    xml_builder_append(builder, "<tt:Exposure>\n");
    append_string_element(builder, "Mode", config->mode);
    append_string_element(builder, "Priority", config->priority);
    append_float_element(builder, "MinExposureTime", &config->min_exposure_time);
    append_float_element(builder, "MaxExposureTime", &config->max_exposure_time);
    append_float_element(builder, "ExposureTime", &config->exposure_time);
    append_float_element(builder, "MinGain", &config->min_gain);
    append_float_element(builder, "MaxGain", &config->max_gain);
    append_float_element(builder, "Gain", &config->gain);
    append_float_element(builder, "MinIris", &config->min_iris);
    append_float_element(builder, "MaxIris", &config->max_iris);
    append_float_element(builder, "Iris", &config->iris);
    xml_builder_append(builder, "</tt:Exposure>\n");
}

static void append_exposure_options(xml_builder_t *builder, const imaging_exposure_config_t *config)
{
    if (!builder || !config)
        return;

    int has_range = config->min_exposure_time.has_min || config->min_exposure_time.has_max || config->max_exposure_time.has_min
                    || config->max_exposure_time.has_max || config->exposure_time.has_min || config->exposure_time.has_max || config->min_gain.has_min
                    || config->min_gain.has_max || config->max_gain.has_min || config->max_gain.has_max || config->gain.has_min
                    || config->gain.has_max || config->min_iris.has_min || config->min_iris.has_max || config->max_iris.has_min
                    || config->max_iris.has_max || config->iris.has_min || config->iris.has_max;

    if (!has_range && config->modes.count == 0 && config->priorities.count == 0)
        return;

    xml_builder_append(builder, "<tt:ExposureOptions20>\n");
    append_string_list(builder, "Mode", &config->modes);
    append_string_list(builder, "Priority", &config->priorities);
    append_float_range(builder, "MinExposureTime", &config->min_exposure_time);
    append_float_range(builder, "MaxExposureTime", &config->max_exposure_time);
    append_float_range(builder, "ExposureTime", &config->exposure_time);
    append_float_range(builder, "MinGain", &config->min_gain);
    append_float_range(builder, "MaxGain", &config->max_gain);
    append_float_range(builder, "Gain", &config->gain);
    append_float_range(builder, "MinIris", &config->min_iris);
    append_float_range(builder, "MaxIris", &config->max_iris);
    append_float_range(builder, "Iris", &config->iris);
    xml_builder_append(builder, "</tt:ExposureOptions20>\n");
}

static void append_focus_settings(xml_builder_t *builder, const imaging_focus_config_t *config)
{
    if (!builder || !config || !config->present)
        return;

    xml_builder_append(builder, "<tt:Focus>\n");
    append_string_element(builder, "AutoFocusMode", config->mode);
    append_float_element(builder, "DefaultSpeed", &config->default_speed);
    append_float_element(builder, "NearLimit", &config->near_limit);
    append_float_element(builder, "FarLimit", &config->far_limit);
    xml_builder_append(builder, "</tt:Focus>\n");
}

static void append_focus_options(xml_builder_t *builder, const imaging_focus_config_t *config)
{
    if (!builder || !config)
        return;

    int has_range = config->default_speed.has_min || config->default_speed.has_max || config->near_limit.has_min || config->near_limit.has_max
                    || config->far_limit.has_min || config->far_limit.has_max;

    if (!has_range && config->modes.count == 0)
        return;

    xml_builder_append(builder, "<tt:FocusOptions20>\n");
    append_string_list(builder, "AutoFocusModes", &config->modes);
    append_float_range(builder, "DefaultSpeed", &config->default_speed);
    append_float_range(builder, "NearLimit", &config->near_limit);
    append_float_range(builder, "FarLimit", &config->far_limit);
    xml_builder_append(builder, "</tt:FocusOptions20>\n");
}

static void append_white_balance_settings(xml_builder_t *builder, const imaging_white_balance_config_t *config)
{
    if (!builder || !config || !config->present)
        return;

    xml_builder_append(builder, "<tt:WhiteBalance>\n");
    append_string_element(builder, "Mode", config->mode);
    append_float_element(builder, "CrGain", &config->cr_gain);
    append_float_element(builder, "CbGain", &config->cb_gain);
    xml_builder_append(builder, "</tt:WhiteBalance>\n");
}

static void append_white_balance_options(xml_builder_t *builder, const imaging_white_balance_config_t *config)
{
    if (!builder || !config)
        return;

    int has_range = config->cr_gain.has_min || config->cr_gain.has_max || config->cb_gain.has_min || config->cb_gain.has_max;

    if (!has_range && config->modes.count == 0)
        return;

    xml_builder_append(builder, "<tt:WhiteBalanceOptions20>\n");
    append_string_list(builder, "Mode", &config->modes);
    append_float_range(builder, "CrGain", &config->cr_gain);
    append_float_range(builder, "CbGain", &config->cb_gain);
    xml_builder_append(builder, "</tt:WhiteBalanceOptions20>\n");
}

static void append_ircut_auto_adjustment_settings(xml_builder_t *builder, const imaging_ircut_auto_adjustment_t *config)
{
    if (!builder || !config || !config->present)
        return;

    xml_builder_append(builder, "<tt:IrCutFilterAutoAdjustment>\n");
    append_string_element(builder, "BoundaryType", config->boundary_type);
    append_float_element(builder, "BoundaryOffset", &config->boundary_offset);
    append_float_element(builder, "ResponseTime", &config->response_time);
    xml_builder_append(builder, "</tt:IrCutFilterAutoAdjustment>\n");
}

static void append_ircut_auto_adjustment_options(xml_builder_t *builder, const imaging_ircut_auto_adjustment_t *config)
{
    if (!builder || !config)
        return;

    int has_range = config->boundary_offset.has_min || config->boundary_offset.has_max || config->response_time.has_min
                    || config->response_time.has_max;

    if (!has_range && config->boundary_types.count == 0)
        return;

    xml_builder_append(builder, "<tt:IrCutFilterAutoAdjustmentOptions>\n");
    append_string_list(builder, "BoundaryType", &config->boundary_types);
    append_float_range(builder, "BoundaryOffset", &config->boundary_offset);
    append_float_range(builder, "ResponseTime", &config->response_time);
    xml_builder_append(builder, "</tt:IrCutFilterAutoAdjustmentOptions>\n");
}

static void append_ircut_modes(xml_builder_t *builder, const imaging_entry_t *entry)
{
    if (!builder || !entry)
        return;

    int appended = 0;
    if (entry->supports_ircut_on) {
        xml_builder_append(builder, "<tt:IrCutFilterModes>On</tt:IrCutFilterModes>\n");
        appended = 1;
    }
    if (entry->supports_ircut_off) {
        xml_builder_append(builder, "<tt:IrCutFilterModes>Off</tt:IrCutFilterModes>\n");
        appended = 1;
    }
    if (entry->supports_ircut_auto) {
        xml_builder_append(builder, "<tt:IrCutFilterModes>Auto</tt:IrCutFilterModes>\n");
        appended = 1;
    }

    if (!appended) {
        xml_builder_append(builder, "<tt:IrCutFilterModes>On</tt:IrCutFilterModes>\n<tt:IrCutFilterModes>Off</tt:IrCutFilterModes>\n");
    }
}

static void append_focus_move_absolute_options(xml_builder_t *builder, const imaging_focus_absolute_move_t *config)
{
    if (!builder || !config || !config->supported)
        return;

    xml_builder_append(builder, "<tt:Absolute>\n");
    append_float_range(builder, "Position", &config->position);
    append_float_range(builder, "Speed", &config->speed);
    xml_builder_append(builder, "</tt:Absolute>\n");
}

static void append_focus_move_relative_options(xml_builder_t *builder, const imaging_focus_relative_move_t *config)
{
    if (!builder || !config || !config->supported)
        return;

    xml_builder_append(builder, "<tt:Relative>\n");
    append_float_range(builder, "Distance", &config->distance);
    append_float_range(builder, "Speed", &config->speed);
    xml_builder_append(builder, "</tt:Relative>\n");
}

static void append_focus_move_continuous_options(xml_builder_t *builder, const imaging_focus_continuous_move_t *config)
{
    if (!builder || !config || !config->supported)
        return;

    xml_builder_append(builder, "<tt:Continuous>\n");
    append_float_range(builder, "Speed", &config->speed);
    xml_builder_append(builder, "</tt:Continuous>\n");
}

static void build_focus_move_options_xml(const imaging_entry_t *entry, char *buffer, size_t buffer_len)
{
    if (!entry || !buffer || buffer_len == 0)
        return;

    xml_builder_t builder;
    xml_builder_init(&builder, buffer, buffer_len);

    append_focus_move_absolute_options(&builder, &entry->focus_move.absolute);
    append_focus_move_relative_options(&builder, &entry->focus_move.relative);
    append_focus_move_continuous_options(&builder, &entry->focus_move.continuous);
}

static void build_focus_status_xml(const imaging_entry_t *entry, char *buffer, size_t buffer_len)
{
    if (!entry || !buffer || buffer_len == 0)
        return;

    xml_builder_t builder;
    xml_builder_init(&builder, buffer, buffer_len);

    imaging_focus_state_t state = entry->focus_state;
    if (state == IMAGING_FOCUS_STATE_UNKNOWN)
        state = IMAGING_FOCUS_STATE_IDLE;

    float position = 0.0f;
    if (entry->focus_has_last_position)
        position = entry->focus_last_position;

    xml_builder_append(&builder, "<tt:FocusStatus20>\n");
    xml_builder_append(&builder, "<tt:Position>%.6f</tt:Position>\n", position);
    xml_builder_append(&builder, "<tt:MoveStatus>%s</tt:MoveStatus>\n", focus_move_status_to_string(state));
    xml_builder_append(&builder, "</tt:FocusStatus20>\n");
}

static int read_child_float(mxml_node_t *parent, const char *tag, float *out_value)
{
    if (!parent || !tag || !out_value)
        return 0;

    const char *text = get_element_in_element(tag, parent);
    if (!text)
        return 0;

    return parse_float_text(text, out_value);
}

static float resolve_focus_speed(const imaging_entry_t *entry, const imaging_float_value_t *move_speed, int has_requested, float requested_value)
{
    if (has_requested)
        return requested_value;
    if (move_speed && move_speed->has_value)
        return move_speed->value;
    if (entry && entry->focus.default_speed.has_value)
        return entry->focus.default_speed.value;
    return 1.0f;
}

static void set_focus_position(imaging_entry_t *entry, float position)
{
    if (!entry)
        return;
    entry->focus_last_position = position;
    entry->focus_has_last_position = 1;
}

static void apply_focus_delta(imaging_entry_t *entry, float delta)
{
    if (!entry)
        return;

    if (entry->focus_has_last_position)
        entry->focus_last_position += delta;
    else
        entry->focus_last_position = delta;

    entry->focus_has_last_position = 1;
}

static void send_focus_invalid_value_fault(const char *reason, const char *detail)
{
    send_fault("imaging_service", "Sender", "ter:InvalidArgVal", "ter:ConfigModify", (char *) reason, (char *) detail);
}

static void send_focus_not_supported_fault(const char *detail)
{
    send_fault("imaging_service", "Receiver", "ter:ActionNotSupported", "ter:ActionNotSupported", "Focus move unsupported", (char *) detail);
}

static void send_preset_not_supported_fault(const char *detail)
{
    send_fault("imaging_service", "Receiver", "ter:ActionNotSupported", "ter:ActionNotSupported", "Imaging presets unsupported", (char *) detail);
}

static void send_preset_invalid_fault(const char *detail)
{
    send_fault("imaging_service", "Sender", "ter:InvalidArgVal", "ter:NoSource", "Invalid imaging preset token", (char *) detail);
}

static imaging_preset_entry_t *find_preset(imaging_entry_t *entry, const char *token)
{
    if (!entry || !token || !entry->presets)
        return NULL;

    for (int i = 0; i < entry->preset_count; ++i) {
        imaging_preset_entry_t *preset = &entry->presets[i];
        if (preset->token && strcasecmp(preset->token, token) == 0)
            return preset;
    }

    return NULL;
}

static void append_preset_element(xml_builder_t *builder, const imaging_preset_entry_t *preset)
{
    if (!builder || !preset || !preset->token)
        return;

    const char *name = (preset->name && preset->name[0] != '\0') ? preset->name : preset->token;
    const char *type = (preset->type && preset->type[0] != '\0') ? preset->type : "Custom";

    xml_builder_append(builder, "<timg:Preset token=\"%s\" type=\"%s\">\n<tt:Name>%s</tt:Name>\n</timg:Preset>\n", preset->token, type, name);
}

static void build_imaging_presets_xml(const imaging_entry_t *entry, char *buffer, size_t buffer_len)
{
    if (!entry || !buffer || buffer_len == 0)
        return;

    xml_builder_t builder;
    xml_builder_init(&builder, buffer, buffer_len);

    if (!entry->presets)
        return;

    for (int i = 0; i < entry->preset_count; ++i)
        append_preset_element(&builder, &entry->presets[i]);
}

static void build_current_preset_xml(const imaging_entry_t *entry, char *buffer, size_t buffer_len)
{
    if (!entry || !buffer || buffer_len == 0)
        return;

    xml_builder_t builder;
    xml_builder_init(&builder, buffer, buffer_len);

    if (!entry->current_preset_token)
        return;

    imaging_preset_entry_t *preset = find_preset((imaging_entry_t *) entry, entry->current_preset_token);
    if (!preset)
        return;

    append_preset_element(&builder, preset);
}

static int execute_preset_command(const imaging_entry_t *entry, const imaging_preset_entry_t *preset)
{
    if (!entry || !preset)
        return -1;

    const char *template = NULL;
    if (preset->command && preset->command[0] != '\0')
        template = preset->command;
    else if (entry->cmd_apply_preset && entry->cmd_apply_preset[0] != '\0')
        template = entry->cmd_apply_preset;

    if (!template)
        return -1;

    const char *token = preset->token ? preset->token : "";
    const char *name = (preset->name && preset->name[0] != '\0') ? preset->name : token;
    const char *type = (preset->type && preset->type[0] != '\0') ? preset->type : "Custom";

    char command[IMAGING_COMMAND_BUFFER];
    int written = snprintf(command, sizeof(command), template, token, name, type);
    if (written < 0 || (size_t) written >= sizeof(command)) {
        log_error("Imaging preset command template overflow for token %s", token);
        return -1;
    }

    execute_backend_command(command);
    return 0;
}

static void set_current_preset(imaging_entry_t *entry, const char *token)
{
    if (!entry)
        return;

    if (entry->current_preset_token)
        free(entry->current_preset_token);

    entry->current_preset_token = token ? dup_string(token) : NULL;
}

static int perform_absolute_focus_move(imaging_entry_t *entry, mxml_node_t *absolute_node)
{
    if (!entry->focus_move.absolute.supported) {
        send_focus_not_supported_fault("Absolute focus moves are not configured for this source");
        return -1;
    }

    float position = 0.0f;
    if (!read_child_float(absolute_node, "Position", &position)) {
        send_focus_invalid_value_fault("Missing focus position", "Absolute focus moves require the Position element");
        return -1;
    }

    if (!value_within_range(&entry->focus_move.absolute.position, position)) {
        send_focus_invalid_value_fault("Position out of range", "Requested focus position is outside the configured range");
        return -1;
    }

    float requested_speed = 0.0f;
    int has_speed = read_child_float(absolute_node, "Speed", &requested_speed);
    if (has_speed && !value_within_range(&entry->focus_move.absolute.speed, requested_speed)) {
        send_focus_invalid_value_fault("Speed out of range", "Requested focus speed is outside the configured range");
        return -1;
    }

    float speed = resolve_focus_speed(entry, &entry->focus_move.absolute.speed, has_speed, requested_speed);
    if (!value_within_range(&entry->focus_move.absolute.speed, speed)) {
        send_focus_invalid_value_fault("Speed unavailable", "No valid focus speed could be resolved for the absolute move");
        return -1;
    }

    entry->focus_state = IMAGING_FOCUS_STATE_MOVING;
    if (execute_formatted_command(entry->focus_move.absolute.command, position, speed) != 0) {
        entry->focus_state = IMAGING_FOCUS_STATE_IDLE;
        send_focus_invalid_value_fault("Focus command failed", "Failed to build absolute focus command");
        return -1;
    }

    entry->focus_state = IMAGING_FOCUS_STATE_IDLE;
    set_focus_position(entry, position);
    return 0;
}

static int perform_relative_focus_move(imaging_entry_t *entry, mxml_node_t *relative_node)
{
    if (!entry->focus_move.relative.supported) {
        send_focus_not_supported_fault("Relative focus moves are not configured for this source");
        return -1;
    }

    float distance = 0.0f;
    if (!read_child_float(relative_node, "Distance", &distance)) {
        send_focus_invalid_value_fault("Missing focus distance", "Relative focus moves require the Distance element");
        return -1;
    }

    if (!value_within_range(&entry->focus_move.relative.distance, distance)) {
        send_focus_invalid_value_fault("Distance out of range", "Requested focus distance is outside the configured range");
        return -1;
    }

    float requested_speed = 0.0f;
    int has_speed = read_child_float(relative_node, "Speed", &requested_speed);
    if (has_speed && !value_within_range(&entry->focus_move.relative.speed, requested_speed)) {
        send_focus_invalid_value_fault("Speed out of range", "Requested focus speed is outside the configured range");
        return -1;
    }

    float speed = resolve_focus_speed(entry, &entry->focus_move.relative.speed, has_speed, requested_speed);
    if (!value_within_range(&entry->focus_move.relative.speed, speed)) {
        send_focus_invalid_value_fault("Speed unavailable", "No valid focus speed could be resolved for the relative move");
        return -1;
    }

    entry->focus_state = IMAGING_FOCUS_STATE_MOVING;
    if (execute_formatted_command(entry->focus_move.relative.command, distance, speed) != 0) {
        entry->focus_state = IMAGING_FOCUS_STATE_IDLE;
        send_focus_invalid_value_fault("Focus command failed", "Failed to build relative focus command");
        return -1;
    }

    entry->focus_state = IMAGING_FOCUS_STATE_IDLE;
    apply_focus_delta(entry, distance);
    return 0;
}

static int perform_continuous_focus_move(imaging_entry_t *entry, mxml_node_t *continuous_node)
{
    if (!entry->focus_move.continuous.supported) {
        send_focus_not_supported_fault("Continuous focus moves are not configured for this source");
        return -1;
    }

    float speed = 0.0f;
    if (!read_child_float(continuous_node, "Speed", &speed)) {
        send_focus_invalid_value_fault("Missing focus speed", "Continuous focus moves require the Speed element");
        return -1;
    }

    if (!value_within_range(&entry->focus_move.continuous.speed, speed)) {
        send_focus_invalid_value_fault("Speed out of range", "Requested focus speed is outside the configured range");
        return -1;
    }

    entry->focus_state = IMAGING_FOCUS_STATE_MOVING;
    if (execute_formatted_command(entry->focus_move.continuous.command, speed, 0.0f) != 0) {
        entry->focus_state = IMAGING_FOCUS_STATE_IDLE;
        send_focus_invalid_value_fault("Focus command failed", "Failed to build continuous focus command");
        return -1;
    }

    entry->focus_state = IMAGING_FOCUS_STATE_IDLE;
    return 0;
}

static void build_imaging_settings_xml(const imaging_entry_t *entry, char *buffer, size_t buffer_len)
{
    if (!buffer || buffer_len == 0)
        return;

    xml_builder_t builder;
    xml_builder_init(&builder, buffer, buffer_len);

    if (!entry)
        return;

    const char *token = entry->video_source_token ? entry->video_source_token : "VideoSourceToken";
    xml_builder_append(&builder, "<tt:VideoSourceToken>%s</tt:VideoSourceToken>\n", token);

    append_mode_level_setting(&builder, "BacklightCompensation", &entry->backlight);
    append_float_element(&builder, "Brightness", &entry->brightness);
    append_float_element(&builder, "ColorSaturation", &entry->color_saturation);
    append_float_element(&builder, "Contrast", &entry->contrast);
    append_exposure_settings(&builder, &entry->exposure);
    append_focus_settings(&builder, &entry->focus);
    xml_builder_append(&builder, "<tt:IrCutFilter>%s</tt:IrCutFilter>\n", ircut_mode_to_string(entry->ircut_mode));
    append_float_element(&builder, "Sharpness", &entry->sharpness);
    append_mode_level_setting(&builder, "WideDynamicRange", &entry->wide_dynamic_range);
    append_white_balance_settings(&builder, &entry->white_balance);
    append_ircut_auto_adjustment_settings(&builder, &entry->ircut_auto_adjustment);
    append_mode_level_setting(&builder, "ImageStabilization", &entry->image_stabilization);
    append_mode_level_setting(&builder, "ToneCompensation", &entry->tone_compensation);
    append_mode_level_setting(&builder, "Defogging", &entry->defogging);
    append_float_element(&builder, "NoiseReduction", &entry->noise_reduction);
}

static void build_imaging_options_xml(const imaging_entry_t *entry, char *buffer, size_t buffer_len)
{
    if (!buffer || buffer_len == 0)
        return;

    xml_builder_t builder;
    xml_builder_init(&builder, buffer, buffer_len);

    if (!entry)
        return;

    const char *token = entry->video_source_token ? entry->video_source_token : "VideoSourceToken";
    xml_builder_append(&builder, "<tt:VideoSourceToken>%s</tt:VideoSourceToken>\n", token);

    append_mode_level_options(&builder, "BacklightCompensationOptions", &entry->backlight);
    append_float_range(&builder, "Brightness", &entry->brightness);
    append_float_range(&builder, "ColorSaturation", &entry->color_saturation);
    append_float_range(&builder, "Contrast", &entry->contrast);
    append_exposure_options(&builder, &entry->exposure);
    append_focus_options(&builder, &entry->focus);
    append_ircut_modes(&builder, entry);
    append_float_range(&builder, "Sharpness", &entry->sharpness);
    append_mode_level_options(&builder, "WideDynamicRangeOptions", &entry->wide_dynamic_range);
    append_white_balance_options(&builder, &entry->white_balance);
    append_ircut_auto_adjustment_options(&builder, &entry->ircut_auto_adjustment);
    append_mode_level_options(&builder, "ImageStabilizationOptions", &entry->image_stabilization);
    append_mode_level_options(&builder, "ToneCompensationOptions", &entry->tone_compensation);
    append_mode_level_options(&builder, "DefoggingOptions", &entry->defogging);
    append_float_range(&builder, "NoiseReduction", &entry->noise_reduction);
}

static void execute_backend_command(const char *command)
{
    if (command == NULL || command[0] == '\0')
        return;

    int ret = system(command);
    if (ret == -1) {
        log_error("Imaging command '%s' failed: %s", command, strerror(errno));
    } else {
        log_debug("Imaging command '%s' executed (rc=%d)", command, ret);
    }
}

static const char *find_ircut_value(mxml_node_t *node)
{
    return find_child_value(node, "IrCutFilter");
}

static const char *find_mode_level_value(mxml_node_t *settings, const char *container_tag)
{
    if (!settings || !container_tag)
        return NULL;

    mxml_node_t *container = get_element_in_element_ptr(container_tag, settings);
    if (!container)
        return NULL;

    return get_element_in_element("Level", container);
}

static int ensure_imaging_available(const char *method)
{
    (void) method;
    if (service_ctx.imaging_num == 0) {
        send_fault("imaging_service",
                   "Receiver",
                   "ter:ActionNotSupported",
                   "ter:NoImagingForSource",
                   "Imaging service disabled",
                   "This device is not configured with any imaging sources");
        return -1;
    }
    return 0;
}

int imaging_get_service_capabilities()
{
    if (ensure_imaging_available("GetServiceCapabilities") < 0)
        return -1;

    int has_stabilization = 0;
    int has_presets = 0;
    int has_adaptable = 0;

    for (int i = 0; i < service_ctx.imaging_num; ++i) {
        imaging_entry_t *entry = &service_ctx.imaging[i];
        if (entry->image_stabilization.present)
            has_stabilization = 1;
        if (entry->preset_count > 0 && entry->presets)
            has_presets = 1;
        if (entry->cmd_apply_preset && entry->cmd_apply_preset[0] != '\0')
            has_adaptable = 1;
        else if (entry->presets) {
            for (int j = 0; j < entry->preset_count; ++j) {
                if (entry->presets[j].command && entry->presets[j].command[0] != '\0') {
                    has_adaptable = 1;
                    break;
                }
            }
        }
    }

    const char *stab_str = has_stabilization ? "true" : "false";
    const char *presets_str = has_presets ? "true" : "false";
    const char *adaptable_str = has_adaptable ? "true" : "false";

    long size = cat(NULL,
                    "imaging_service_files/GetServiceCapabilities.xml",
                    6,
                    "%IMAGE_STABILIZATION%",
                    stab_str,
                    "%IMAGING_PRESETS%",
                    presets_str,
                    "%ADAPTABLE_PRESET%",
                    adaptable_str);

    output_http_headers(size);
    return cat("stdout",
               "imaging_service_files/GetServiceCapabilities.xml",
               6,
               "%IMAGE_STABILIZATION%",
               stab_str,
               "%IMAGING_PRESETS%",
               presets_str,
               "%ADAPTABLE_PRESET%",
               adaptable_str);
}

int imaging_get_imaging_settings()
{
    if (ensure_imaging_available("GetImagingSettings") < 0)
        return -1;

    const char *token = get_element("VideoSourceToken", "Body");
    imaging_entry_t *entry = find_imaging_entry(token);
    if (entry == NULL) {
        send_fault("imaging_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoSource",
                   "Unknown video source",
                   "The requested VideoSourceToken does not exist");
        return -1;
    }

    imaging_entry_t runtime_entry = *entry;
    prudynt_imaging_state_t runtime_state;
    if (prudynt_load_imaging_state(&runtime_state) == 0) {
        merge_prudynt_state(&runtime_entry, &runtime_state);
        entry = &runtime_entry;
    }

    char settings_xml[IMAGING_XML_BUFFER];
    build_imaging_settings_xml(entry, settings_xml, sizeof(settings_xml));

    long size = cat(NULL, "imaging_service_files/GetImagingSettings.xml", 2, "%IMAGING_SETTINGS%", settings_xml);

    output_http_headers(size);
    return cat("stdout", "imaging_service_files/GetImagingSettings.xml", 2, "%IMAGING_SETTINGS%", settings_xml);
}

int imaging_get_options()
{
    if (ensure_imaging_available("GetOptions") < 0)
        return -1;

    const char *token = get_element("VideoSourceToken", "Body");
    imaging_entry_t *entry = find_imaging_entry(token);
    if (entry == NULL) {
        send_fault("imaging_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoSource",
                   "Unknown video source",
                   "The requested VideoSourceToken does not exist");
        return -1;
    }

    imaging_entry_t runtime_entry = *entry;
    prudynt_imaging_state_t runtime_state;
    if (prudynt_load_imaging_state(&runtime_state) == 0) {
        merge_prudynt_state(&runtime_entry, &runtime_state);
        entry = &runtime_entry;
    }

    char options_xml[IMAGING_XML_BUFFER];
    build_imaging_options_xml(entry, options_xml, sizeof(options_xml));

    long size = cat(NULL, "imaging_service_files/GetOptions.xml", 2, "%IMAGING_OPTIONS%", options_xml);

    output_http_headers(size);
    return cat("stdout", "imaging_service_files/GetOptions.xml", 2, "%IMAGING_OPTIONS%", options_xml);
}

int imaging_set_imaging_settings()
{
    if (ensure_imaging_available("SetImagingSettings") < 0)
        return -1;

    const char *token = get_element("VideoSourceToken", "Body");
    imaging_entry_t *entry = find_imaging_entry(token);
    if (entry == NULL) {
        send_fault("imaging_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoSource",
                   "Unknown video source",
                   "The requested VideoSourceToken does not exist");
        return -1;
    }

    prudynt_imaging_state_t runtime_state;
    prudynt_imaging_state_t *state_ptr = NULL;
    if (prudynt_load_imaging_state(&runtime_state) == 0)
        state_ptr = &runtime_state;

    mxml_node_t *settings_node = get_element_ptr(NULL, "ImagingSettings", "Body");
    if (settings_node == NULL) {
        send_fault("imaging_service", "Sender", "ter:InvalidArgVal", "ter:NoSource", "Missing imaging settings", "ImagingSettings element is required");
        return -1;
    }

    const char *ircut = find_ircut_value(settings_node);
    if (ircut != NULL) {
        ircut_mode_t requested = ircut_mode_from_string(ircut);
        if (requested == IRCUT_MODE_UNSPECIFIED) {
            send_fault("imaging_service",
                       "Sender",
                       "ter:InvalidArgVal",
                       "ter:ConfigModify",
                       "Unsupported IrCutFilter",
                       "IrCutFilter value must be On, Off or Auto");
            return -1;
        }

        if ((requested == IRCUT_MODE_ON && !entry->supports_ircut_on) || (requested == IRCUT_MODE_OFF && !entry->supports_ircut_off)
            || (requested == IRCUT_MODE_AUTO && !entry->supports_ircut_auto)) {
            send_fault("imaging_service",
                       "Sender",
                       "ter:InvalidArgVal",
                       "ter:ConfigModify",
                       "Unsupported IrCutFilter",
                       "Requested IrCutFilter mode is not supported by this source");
            return -1;
        }

        if (requested != entry->ircut_mode) {
            switch (requested) {
            case IRCUT_MODE_ON:
                execute_backend_command(entry->cmd_ircut_on);
                break;
            case IRCUT_MODE_OFF:
                execute_backend_command(entry->cmd_ircut_off);
                break;
            case IRCUT_MODE_AUTO:
                execute_backend_command(entry->cmd_ircut_auto);
                break;
            default:
                break;
            }
            entry->ircut_mode = requested;
        }
    }

    static const struct {
        const char *tag;
        const char *key;
    } prudynt_map[] = {
        {"Brightness", "brightness"},
        {"ColorSaturation", "saturation"},
        {"Contrast", "contrast"},
        {"Sharpness", "sharpness"},
        {"NoiseReduction", "noise_reduction"},
    };

    prudynt_command_t commands[sizeof(prudynt_map) / sizeof(prudynt_map[0])];
    size_t command_count = 0;
    for (size_t i = 0; i < sizeof(prudynt_map) / sizeof(prudynt_map[0]); ++i) {
        const char *value = find_child_value(settings_node, prudynt_map[i].tag);
        if (!value)
            continue;
        float parsed_value;
        int is_normalized = 0;
        if (!parse_imaging_value(value, &parsed_value, &is_normalized))
            continue;
        commands[command_count].key = prudynt_map[i].key;
        commands[command_count].value = normalize_with_state(prudynt_map[i].key, parsed_value, is_normalized, state_ptr);
        command_count++;
    }

    static const struct {
        const char *container_tag;
        const char *key;
    } prudynt_mode_level_map[] = {
        {"BacklightCompensation", "backlight"},
        {"WideDynamicRange", "wide_dynamic_range"},
        {"ToneCompensation", "tone"},
        {"Defogging", "defog"},
    };

    for (size_t i = 0; i < sizeof(prudynt_mode_level_map) / sizeof(prudynt_mode_level_map[0]); ++i) {
        const char *level = find_mode_level_value(settings_node, prudynt_mode_level_map[i].container_tag);
        if (!level)
            continue;
        float parsed_value;
        int is_normalized = 0;
        if (!parse_imaging_value(level, &parsed_value, &is_normalized))
            continue;
        commands[command_count].key = prudynt_mode_level_map[i].key;
        commands[command_count].value = normalize_with_state(prudynt_mode_level_map[i].key, parsed_value, is_normalized, state_ptr);
        command_count++;
    }

    if (command_count > 0) {
        if (prudynt_apply_imaging_changes(commands, command_count, 1500) != 0) {
            send_fault("imaging_service",
                       "Receiver",
                       "ter:Action",
                       "ter:ConfigModify",
                       "Failed to apply imaging parameters",
                       "Streamer rejected one or more imaging values");
            return -1;
        }
    }

    long size = cat(NULL, "imaging_service_files/SetImagingSettings.xml", 0);
    output_http_headers(size);
    return cat("stdout", "imaging_service_files/SetImagingSettings.xml", 0);
}

int imaging_move()
{
    if (ensure_imaging_available("Move") < 0)
        return -1;

    const char *token = get_element("VideoSourceToken", "Body");
    imaging_entry_t *entry = require_imaging_entry(token);
    if (!entry)
        return -1;

    if (!focus_move_capable(&entry->focus_move)) {
        send_focus_not_supported_fault("This video source does not expose any focus move commands");
        return -1;
    }

    mxml_node_t *focus_node = get_element_ptr(NULL, "Focus", "Body");
    if (!focus_node) {
        send_focus_invalid_value_fault("Missing Focus element", "Move requests must include the Focus container");
        return -1;
    }

    mxml_node_t *absolute_node = get_element_in_element_ptr("Absolute", focus_node);
    mxml_node_t *relative_node = get_element_in_element_ptr("Relative", focus_node);
    mxml_node_t *continuous_node = get_element_in_element_ptr("Continuous", focus_node);

    int present = (absolute_node != NULL) + (relative_node != NULL) + (continuous_node != NULL);
    if (present != 1) {
        send_focus_invalid_value_fault("Invalid focus move", "Exactly one of Absolute, Relative or Continuous must be provided");
        return -1;
    }

    int ret = -1;
    if (absolute_node)
        ret = perform_absolute_focus_move(entry, absolute_node);
    else if (relative_node)
        ret = perform_relative_focus_move(entry, relative_node);
    else if (continuous_node)
        ret = perform_continuous_focus_move(entry, continuous_node);

    if (ret != 0)
        return ret;

    long size = cat(NULL, "imaging_service_files/Move.xml", 0);
    output_http_headers(size);
    return cat("stdout", "imaging_service_files/Move.xml", 0);
}

int imaging_get_move_options()
{
    if (ensure_imaging_available("GetMoveOptions") < 0)
        return -1;

    const char *token = get_element("VideoSourceToken", "Body");
    imaging_entry_t *entry = require_imaging_entry(token);
    if (!entry)
        return -1;

    if (!focus_move_capable(&entry->focus_move)) {
        send_focus_not_supported_fault("No focus move options are configured for this source");
        return -1;
    }

    char options_xml[IMAGING_XML_BUFFER];
    build_focus_move_options_xml(entry, options_xml, sizeof(options_xml));
    if (options_xml[0] == '\0') {
        send_focus_not_supported_fault("Unable to build focus move options for this source");
        return -1;
    }

    long size = cat(NULL, "imaging_service_files/GetMoveOptions.xml", 2, "%MOVE_OPTIONS%", options_xml);
    output_http_headers(size);
    return cat("stdout", "imaging_service_files/GetMoveOptions.xml", 2, "%MOVE_OPTIONS%", options_xml);
}

int imaging_stop()
{
    if (ensure_imaging_available("Stop") < 0)
        return -1;

    const char *token = get_element("VideoSourceToken", "Body");
    imaging_entry_t *entry = require_imaging_entry(token);
    if (!entry)
        return -1;

    if (!entry->focus_move.cmd_stop || entry->focus_move.cmd_stop[0] == '\0') {
        send_focus_not_supported_fault("No focus stop command configured for this source");
        return -1;
    }

    execute_backend_command(entry->focus_move.cmd_stop);
    entry->focus_state = IMAGING_FOCUS_STATE_IDLE;

    long size = cat(NULL, "imaging_service_files/Stop.xml", 0);
    output_http_headers(size);
    return cat("stdout", "imaging_service_files/Stop.xml", 0);
}

int imaging_get_status()
{
    if (ensure_imaging_available("GetStatus") < 0)
        return -1;

    const char *token = get_element("VideoSourceToken", "Body");
    imaging_entry_t *entry = require_imaging_entry(token);
    if (!entry)
        return -1;

    char status_xml[IMAGING_XML_BUFFER];
    build_focus_status_xml(entry, status_xml, sizeof(status_xml));

    long size = cat(NULL, "imaging_service_files/GetStatus.xml", 2, "%IMAGING_STATUS%", status_xml);
    output_http_headers(size);
    return cat("stdout", "imaging_service_files/GetStatus.xml", 2, "%IMAGING_STATUS%", status_xml);
}

int imaging_get_presets()
{
    if (ensure_imaging_available("GetPresets") < 0)
        return -1;

    const char *token = get_element("VideoSourceToken", "Body");
    imaging_entry_t *entry = require_imaging_entry(token);
    if (!entry)
        return -1;

    if (entry->preset_count <= 0 || entry->presets == NULL) {
        send_preset_not_supported_fault("This video source does not define imaging presets");
        return -1;
    }

    char presets_xml[IMAGING_XML_BUFFER];
    build_imaging_presets_xml(entry, presets_xml, sizeof(presets_xml));
    if (presets_xml[0] == '\0') {
        send_preset_not_supported_fault("Failed to build imaging preset list for this source");
        return -1;
    }

    long size = cat(NULL, "imaging_service_files/GetPresets.xml", 2, "%IMAGING_PRESETS%", presets_xml);
    output_http_headers(size);
    return cat("stdout", "imaging_service_files/GetPresets.xml", 2, "%IMAGING_PRESETS%", presets_xml);
}

int imaging_get_current_preset()
{
    if (ensure_imaging_available("GetCurrentPreset") < 0)
        return -1;

    const char *token = get_element("VideoSourceToken", "Body");
    imaging_entry_t *entry = require_imaging_entry(token);
    if (!entry)
        return -1;

    char preset_xml[IMAGING_XML_BUFFER];
    build_current_preset_xml(entry, preset_xml, sizeof(preset_xml));

    long size = cat(NULL, "imaging_service_files/GetCurrentPreset.xml", 2, "%CURRENT_PRESET%", preset_xml);
    output_http_headers(size);
    return cat("stdout", "imaging_service_files/GetCurrentPreset.xml", 2, "%CURRENT_PRESET%", preset_xml);
}

int imaging_set_current_preset()
{
    if (ensure_imaging_available("SetCurrentPreset") < 0)
        return -1;

    const char *token = get_element("VideoSourceToken", "Body");
    imaging_entry_t *entry = require_imaging_entry(token);
    if (!entry)
        return -1;

    if (entry->preset_count <= 0 || entry->presets == NULL) {
        send_preset_not_supported_fault("This video source does not define imaging presets");
        return -1;
    }

    const char *preset_token = get_element("PresetToken", "Body");
    if (!preset_token || preset_token[0] == '\0') {
        send_preset_invalid_fault("PresetToken element is required");
        return -1;
    }

    imaging_preset_entry_t *preset = find_preset(entry, preset_token);
    if (!preset) {
        send_preset_invalid_fault("Requested Imaging Preset token is not available for this source");
        return -1;
    }

    if (execute_preset_command(entry, preset) != 0) {
        send_preset_not_supported_fault("Failed to execute backend command for the requested preset");
        return -1;
    }

    set_current_preset(entry, preset->token);

    long size = cat(NULL, "imaging_service_files/SetCurrentPreset.xml", 0);
    output_http_headers(size);
    return cat("stdout", "imaging_service_files/SetCurrentPreset.xml", 0);
}

int imaging_unsupported(const char *method)
{
    if (service_ctx.adv_fault_if_unknown == 1)
        send_action_failed_fault("imaging_service", -1);
    else
        send_empty_response("timg", (char *) method);
    return -1;
}
