#include "prudynt_bridge.h"

#include "log.h"

#include <errno.h>
#include <fcntl.h>
#include <json_config.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PRUDYNT_STATE_PATH "/run/prudynt/imaging.json"
#define PRUDYNT_FIFO_PATH "/run/prudynt/imagingctl"
#define PRUDYNT_WAIT_INTERVAL_MS 100
#define PRUDYNT_DEFAULT_TIMEOUT_MS 1200
#define PRUDYNT_APPLY_TOLERANCE 0.02f

static void reset_state(prudynt_imaging_state_t *state)
{
    if (state)
        memset(state, 0, sizeof(*state));
}

static void parse_field(JsonValue *fields, const char *name, prudynt_field_state_t *out)
{
    if (!fields || !name || !out)
        return;

    JsonValue *node = get_object_item(fields, name);
    if (!node || node->type != JSON_OBJECT)
        return;

    JsonValue *value = get_object_item(node, "value");
    JsonValue *min = get_object_item(node, "min");
    JsonValue *max = get_object_item(node, "max");
    if (!value || !min || !max)
        return;
    if (value->type != JSON_NUMBER || min->type != JSON_NUMBER || max->type != JSON_NUMBER)
        return;

    double raw_min = min->value.number;
    double raw_max = max->value.number;
    if (raw_max <= raw_min)
        return;

    double raw_value = value->value.number;

    out->present = 1;
    out->value = (float) raw_value;
    out->min = (float) raw_min;
    out->max = (float) raw_max;
}

static float field_normalized_value(const prudynt_field_state_t *field)
{
    if (!field || !field->present)
        return -1.0f;

    float span = field->max - field->min;
    if (span <= 0.0f)
        return -1.0f;

    return (field->value - field->min) / span;
}

int prudynt_load_imaging_state(prudynt_imaging_state_t *state)
{
    if (!state)
        return -1;

    reset_state(state);

    JsonValue *doc = load_config(PRUDYNT_STATE_PATH);
    if (!doc)
        return -1;

    JsonValue *fields = get_object_item(doc, "fields");
    if (fields && fields->type == JSON_OBJECT) {
        parse_field(fields, "brightness", &state->brightness);
        parse_field(fields, "contrast", &state->contrast);
        parse_field(fields, "saturation", &state->saturation);
        parse_field(fields, "sharpness", &state->sharpness);
        parse_field(fields, "backlight", &state->backlight);
        parse_field(fields, "wide_dynamic_range", &state->wide_dynamic_range);
        parse_field(fields, "tone", &state->tone);
        parse_field(fields, "defog", &state->defog);
        parse_field(fields, "noise_reduction", &state->noise_reduction);
    }

    free_json_value(doc);

    if (!state->brightness.present && !state->contrast.present && !state->saturation.present && !state->sharpness.present && !state->backlight.present
        && !state->wide_dynamic_range.present && !state->tone.present && !state->defog.present && !state->noise_reduction.present)
        return -1;

    return 0;
}

static prudynt_field_state_t *lookup_field(prudynt_imaging_state_t *state, const char *key)
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

static int write_fifo_command(const char *payload, size_t len)
{
    int fd = open(PRUDYNT_FIFO_PATH, O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        log_error("prudynt_bridge: unable to open %s: %s", PRUDYNT_FIFO_PATH, strerror(errno));
        return -1;
    }

    ssize_t rc = write(fd, payload, len);
    close(fd);
    if (rc != (ssize_t) len) {
        log_error("prudynt_bridge: short write to %s", PRUDYNT_FIFO_PATH);
        return -1;
    }
    return 0;
}

int prudynt_apply_imaging_changes(const prudynt_command_t *commands, size_t command_count, int timeout_ms)
{
    if (!commands || command_count == 0)
        return 0;

    char buffer[256];
    int written = snprintf(buffer, sizeof(buffer), "SET");
    if (written < 0 || written >= (int) sizeof(buffer))
        return -1;

    for (size_t i = 0; i < command_count; ++i) {
        float clamped = commands[i].value;
        if (clamped < 0.0f)
            clamped = 0.0f;
        else if (clamped > 1.0f)
            clamped = 1.0f;
        int rc = snprintf(buffer + written, sizeof(buffer) - (size_t) written, " %s=%.4f", commands[i].key, clamped);
        if (rc < 0)
            return -1;
        written += rc;
        if (written >= (int) sizeof(buffer))
            return -1;
    }
    if (written + 1 >= (int) sizeof(buffer))
        return -1;
    buffer[written++] = '\n';

    if (write_fifo_command(buffer, (size_t) written) != 0)
        return -1;

    int wait_limit = timeout_ms > 0 ? timeout_ms : PRUDYNT_DEFAULT_TIMEOUT_MS;
    int elapsed = 0;

    while (elapsed <= wait_limit) {
        prudynt_imaging_state_t state;
        if (prudynt_load_imaging_state(&state) == 0) {
            int satisfied = 1;
            for (size_t i = 0; i < command_count && satisfied; ++i) {
                float clamped = commands[i].value;
                if (clamped < 0.0f)
                    clamped = 0.0f;
                else if (clamped > 1.0f)
                    clamped = 1.0f;
                prudynt_field_state_t *field = lookup_field(&state, commands[i].key);
                float normalized = field_normalized_value(field);
                if (normalized < 0.0f || fabsf(normalized - clamped) > PRUDYNT_APPLY_TOLERANCE)
                    satisfied = 0;
            }
            if (satisfied)
                return 0;
        }
        usleep(PRUDYNT_WAIT_INTERVAL_MS * 1000);
        elapsed += PRUDYNT_WAIT_INTERVAL_MS;
    }

    return -1;
}
