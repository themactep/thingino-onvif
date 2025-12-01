#pragma once

#include <stddef.h>

typedef struct {
    int present;
    float value; // Raw value as reported by prudynt (e.g. 0-255 scale)
    float min;
    float max;
} prudynt_field_state_t;

typedef struct {
    prudynt_field_state_t brightness;
    prudynt_field_state_t contrast;
    prudynt_field_state_t saturation;
    prudynt_field_state_t sharpness;
    prudynt_field_state_t backlight;
    prudynt_field_state_t wide_dynamic_range;
    prudynt_field_state_t tone;
    prudynt_field_state_t defog;
    prudynt_field_state_t noise_reduction;
} prudynt_imaging_state_t;

typedef struct {
    const char *key;
    float value;
} prudynt_command_t;

int prudynt_load_imaging_state(prudynt_imaging_state_t *state);
int prudynt_apply_imaging_changes(const prudynt_command_t *commands, size_t command_count, int timeout_ms);
