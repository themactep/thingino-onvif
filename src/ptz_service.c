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

#include "ptz_service.h"

#include "conf.h"
#include "fault.h"
#include "log.h"
#include "mxml_wrapper.h"
#include "onvif_simple_server.h"
#include "utils.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

extern service_context_t service_ctx;
presets_t presets;

#define PTZ_URI_PANTILT_ABS_GENERIC "http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionGenericSpace"
#define PTZ_URI_ZOOM_ABS_GENERIC "http://www.onvif.org/ver10/tptz/ZoomSpaces/PositionGenericSpace"
#define PTZ_URI_PANTILT_REL_GENERIC "http://www.onvif.org/ver10/tptz/PanTiltSpaces/TranslationGenericSpace"
#define PTZ_URI_ZOOM_REL_GENERIC "http://www.onvif.org/ver10/tptz/ZoomSpaces/TranslationGenericSpace"
#define PTZ_URI_PANTILT_VEL_GENERIC "http://www.onvif.org/ver10/tptz/PanTiltSpaces/VelocityGenericSpace"
#define PTZ_URI_ZOOM_VEL_GENERIC "http://www.onvif.org/ver10/tptz/ZoomSpaces/VelocityGenericSpace"
#define PTZ_NORMALIZED_TOLERANCE 0.01

static double clamp_double(double value, double min_v, double max_v)
{
    if (value < min_v)
        return min_v;
    if (value > max_v)
        return max_v;
    return value;
}

static double ptz_normalized_to_range(double normalized, double min_v, double max_v)
{
    if (max_v <= min_v)
        return min_v;
    double span = max_v - min_v;
    double mapped = min_v + ((normalized + 1.0) * 0.5 * span);
    return clamp_double(mapped, min_v, max_v);
}

static double ptz_range_to_normalized(double value, double min_v, double max_v)
{
    if (max_v <= min_v)
        return 0.0;
    double span = max_v - min_v;
    double normalized = ((value - min_v) / span) * 2.0 - 1.0;
    return clamp_double(normalized, -1.0, 1.0);
}

static double ptz_zoom_normalized_to_range(double normalized, double min_v, double max_v)
{
    if (normalized < 0.0)
        normalized = 0.0;
    if (normalized > 1.0)
        normalized = 1.0;
    if (max_v <= min_v)
        return min_v;
    double span = max_v - min_v;
    return clamp_double(min_v + normalized * span, min_v, max_v);
}

static double ptz_zoom_range_to_normalized(double value, double min_v, double max_v)
{
    if (max_v <= min_v)
        return 0.0;
    double span = max_v - min_v;
    double normalized = (value - min_v) / span;
    if (normalized < 0.0)
        normalized = 0.0;
    if (normalized > 1.0)
        normalized = 1.0;
    return normalized;
}

static double ptz_relative_normalized_to_delta(double normalized, double min_v, double max_v)
{
    double span = max_v - min_v;
    if (span <= 0.0)
        return 0.0;
    return normalized * span;
}

static double ptz_decode_absolute_normalized(const char *value_str, double min_v, double max_v)
{
    if (value_str == NULL)
        return 0.0;
    double value = atof(value_str);
    if (fabs(value) <= 1.0 + PTZ_NORMALIZED_TOLERANCE) {
        if (value < -1.0)
            value = -1.0;
        if (value > 1.0)
            value = 1.0;
        return value;
    }
    double clamped = clamp_double(value, min_v, max_v);
    return ptz_range_to_normalized(clamped, min_v, max_v);
}

static double ptz_decode_zoom_normalized(const char *value_str, double min_v, double max_v)
{
    if (value_str == NULL)
        return 0.0;
    double value = atof(value_str);
    if (value >= -PTZ_NORMALIZED_TOLERANCE && value <= 1.0 + PTZ_NORMALIZED_TOLERANCE) {
        if (value < 0.0)
            value = 0.0;
        if (value > 1.0)
            value = 1.0;
        return value;
    }
    double clamped = clamp_double(value, min_v, max_v);
    return ptz_zoom_range_to_normalized(clamped, min_v, max_v);
}

static double ptz_decode_relative_normalized(const char *value_str, double min_v, double max_v)
{
    if (value_str == NULL)
        return 0.0;
    double value = atof(value_str);
    if (fabs(value) <= 1.0 + PTZ_NORMALIZED_TOLERANCE) {
        if (value < -1.0)
            value = -1.0;
        if (value > 1.0)
            value = 1.0;
        return value;
    }
    double span = max_v - min_v;
    if (span <= 0.0)
        return 0.0;
    double normalized = value / span;
    if (normalized < -1.0)
        normalized = -1.0;
    if (normalized > 1.0)
        normalized = 1.0;
    return normalized;
}

static double ptz_decode_zoom_relative_normalized(const char *value_str, double min_v, double max_v)
{
    if (value_str == NULL)
        return 0.0;
    double value = atof(value_str);
    if (fabs(value) <= 1.0 + PTZ_NORMALIZED_TOLERANCE) {
        if (value < -1.0)
            value = -1.0;
        if (value > 1.0)
            value = 1.0;
        return value;
    }
    double span = max_v - min_v;
    if (span <= 0.0)
        return 0.0;
    double normalized = value / span;
    if (normalized < -1.0)
        normalized = -1.0;
    if (normalized > 1.0)
        normalized = 1.0;
    return normalized;
}

static void ptz_apply_reverse(double *pan_norm, double *tilt_norm)
{
    if (!service_ctx.ptz_node.reverse_mode_on)
        return;
    if (pan_norm)
        *pan_norm = -*pan_norm;
    if (tilt_norm)
        *tilt_norm = -*tilt_norm;
}

static int ptz_space_matches(const char *space_attr, const char *expected_uri)
{
    if (space_attr == NULL)
        return 1;
    return strcmp(space_attr, expected_uri) == 0;
}

int init_presets()
{
    FILE *fp;
    char out[MAX_LEN];
    int i, num;
    double x, y, z;
    char name[MAX_LEN];
    char *p;

    presets.count = 0;
    presets.items = (preset_t *) malloc(sizeof(preset_t));

    // Run command that returns to stdout the list of the presets in the form number=name,pan,tilt,zoom (zoom is optional)
    if (service_ctx.ptz_node.get_presets == NULL) {
        return -1;
    }
    fp = popen(service_ctx.ptz_node.get_presets, "r");
    if (fp == NULL) {
        return -2;
    } else {
        while (fgets(out, sizeof(out), fp)) {
            p = out;
            name[0] = '\0';
            x = -1.0;
            y = -1.0;
            z = 1.0;
            while ((p = strchr(p, ',')) != NULL) {
                *p++ = ' ';
            }
            if (sscanf(out, "%d=%s %lf %lf %lf", &num, name, &x, &y, &z) == 0) {
                pclose(fp);
                return -3;
            } else {
                if (strlen(name) != 0) {
                    presets.count++;
                    presets.items = (preset_t *) realloc(presets.items, sizeof(preset_t) * presets.count);
                    presets.items[presets.count - 1].name = (char *) malloc(strlen(name) + 1);
                    strcpy(presets.items[presets.count - 1].name, name);
                    presets.items[presets.count - 1].number = num;
                    presets.items[presets.count - 1].x = x;
                    presets.items[presets.count - 1].y = y;
                    presets.items[presets.count - 1].z = z;
                }
            }
        }
        pclose(fp);
    }

    for (i = 0; i < presets.count; i++) {
        if (presets.items[i].name != NULL) {
            log_debug("Preset %d - %d - %s - %f - %f", i, presets.items[i].number, presets.items[i].name, presets.items[i].x, presets.items[i].y);
        }
    }

    return 0;
}

void destroy_presets()
{
    int i;

    for (i = presets.count - 1; i >= 0; i--) {
        free(presets.items[i].name);
    }
    free(presets.items);
}

// ---- Preset Tours storage ----

typedef struct {
    char token[64];
    char name[64];
    char status[16]; // Idle/Touring/Paused
} preset_tour_t;

static preset_tour_t *g_tours = NULL;
static int g_tours_count = 0;
static int g_tours_loaded = 0;

static const char *preset_tours_file_path()
{
    return DEFAULT_CONF_DIR "/preset_tours.json";
}

static void tours_ensure_loaded()
{
    if (g_tours_loaded)
        return;
    g_tours_loaded = 1;
    FILE *f = fopen(preset_tours_file_path(), "r");
    if (!f)
        return;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0 || len > 1 << 20) {
        fclose(f);
        return;
    }
    char *buf = (char *) malloc(len + 1);
    if (!buf) {
        fclose(f);
        return;
    }
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);

    // Pass 1: TitleCase keys (Token/Name/Status)
    int before = g_tours_count;
    char *p = buf;
    while ((p = strstr(p, "\"Token\"")) != NULL) {
        char token[64] = {0}, name[64] = {0}, status[16] = {0};
        char *colon = strchr(p, ':');
        if (!colon)
            break;
        char *valq1 = strchr(colon, '"');
        if (!valq1)
            break;
        char *valq2 = strchr(valq1 + 1, '"');
        if (!valq2)
            break;
        size_t tlen = (size_t) (valq2 - (valq1 + 1));
        if (tlen >= sizeof(token))
            tlen = sizeof(token) - 1;
        strncpy(token, valq1 + 1, tlen);
        token[tlen] = '\0';
        // Name (optional)
        char *np = strstr(p, "\"Name\"");
        if (np) {
            char *ncolon = strchr(np, ':');
            char *nq1 = ncolon ? strchr(ncolon, '"') : NULL;
            char *nq2 = nq1 ? strchr(nq1 + 1, '"') : NULL;
            if (nq1 && nq2) {
                size_t nlen = (size_t) (nq2 - (nq1 + 1));
                if (nlen >= sizeof(name))
                    nlen = sizeof(name) - 1;
                strncpy(name, nq1 + 1, nlen);
                name[nlen] = '\0';
            }
        }
        // Status (optional)
        char *sp = strstr(p, "\"Status\"");
        if (sp) {
            char *scolon = strchr(sp, ':');
            char *sq1 = scolon ? strchr(scolon, '"') : NULL;
            char *sq2 = sq1 ? strchr(sq1 + 1, '"') : NULL;
            if (sq1 && sq2) {
                size_t slen = (size_t) (sq2 - (sq1 + 1));
                if (slen >= sizeof(status))
                    slen = sizeof(status) - 1;
                strncpy(status, sq1 + 1, slen);
                status[slen] = '\0';
            }
        }
        if (token[0]) {
            g_tours = (preset_tour_t *) realloc(g_tours, sizeof(preset_tour_t) * (g_tours_count + 1));
            memset(&g_tours[g_tours_count], 0, sizeof(preset_tour_t));
            strncpy(g_tours[g_tours_count].token, token, sizeof(g_tours[g_tours_count].token) - 1);
            if (name[0])
                strncpy(g_tours[g_tours_count].name, name, sizeof(g_tours[g_tours_count].name) - 1);
            strncpy(g_tours[g_tours_count].status, status[0] ? status : "Idle", sizeof(g_tours[g_tours_count].status) - 1);
            g_tours_count++;
        }
        p = valq2 + 1;
    }

    // Pass 2: lowercase keys (token/name/status) only if nothing loaded in pass 1
    if (g_tours_count == before) {
        p = buf;
        while ((p = strstr(p, "\"token\"")) != NULL) {
            char token[64] = {0}, name[64] = {0}, status[16] = {0};
            char *colon = strchr(p, ':');
            if (!colon)
                break;
            char *valq1 = strchr(colon, '"');
            if (!valq1)
                break;
            char *valq2 = strchr(valq1 + 1, '"');
            if (!valq2)
                break;
            size_t tlen = (size_t) (valq2 - (valq1 + 1));
            if (tlen >= sizeof(token))
                tlen = sizeof(token) - 1;
            strncpy(token, valq1 + 1, tlen);
            token[tlen] = '\0';
            // name (optional)
            char *np = strstr(p, "\"name\"");
            if (np) {
                char *ncolon = strchr(np, ':');
                char *nq1 = ncolon ? strchr(ncolon, '"') : NULL;
                char *nq2 = nq1 ? strchr(nq1 + 1, '"') : NULL;
                if (nq1 && nq2) {
                    size_t nlen = (size_t) (nq2 - (nq1 + 1));
                    if (nlen >= sizeof(name))
                        nlen = sizeof(name) - 1;
                    strncpy(name, nq1 + 1, nlen);
                    name[nlen] = '\0';
                }
            }
            // status (optional)
            char *sp = strstr(p, "\"status\"");
            if (sp) {
                char *scolon = strchr(sp, ':');
                char *sq1 = scolon ? strchr(scolon, '"') : NULL;
                char *sq2 = sq1 ? strchr(sq1 + 1, '"') : NULL;
                if (sq1 && sq2) {
                    size_t slen = (size_t) (sq2 - (sq1 + 1));
                    if (slen >= sizeof(status))
                        slen = sizeof(status) - 1;
                    strncpy(status, sq1 + 1, slen);
                    status[slen] = '\0';
                }
            }
            if (token[0]) {
                g_tours = (preset_tour_t *) realloc(g_tours, sizeof(preset_tour_t) * (g_tours_count + 1));
                memset(&g_tours[g_tours_count], 0, sizeof(preset_tour_t));
                strncpy(g_tours[g_tours_count].token, token, sizeof(g_tours[g_tours_count].token) - 1);
                if (name[0])
                    strncpy(g_tours[g_tours_count].name, name, sizeof(g_tours[g_tours_count].name) - 1);
                strncpy(g_tours[g_tours_count].status, status[0] ? status : "Idle", sizeof(g_tours[g_tours_count].status) - 1);
                g_tours_count++;
            }
            p = valq2 + 1;
        }
    }

    free(buf);
}

static void tours_save()
{
    // Ensure directory exists
    mkdir(DEFAULT_CONF_DIR, 0755);
    FILE *f = fopen(preset_tours_file_path(), "w");
    if (!f)
        return;
    fprintf(f, "{\n  \"preset_tours\": [\n");
    for (int i = 0; i < g_tours_count; i++) {
        fprintf(f,
                "    { \"token\": \"%s\", \"name\": \"%s\", \"status\": \"%s\" }%s\n",
                g_tours[i].token,
                g_tours[i].name,
                g_tours[i].status[0] ? g_tours[i].status : "Idle",
                (i == g_tours_count - 1) ? "" : ",");
    }
    fprintf(f, "  ]\n}\n");
    fclose(f);
}

static int tour_index_by_token(const char *token)
{
    for (int i = 0; i < g_tours_count; i++) {
        if (strcmp(g_tours[i].token, token) == 0)
            return i;
    }
    return -1;
}

int ptz_get_service_capabilities()
{
    char eflip_supported[8];
    char reverse_supported[8];
    char move_status[8], status_position[8], move_and_track[64];

    if (service_ctx.ptz_node.eflip_supported) {
        strcpy(eflip_supported, "true");
    } else {
        strcpy(eflip_supported, "false");
    }

    if (service_ctx.ptz_node.reverse_supported) {
        strcpy(reverse_supported, "true");
    } else {
        strcpy(reverse_supported, "false");
    }

    if (service_ctx.ptz_node.is_moving != NULL) {
        strcpy(move_status, "true");
    } else {
        strcpy(move_status, "false");
    }
    if (service_ctx.ptz_node.get_position != NULL) {
        strcpy(status_position, "true");
    } else {
        strcpy(status_position, "false");
    }
    if (service_ctx.ptz_node.start_tracking != NULL) {
        strcpy(move_and_track, "PresetToken PTZVector");
    } else {
        strcpy(move_and_track, "");
    }

    long size = cat(NULL,
                    "ptz_service_files/GetServiceCapabilities.xml",
                    10,
                    "%EFLIP_SUPPORTED%",
                    eflip_supported,
                    "%REVERSE_SUPPORTED%",
                    reverse_supported,
                    "%MOVE_STATUS%",
                    move_status,
                    "%STATUS_POSITION%",
                    status_position,
                    "%MOVE_AND_TRACK%",
                    move_and_track);

    output_http_headers(size);

    return cat("stdout",
               "ptz_service_files/GetServiceCapabilities.xml",
               10,
               "%EFLIP_SUPPORTED%",
               eflip_supported,
               "%REVERSE_SUPPORTED%",
               reverse_supported,
               "%MOVE_STATUS%",
               move_status,
               "%STATUS_POSITION%",
               status_position,
               "%MOVE_AND_TRACK%",
               move_and_track);
}

int ptz_get_configurations()
{
    char use_count[8];
    char pan_min[16];
    char pan_max[16];
    char tilt_min[16];
    char tilt_max[16];
    const char *zoom_min;
    const char *zoom_max;
    const char *eflip_mode = service_ctx.ptz_node.eflip_mode_on ? "ON" : "OFF";
    const char *reverse_mode = service_ctx.ptz_node.reverse_mode_on ? "ON" : "OFF";

    snprintf(pan_min, sizeof(pan_min), "%.4f", service_ctx.ptz_node.pan_min);
    snprintf(pan_max, sizeof(pan_max), "%.4f", service_ctx.ptz_node.pan_max);
    snprintf(tilt_min, sizeof(tilt_min), "%.4f", service_ctx.ptz_node.tilt_min);
    snprintf(tilt_max, sizeof(tilt_max), "%.4f", service_ctx.ptz_node.tilt_max);

    if (service_ctx.ptz_node.max_step_z > service_ctx.ptz_node.min_step_z) {
        zoom_min = "0.0";
        zoom_max = "1.0";
    } else {
        zoom_min = "0.0";
        zoom_max = "0.0";
    }

    // Calculate UseCount: number of profiles that use PTZ
    // If PTZ is enabled, UseCount equals the number of profiles
    if (service_ctx.ptz_node.enable == 1) {
        sprintf(use_count, "%d", service_ctx.profiles_num);
    } else {
        sprintf(use_count, "0");
    }

    long size = cat(NULL,
                    "ptz_service_files/GetConfigurations.xml",
                    14,
                    "%USE_COUNT%",
                    use_count,
                    "%MIN_X%",
                    pan_min,
                    "%MAX_X%",
                    pan_max,
                    "%MIN_Y%",
                    tilt_min,
                    "%MAX_Y%",
                    tilt_max,
                    "%MIN_Z%",
                    zoom_min,
                    "%MAX_Z%",
                    zoom_max,
                    "%EFLIP_MODE%",
                    eflip_mode,
                    "%REVERSE_MODE%",
                    reverse_mode);

    output_http_headers(size);

    return cat("stdout",
               "ptz_service_files/GetConfigurations.xml",
               18,
               "%USE_COUNT%",
               use_count,
               "%MIN_X%",
               pan_min,
               "%MAX_X%",
               pan_max,
               "%MIN_Y%",
               tilt_min,
               "%MAX_Y%",
               tilt_max,
               "%MIN_Z%",
               zoom_min,
               "%MAX_Z%",
               zoom_max,
               "%EFLIP_MODE%",
               eflip_mode,
               "%REVERSE_MODE%",
               reverse_mode);
}

int ptz_get_configuration()
{
    char pan_min[16];
    char pan_max[16];
    char tilt_min[16];
    char tilt_max[16];
    const char *zoom_min;
    const char *zoom_max;
    const char *eflip_mode = service_ctx.ptz_node.eflip_mode_on ? "ON" : "OFF";
    const char *reverse_mode = service_ctx.ptz_node.reverse_mode_on ? "ON" : "OFF";

    snprintf(pan_min, sizeof(pan_min), "%.4f", service_ctx.ptz_node.pan_min);
    snprintf(pan_max, sizeof(pan_max), "%.4f", service_ctx.ptz_node.pan_max);
    snprintf(tilt_min, sizeof(tilt_min), "%.4f", service_ctx.ptz_node.tilt_min);
    snprintf(tilt_max, sizeof(tilt_max), "%.4f", service_ctx.ptz_node.tilt_max);

    if (service_ctx.ptz_node.max_step_z > service_ctx.ptz_node.min_step_z) {
        zoom_min = "0.0";
        zoom_max = "1.0";
    } else {
        zoom_min = "0.0";
        zoom_max = "0.0";
    }

    long size = cat(NULL,
                    "ptz_service_files/GetConfiguration.xml",
                    16,
                    "%MIN_X%",
                    pan_min,
                    "%MAX_X%",
                    pan_max,
                    "%MIN_Y%",
                    tilt_min,
                    "%MAX_Y%",
                    tilt_max,
                    "%MIN_Z%",
                    zoom_min,
                    "%MAX_Z%",
                    zoom_max,
                    "%EFLIP_MODE%",
                    eflip_mode,
                    "%REVERSE_MODE%",
                    reverse_mode);

    output_http_headers(size);

    return cat("stdout",
               "ptz_service_files/GetConfiguration.xml",
               16,
               "%MIN_X%",
               pan_min,
               "%MAX_X%",
               pan_max,
               "%MIN_Y%",
               tilt_min,
               "%MAX_Y%",
               tilt_max,
               "%MIN_Z%",
               zoom_min,
               "%MAX_Z%",
               zoom_max,
               "%EFLIP_MODE%",
               eflip_mode,
               "%REVERSE_MODE%",
               reverse_mode);
}

int ptz_get_configuration_options()
{
    char pan_min[16];
    char pan_max[16];
    char tilt_min[16];
    char tilt_max[16];
    const char *zoom_min;
    const char *zoom_max;
    char eflip_modes[128];
    char reverse_modes[128];

    snprintf(pan_min, sizeof(pan_min), "%.4f", service_ctx.ptz_node.pan_min);
    snprintf(pan_max, sizeof(pan_max), "%.4f", service_ctx.ptz_node.pan_max);
    snprintf(tilt_min, sizeof(tilt_min), "%.4f", service_ctx.ptz_node.tilt_min);
    snprintf(tilt_max, sizeof(tilt_max), "%.4f", service_ctx.ptz_node.tilt_max);

    if (service_ctx.ptz_node.max_step_z > service_ctx.ptz_node.min_step_z) {
        zoom_min = "0.0";
        zoom_max = "1.0";
    } else {
        zoom_min = "0.0";
        zoom_max = "0.0";
    }

    if (service_ctx.ptz_node.eflip_supported) {
        strcpy(eflip_modes, "<tt:Mode>OFF</tt:Mode><tt:Mode>ON</tt:Mode>");
    } else {
        strcpy(eflip_modes, "<tt:Mode>OFF</tt:Mode>");
    }

    if (service_ctx.ptz_node.reverse_supported) {
        strcpy(reverse_modes, "<tt:Mode>OFF</tt:Mode><tt:Mode>ON</tt:Mode>");
    } else {
        strcpy(reverse_modes, "<tt:Mode>OFF</tt:Mode>");
    }

    long size = cat(NULL,
                    "ptz_service_files/GetConfigurationOptions.xml",
                    16,
                    "%MIN_X%",
                    pan_min,
                    "%MAX_X%",
                    pan_max,
                    "%MIN_Y%",
                    tilt_min,
                    "%MAX_Y%",
                    tilt_max,
                    "%MIN_Z%",
                    zoom_min,
                    "%MAX_Z%",
                    zoom_max,
                    "%EFLIP_MODES%",
                    eflip_modes,
                    "%REVERSE_MODES%",
                    reverse_modes);

    output_http_headers(size);

    return cat("stdout",
               "ptz_service_files/GetConfigurationOptions.xml",
               16,
               "%MIN_X%",
               pan_min,
               "%MAX_X%",
               pan_max,
               "%MIN_Y%",
               tilt_min,
               "%MAX_Y%",
               tilt_max,
               "%MIN_Z%",
               zoom_min,
               "%MAX_Z%",
               zoom_max,
               "%EFLIP_MODES%",
               eflip_modes,
               "%REVERSE_MODES%",
               reverse_modes);
}

int ptz_get_nodes()
{
    char pan_min[16];
    char pan_max[16];
    char tilt_min[16];
    char tilt_max[16];
    const char *zoom_min;
    const char *zoom_max;

    snprintf(pan_min, sizeof(pan_min), "%.4f", service_ctx.ptz_node.pan_min);
    snprintf(pan_max, sizeof(pan_max), "%.4f", service_ctx.ptz_node.pan_max);
    snprintf(tilt_min, sizeof(tilt_min), "%.4f", service_ctx.ptz_node.tilt_min);
    snprintf(tilt_max, sizeof(tilt_max), "%.4f", service_ctx.ptz_node.tilt_max);

    if (service_ctx.ptz_node.max_step_z > service_ctx.ptz_node.min_step_z) {
        zoom_min = "0.0";
        zoom_max = "1.0";
    } else {
        zoom_min = "0.0";
        zoom_max = "0.0";
    }

    char max_tours[16];
    sprintf(max_tours, "%d", service_ctx.ptz_node.max_preset_tours);

    long size = cat(NULL,
                    "ptz_service_files/GetNodes.xml",
                    14,
                    "%MIN_X%",
                    pan_min,
                    "%MAX_X%",
                    pan_max,
                    "%MIN_Y%",
                    tilt_min,
                    "%MAX_Y%",
                    tilt_max,
                    "%MIN_Z%",
                    zoom_min,
                    "%MAX_Z%",
                    zoom_max,
                    "%MAX_PRESET_TOURS%",
                    max_tours);

    output_http_headers(size);

    return cat("stdout",
               "ptz_service_files/GetNodes.xml",
               14,
               "%MIN_X%",
               pan_min,
               "%MAX_X%",
               pan_max,
               "%MIN_Y%",
               tilt_min,
               "%MAX_Y%",
               tilt_max,
               "%MIN_Z%",
               zoom_min,
               "%MAX_Z%",
               zoom_max,
               "%MAX_PRESET_TOURS%",
               max_tours);
}

int ptz_get_node()
{
    char pan_min[16];
    char pan_max[16];
    char tilt_min[16];
    char tilt_max[16];
    const char *zoom_min;
    const char *zoom_max;

    snprintf(pan_min, sizeof(pan_min), "%.4f", service_ctx.ptz_node.pan_min);
    snprintf(pan_max, sizeof(pan_max), "%.4f", service_ctx.ptz_node.pan_max);
    snprintf(tilt_min, sizeof(tilt_min), "%.4f", service_ctx.ptz_node.tilt_min);
    snprintf(tilt_max, sizeof(tilt_max), "%.4f", service_ctx.ptz_node.tilt_max);

    if (service_ctx.ptz_node.max_step_z > service_ctx.ptz_node.min_step_z) {
        zoom_min = "0.0";
        zoom_max = "1.0";
    } else {
        zoom_min = "0.0";
        zoom_max = "0.0";
    }

    const char *node_token = get_element("NodeToken", "Body");
    if (strcmp("PTZNodeToken", node_token) != 0) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoEntity", "No entity", "No such node on the device");
        return -1;
    }

    char max_tours[16];
    sprintf(max_tours, "%d", service_ctx.ptz_node.max_preset_tours);

    long size = cat(NULL,
                    "ptz_service_files/GetNode.xml",
                    14,
                    "%MIN_X%",
                    pan_min,
                    "%MAX_X%",
                    pan_max,
                    "%MIN_Y%",
                    tilt_min,
                    "%MAX_Y%",
                    tilt_max,
                    "%MIN_Z%",
                    zoom_min,
                    "%MAX_Z%",
                    zoom_max,
                    "%MAX_PRESET_TOURS%",
                    max_tours);

    output_http_headers(size);

    return cat("stdout",
               "ptz_service_files/GetNode.xml",
               14,
               "%MIN_X%",
               pan_min,
               "%MAX_X%",
               pan_max,
               "%MIN_Y%",
               tilt_min,
               "%MAX_Y%",
               tilt_max,
               "%MIN_Z%",
               zoom_min,
               "%MAX_Z%",
               zoom_max,
               "%MAX_PRESET_TOURS%",
               max_tours);
}

int ptz_get_presets()
{
    mxml_node_t *node;
    int i, c;
    char dest_a[] = "stdout";
    char *dest;
    char token[16];
    char sx[16], sy[16], sz[16];
    long size, total_size;

    node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoProfile",
                   "No profile",
                   "The requested profile token ProfileToken does not exist");
        return -1;
    }

    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoPTZProfile",
                   "No PTZ profile",
                   "The requested profile token does not reference a PTZ configuration");
        return -2;
    }

    init_presets();

    // We need 1st step to evaluate content length
    for (c = 0; c < 2; c++) {
        if (c == 0) {
            dest = NULL;
        } else {
            dest = dest_a;
            output_http_headers(total_size);
        }

        size = cat(dest, "ptz_service_files/GetPresets_1.xml", 0);
        if (c == 0)
            total_size = size;
        else
            fflush(stdout);

        for (i = 0; i < presets.count; i++) {
            sprintf(token, "PresetToken_%d", presets.items[i].number);
            double pan_norm = ptz_range_to_normalized(presets.items[i].x, service_ctx.ptz_node.min_step_x, service_ctx.ptz_node.max_step_x);
            double tilt_norm = ptz_range_to_normalized(presets.items[i].y, service_ctx.ptz_node.min_step_y, service_ctx.ptz_node.max_step_y);
            ptz_apply_reverse(&pan_norm, &tilt_norm);
            double zoom_norm = ptz_zoom_range_to_normalized(presets.items[i].z, service_ctx.ptz_node.min_step_z, service_ctx.ptz_node.max_step_z);
            snprintf(sx, sizeof(sx), "%.4f", pan_norm);
            snprintf(sy, sizeof(sy), "%.4f", tilt_norm);
            snprintf(sz, sizeof(sz), "%.4f", zoom_norm);
            size = cat(
                dest, "ptz_service_files/GetPresets_2.xml", 10, "%TOKEN%", token, "%NAME%", presets.items[i].name, "%X%", sx, "%Y%", sy, "%Z%", sz);
            if (c == 0)
                total_size += size;
            else
                fflush(stdout);
        }

        size = cat(dest, "ptz_service_files/GetPresets_3.xml", 0);
        if (c == 0)
            total_size += size;
        else
            fflush(stdout);
    }

    destroy_presets();

    return total_size;
}

int ptz_goto_preset()
{
    const char *preset_token;
    int i, preset_number, count, found;
    char sys_command[MAX_LEN];
    mxml_node_t *node;

    node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoProfile",
                   "No profile",
                   "The requested profile token ProfileToken does not exist");
        return -1;
    }

    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoPTZProfile",
                   "No PTZ profile",
                   "The requested profile token does not reference a PTZ configuration");
        return -2;
    }

    preset_token = get_element("PresetToken", "Body");
    if (sscanf(preset_token, "PresetToken_%d", &preset_number) != 1) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoToken", "No token", "The requested preset token does not exist");
        return -3;
    }

    init_presets();
    count = presets.count;
    found = 0;
    for (i = 0; i < presets.count; i++) {
        if (presets.items[i].number == preset_number) {
            found = 1;
            break;
        }
    }
    destroy_presets();

    if (found == 0) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoToken", "No token", "The requested preset token does not exist");
        return -4;
    }
    if (service_ctx.ptz_node.move_preset == NULL) {
        send_action_failed_fault("ptz_service", -5);
        return -5;
    }

    sprintf(sys_command, service_ctx.ptz_node.move_preset, preset_number);
    system(sys_command);
    long size = cat(NULL, "ptz_service_files/GotoPreset.xml", 0);

    output_http_headers(size);

    return cat("stdout", "ptz_service_files/GotoPreset.xml", 0);
}

int ptz_goto_home_position()
{
    mxml_node_t *node;

    node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoProfile",
                   "No profile",
                   "The requested profile token ProfileToken does not exist");
        return -1;
    }

    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoPTZProfile",
                   "No PTZ profile",
                   "The requested profile token does not reference a PTZ configuration");
        return -2;
    }

    if (service_ctx.ptz_node.goto_home_position == NULL) {
        send_action_failed_fault("ptz_service", -3);
        return -3;
    }
    system(service_ctx.ptz_node.goto_home_position);

    long size = cat(NULL, "ptz_service_files/GotoHomePosition.xml", 0);

    output_http_headers(size);

    return cat("stdout", "ptz_service_files/GotoHomePosition.xml", 0);
}

int ptz_continuous_move()
{
    const char *x = NULL;
    const char *y = NULL;
    const char *z = NULL;
    double dx = 0.0, dy = 0.0, dz = 0.0;
    int pantilt_present = 0;
    int zoom_present = 0;
    char sys_command[MAX_LEN];
    int ret = -1;
    mxml_node_t *node;

    log_debug("PTZ: ContinuousMove called");

    node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoProfile",
                   "No profile",
                   "The requested profile token ProfileToken does not exist");
        return -1;
    }

    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoPTZProfile",
                   "No PTZ profile",
                   "The requested profile token does not reference a PTZ configuration");
        return -2;
    }

    mxml_node_t *velocity_node = get_element_ptr(NULL, "Velocity", "Body");
    log_debug("PTZ: Velocity node: %p", velocity_node);
    if (velocity_node != NULL) {
        // Debug: List all children of Velocity node
        mxml_node_t *child = mxmlGetFirstChild(velocity_node);
        while (child) {
            if (mxmlGetType(child) == MXML_TYPE_ELEMENT) {
                const char *child_name = mxmlGetElement(child);
                log_debug("PTZ: Velocity child element: %s", child_name ? child_name : "NULL");
            }
            child = mxmlGetNextSibling(child);
        }

        mxml_node_t *pantilt_node = get_element_ptr(velocity_node, "PanTilt", NULL);
        log_debug("PTZ: PanTilt node: %p", pantilt_node);
        if (pantilt_node != NULL) {
            const char *space_attr = get_attribute(pantilt_node, "space");
            if (!ptz_space_matches(space_attr, PTZ_URI_PANTILT_VEL_GENERIC)) {
                send_fault("ptz_service",
                           "Sender",
                           "ter:InvalidArgVal",
                           "ter:SpaceNotSupported",
                           "Space not supported",
                           "Pan/Tilt velocity space is not supported");
                return -3;
            }
            pantilt_present = 1;
            x = get_attribute(pantilt_node, "x");
            y = get_attribute(pantilt_node, "y");
            log_debug("PTZ: Raw X attribute: %s", x ? x : "NULL");
            log_debug("PTZ: Raw Y attribute: %s", y ? y : "NULL");
            double pan_norm = 0.0;
            double tilt_norm = 0.0;
            int pan_has = 0;
            int tilt_has = 0;
            if (x != NULL) {
                pan_norm = ptz_decode_relative_normalized(x, service_ctx.ptz_node.min_step_x, service_ctx.ptz_node.max_step_x);
                pan_has = 1;
            }
            if (y != NULL) {
                tilt_norm = ptz_decode_relative_normalized(y, service_ctx.ptz_node.min_step_y, service_ctx.ptz_node.max_step_y);
                tilt_has = 1;
            }
            if (pan_has || tilt_has) {
                ptz_apply_reverse(pan_has ? &pan_norm : NULL, tilt_has ? &tilt_norm : NULL);
            }
            if (pan_has)
                dx = pan_norm;
            if (tilt_has)
                dy = tilt_norm;
        }

        // Look for Zoom as sibling of PanTilt under Velocity, not inside PanTilt
        mxml_node_t *zoom_node = get_element_ptr(velocity_node, "Zoom", NULL);
        if (zoom_node != NULL) {
            const char *space_attr = get_attribute(zoom_node, "space");
            if (!ptz_space_matches(space_attr, PTZ_URI_ZOOM_VEL_GENERIC)) {
                send_fault("ptz_service",
                           "Sender",
                           "ter:InvalidArgVal",
                           "ter:SpaceNotSupported",
                           "Space not supported",
                           "Zoom velocity space is not supported");
                return -3;
            }
            zoom_present = 1;
            z = get_attribute(zoom_node, "x");
            log_debug("PTZ: Raw Z attribute: %s", z ? z : "NULL");
            if (z != NULL) {
                dz = ptz_decode_zoom_relative_normalized(z, service_ctx.ptz_node.min_step_z, service_ctx.ptz_node.max_step_z);
            }
        }
    }

    // Parse velocities first
    if (x != NULL)
        log_debug("PTZ: ContinuousMove X velocity (normalized): %f", dx);

    if (y != NULL)
        log_debug("PTZ: ContinuousMove Y velocity (normalized): %f", dy);

    // Check if we have diagonal movement (both X and Y non-zero) and continuous_move command is configured
    int has_diagonal = (x != NULL && dx != 0.0 && y != NULL && dy != 0.0);
    int use_continuous_move = has_diagonal && (service_ctx.ptz_node.continuous_move != NULL);

    if (use_continuous_move) {
        // Use single command for true diagonal movement
        double x_target, y_target;

        // Calculate target positions based on velocity direction
        if (dx > 0.0) {
            x_target = service_ctx.ptz_node.max_step_x;
        } else {
            x_target = service_ctx.ptz_node.min_step_x;
        }

        if (dy > 0.0) {
            y_target = service_ctx.ptz_node.min_step_y; // Up means min
        } else {
            y_target = service_ctx.ptz_node.max_step_y; // Down means max
        }

        sprintf(sys_command, service_ctx.ptz_node.continuous_move, x_target, y_target);
        log_debug("PTZ: Executing diagonal continuous_move command: %s", sys_command);
        system(sys_command);
        ret = 0;
    } else {
        // Fall back to separate axis commands

        // Check for validation errors before executing any commands
        if (x != NULL && dx != 0.0) {
            if (dx > 0.0 && service_ctx.ptz_node.move_right == NULL) {
                send_action_failed_fault("ptz_service", -3);
                return -3;
            }
            if (dx < 0.0 && service_ctx.ptz_node.move_left == NULL) {
                send_action_failed_fault("ptz_service", -4);
                return -4;
            }
        }

        if (y != NULL && dy != 0.0) {
            if (dy > 0.0 && service_ctx.ptz_node.move_up == NULL) {
                send_action_failed_fault("ptz_service", -5);
                return -5;
            }
            if (dy < 0.0 && service_ctx.ptz_node.move_down == NULL) {
                send_action_failed_fault("ptz_service", -6);
                return -6;
            }
        }

        // Execute movement commands - both axes if needed for diagonal movement
        if (x != NULL && dx > 0.0) {
            sprintf(sys_command, service_ctx.ptz_node.move_right, dx);
            log_debug("PTZ: Executing move_right command: %s", sys_command);
            system(sys_command);
            ret = 0;
        } else if (x != NULL && dx < 0.0) {
            sprintf(sys_command, service_ctx.ptz_node.move_left, -dx);
            log_debug("PTZ: Executing move_left command: %s", sys_command);
            system(sys_command);
            ret = 0;
        }

        if (y != NULL && dy > 0.0) {
            sprintf(sys_command, service_ctx.ptz_node.move_up, dy);
            log_debug("PTZ: Executing move_up command: %s", sys_command);
            system(sys_command);
            ret = 0;
        } else if (y != NULL && dy < 0.0) {
            sprintf(sys_command, service_ctx.ptz_node.move_down, -dy);
            log_debug("PTZ: Executing move_down command: %s", sys_command);
            system(sys_command);
            ret = 0;
        }
    }

    if (z != NULL) {
        if (dz > 0.0) {
            if (service_ctx.ptz_node.move_in == NULL) {
                send_action_failed_fault("ptz_service", -7);
                return -7;
            }
            sprintf(sys_command, service_ctx.ptz_node.move_in, dz);
            system(sys_command);
            ret = 0;
        } else if (dz < 0.0) {
            if (service_ctx.ptz_node.move_out == NULL) {
                send_action_failed_fault("ptz_service", -8);
                return -8;
            }
            sprintf(sys_command, service_ctx.ptz_node.move_out, -dz);
            system(sys_command);
            ret = 0;
        }
    }

    // Per spec: zero velocity in an axis shall stop that axis
    if (pantilt_present && x != NULL && y != NULL && dx == 0.0 && dy == 0.0 && service_ctx.ptz_node.move_stop != NULL) {
        sprintf(sys_command, service_ctx.ptz_node.move_stop, "pantilt");
        log_debug("PTZ: Stopping pan/tilt due to zero velocity");
        system(sys_command);
        ret = 0;
    }
    if (zoom_present && z != NULL && dz == 0.0 && service_ctx.ptz_node.move_stop != NULL) {
        sprintf(sys_command, service_ctx.ptz_node.move_stop, "zoom");
        log_debug("PTZ: Stopping zoom due to zero velocity");
        system(sys_command);
        ret = 0;
    }

    long size = cat(NULL, "ptz_service_files/ContinuousMove.xml", 0);

    output_http_headers(size);

    return cat("stdout", "ptz_service_files/ContinuousMove.xml", 0);
}

int ptz_relative_move()
{
    char const *x = NULL;
    char const *y = NULL;
    char const *z = NULL;
    char const *space_p = NULL;
    char const *space_z = NULL;
    double dx = 0.0;
    double dy = 0.0;
    double dz = 0.0;
    double pt_speed = -1.0;
    double zoom_speed = -1.0;
    char sys_command[MAX_LEN];
    int ret = 0;
    mxml_node_t *node;
    mxml_node_t *node_p = NULL;
    mxml_node_t *node_z = NULL;
    int pantilt_present = 0;
    int zoom_present = 0;

    node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoProfile",
                   "No profile",
                   "The requested profile token ProfileToken does not exist");
        return -1;
    }

    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoPTZProfile",
                   "No PTZ profile",
                   "The requested profile token does not reference a PTZ configuration");
        return -2;
    }

    if (service_ctx.ptz_node.jump_to_rel == NULL) {
        send_action_failed_fault("ptz_service", -3);
        return -3;
    }

    node = get_element_ptr(NULL, "Translation", "Body");
    if (node != NULL) {
        node_p = get_element_in_element_ptr("PanTilt", node);
        if (node_p != NULL) {
            x = get_attribute(node_p, "x");
            y = get_attribute(node_p, "y");
            space_p = get_attribute(node_p, "space");
        }
        node_z = get_element_in_element_ptr("Zoom", node);
        if (node_z != NULL) {
            z = get_attribute(node_z, "x");
            space_z = get_attribute(node_z, "space");
        }
    }

    // Optional Speed parsing
    node = get_element_ptr(NULL, "Speed", "Body");
    if (node != NULL) {
        mxml_node_t *pt = get_element_in_element_ptr("PanTilt", node);
        if (pt) {
            const char *sx = get_attribute(pt, "x");
            const char *sy = get_attribute(pt, "y");
            if (sx) {
                double d = atof(sx);
                if (d < 0)
                    d = -d;
                pt_speed = d;
            }
            if (sy) {
                double d = atof(sy);
                if (d < 0)
                    d = -d;
                if (pt_speed < 0 || d > pt_speed)
                    pt_speed = d;
            }
            if (pt_speed < 0)
                pt_speed = 0;
            if (pt_speed > 1)
                pt_speed = 1;
        }
        mxml_node_t *zm = get_element_in_element_ptr("Zoom", node);
        if (zm) {
            const char *sz = get_attribute(zm, "x");
            if (sz) {
                double d = atof(sz);
                if (d < 0)
                    d = -d;
                zoom_speed = d;
                if (zoom_speed < 0)
                    zoom_speed = 0;
                if (zoom_speed > 1)
                    zoom_speed = 1;
            }
        }
    }

    if (node_p != NULL) {
        if ((space_p == NULL) || (strcmp(space_p, PTZ_URI_PANTILT_REL_GENERIC) == 0)) {
            if ((x == NULL) || (y == NULL)) {
                ret = -4;
            } else {
                double pan_norm = ptz_decode_relative_normalized(x, service_ctx.ptz_node.min_step_x, service_ctx.ptz_node.max_step_x);
                double tilt_norm = ptz_decode_relative_normalized(y, service_ctx.ptz_node.min_step_y, service_ctx.ptz_node.max_step_y);
                ptz_apply_reverse(&pan_norm, &tilt_norm);
                dx = ptz_relative_normalized_to_delta(pan_norm, service_ctx.ptz_node.min_step_x, service_ctx.ptz_node.max_step_x);
                dy = ptz_relative_normalized_to_delta(tilt_norm, service_ctx.ptz_node.min_step_y, service_ctx.ptz_node.max_step_y);
                pantilt_present = 1;
            }
        } else if (strcmp("http://www.onvif.org/ver10/tptz/PanTiltSpaces/TranslationSpaceFov", space_p) == 0) {
            if ((x == NULL) || (y == NULL)) {
                ret = -8;
            } else {
                dx = atof(x);
                if ((dx > 100.0) || (dx < -100.0)) {
                    ret = -9;
                }
                dy = atof(y);
                if ((dy > 100.0) || (dy < -100.0)) {
                    ret = -10;
                }
                if (ret == 0) {
                    dx = (dx / 100.0) * (63.0 / 2.0);
                    dy = (dy / 100.0) * (37.0 / 2.0);
                    dx = dx / (360.0 / service_ctx.ptz_node.max_step_x);
                    dy = dy / (180.0 / service_ctx.ptz_node.max_step_y);
                    ptz_apply_reverse(&dx, &dy);
                    pantilt_present = 1;
                }
            }
        } else {
            send_fault("ptz_service",
                       "Sender",
                       "ter:InvalidArgVal",
                       "ter:SpaceNotSupported",
                       "Space not supported",
                       "Pan/Tilt relative space is not supported");
            return -4;
        }
    }

    if (node_z != NULL) {
        if ((space_z == NULL) || (strcmp(space_z, PTZ_URI_ZOOM_REL_GENERIC) == 0)) {
            if (z == NULL) {
                ret = -7;
            } else {
                double zoom_norm = ptz_decode_zoom_relative_normalized(z, service_ctx.ptz_node.min_step_z, service_ctx.ptz_node.max_step_z);
                dz = ptz_relative_normalized_to_delta(zoom_norm, service_ctx.ptz_node.min_step_z, service_ctx.ptz_node.max_step_z);
                zoom_present = 1;
            }
        } else {
            send_fault("ptz_service",
                       "Sender",
                       "ter:InvalidArgVal",
                       "ter:SpaceNotSupported",
                       "Space not supported",
                       "Zoom relative space is not supported");
            return -4;
        }
    }

    if (!pantilt_present && !zoom_present) {
        ret = -4;
    }

    if (ret == 0 && pantilt_present) {
        if (service_ctx.ptz_node.jump_to_rel_speed != NULL && (pt_speed >= 0.0 || zoom_speed >= 0.0)) {
            double pts = (pt_speed >= 0.0) ? pt_speed : 0.0;
            double zs = (zoom_speed >= 0.0) ? zoom_speed : 0.0;
            sprintf(sys_command, service_ctx.ptz_node.jump_to_rel_speed, dx, dy, dz, pts, zs);
        } else {
            sprintf(sys_command, service_ctx.ptz_node.jump_to_rel, dx, dy, dz);
        }
    } else if (ret == 0 && zoom_present) {
        if (service_ctx.ptz_node.jump_to_rel_speed != NULL && zoom_speed >= 0.0) {
            sprintf(sys_command, service_ctx.ptz_node.jump_to_rel_speed, 0.0, 0.0, dz, 0.0, zoom_speed);
        } else {
            sprintf(sys_command, service_ctx.ptz_node.jump_to_rel, 0.0, 0.0, dz);
        }
    }

    if (ret == 0) {
        system(sys_command);

        long size = cat(NULL, "ptz_service_files/RelativeMove.xml", 0);

        output_http_headers(size);

        return cat("stdout", "ptz_service_files/RelativeMove.xml", 0);

    } else {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:InvalidTranslation",
                   "Invalid translation",
                   "The requested translation is out of bounds");
        return ret;
    }
}

int ptz_absolute_move()
{
    char const *x = NULL;
    char const *y = NULL;
    char const *z = NULL;
    double dx = 0.0;
    double dy = 0.0;
    double dz = 0.0;
    double pt_speed = -1.0;
    double zoom_speed = -1.0;
    char sys_command[MAX_LEN];
    int ret = 0;
    mxml_node_t *node;

    node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoProfile",
                   "No profile",
                   "The requested profile token ProfileToken does not exist");
        return -1;
    }

    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoPTZProfile",
                   "No PTZ profile",
                   "The requested profile token does not reference a PTZ configuration");
        return -2;
    }

    if (service_ctx.ptz_node.jump_to_abs == NULL) {
        send_action_failed_fault("ptz_service", -3);
        return -3;
    }

    int pantilt_present = 0;
    int zoom_present = 0;
    double pan_norm = 0.0;
    double tilt_norm = 0.0;
    double zoom_norm = 0.0;

    node = get_element_ptr(NULL, "Position", "Body");
    if (node != NULL) {
        mxml_node_t *pan_tilt_node = get_element_in_element_ptr("PanTilt", node);
        if (pan_tilt_node != NULL) {
            const char *space_attr = get_attribute(pan_tilt_node, "space");
            if (space_attr != NULL && strcmp(space_attr, PTZ_URI_PANTILT_ABS_GENERIC) != 0) {
                send_fault("ptz_service",
                           "Sender",
                           "ter:InvalidArgVal",
                           "ter:SpaceNotSupported",
                           "Space not supported",
                           "Pan/Tilt absolute space is not supported");
                return -4;
            }
            x = get_attribute(pan_tilt_node, "x");
            y = get_attribute(pan_tilt_node, "y");
            if ((x != NULL) && (y != NULL)) {
                pan_norm = ptz_decode_absolute_normalized(x, service_ctx.ptz_node.min_step_x, service_ctx.ptz_node.max_step_x);
                tilt_norm = ptz_decode_absolute_normalized(y, service_ctx.ptz_node.min_step_y, service_ctx.ptz_node.max_step_y);
                pantilt_present = 1;
            }
        }
        mxml_node_t *zoom_node = get_element_in_element_ptr("Zoom", node);
        if (zoom_node != NULL) {
            const char *space_attr = get_attribute(zoom_node, "space");
            if (space_attr != NULL && strcmp(space_attr, PTZ_URI_ZOOM_ABS_GENERIC) != 0) {
                send_fault("ptz_service",
                           "Sender",
                           "ter:InvalidArgVal",
                           "ter:SpaceNotSupported",
                           "Space not supported",
                           "Zoom absolute space is not supported");
                return -4;
            }
            z = get_attribute(zoom_node, "x");
            if (z != NULL) {
                zoom_norm = ptz_decode_zoom_normalized(z, service_ctx.ptz_node.min_step_z, service_ctx.ptz_node.max_step_z);
                zoom_present = 1;
            }
        }
    }

    mxml_node_t *speed_node = get_element_ptr(NULL, "Speed", "Body");
    if (speed_node != NULL) {
        mxml_node_t *pt = get_element_in_element_ptr("PanTilt", speed_node);
        if (pt) {
            const char *sx = get_attribute(pt, "x");
            const char *sy = get_attribute(pt, "y");
            if (sx) {
                double d = atof(sx);
                if (d < 0)
                    d = -d;
                pt_speed = d;
            }
            if (sy) {
                double d = atof(sy);
                if (d < 0)
                    d = -d;
                if (pt_speed < 0 || d > pt_speed)
                    pt_speed = d;
            }
            if (pt_speed < 0)
                pt_speed = 0;
            if (pt_speed > 1)
                pt_speed = 1;
        }
        mxml_node_t *zm = get_element_in_element_ptr("Zoom", speed_node);
        if (zm) {
            const char *sz = get_attribute(zm, "x");
            if (sz) {
                double d = atof(sz);
                if (d < 0)
                    d = -d;
                zoom_speed = d;
                if (zoom_speed < 0)
                    zoom_speed = 0;
                if (zoom_speed > 1)
                    zoom_speed = 1;
            }
        }
    }

    if (!pantilt_present && !zoom_present) {
        ret = -4;
    } else {
        if (pantilt_present) {
            ptz_apply_reverse(&pan_norm, &tilt_norm);
            dx = ptz_normalized_to_range(pan_norm, service_ctx.ptz_node.min_step_x, service_ctx.ptz_node.max_step_x);
            dy = ptz_normalized_to_range(tilt_norm, service_ctx.ptz_node.min_step_y, service_ctx.ptz_node.max_step_y);
        }
        if (zoom_present) {
            dz = ptz_zoom_normalized_to_range(zoom_norm, service_ctx.ptz_node.min_step_z, service_ctx.ptz_node.max_step_z);
        }

        if (pantilt_present) {
            if (service_ctx.ptz_node.jump_to_abs_speed != NULL && (pt_speed >= 0.0 || zoom_speed >= 0.0)) {
                double pts = (pt_speed >= 0.0) ? pt_speed : 0.0;
                double zs = (zoom_speed >= 0.0) ? zoom_speed : 0.0;
                sprintf(sys_command, service_ctx.ptz_node.jump_to_abs_speed, dx, dy, dz, pts, zs);
            } else {
                sprintf(sys_command, service_ctx.ptz_node.jump_to_abs, dx, dy, dz);
            }
        } else if (zoom_present) {
            if (service_ctx.ptz_node.jump_to_abs_speed != NULL && zoom_speed >= 0.0) {
                sprintf(sys_command, service_ctx.ptz_node.jump_to_abs_speed, 0.0, 0.0, dz, 0.0, zoom_speed);
            } else {
                sprintf(sys_command, service_ctx.ptz_node.jump_to_abs, 0.0, 0.0, dz);
            }
        }
    }

    if (ret == 0) {
        system(sys_command);

        long size = cat(NULL, "ptz_service_files/AbsoluteMove.xml", 0);

        output_http_headers(size);

        return cat("stdout", "ptz_service_files/AbsoluteMove.xml", 0);

    } else {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:InvalidPosition", "Invalid position", "The requested position is out of bounds");
        return ret;
    }
}

int ptz_stop()
{
    char sys_command[MAX_LEN];
    mxml_node_t *node;
    int pantilt = 1;
    int zoom = 1;

    node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoProfile",
                   "No profile",
                   "The requested profile token ProfileToken does not exist");
        return -1;
    }

    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoPTZProfile",
                   "No PTZ profile",
                   "The requested profile token does not reference a PTZ configuration");
        return -2;
    }

    if (service_ctx.ptz_node.move_stop == NULL) {
        send_action_failed_fault("ptz_service", -3);
        return -3;
    }

    const char *pantilt_node = get_element("PanTilt", "Body");
    if ((pantilt_node != NULL) && (strcasecmp("false", pantilt_node) == 0)) {
        pantilt = 0;
    }
    const char *zoom_node = get_element("Zoom", "Body");
    if ((zoom_node != NULL) && (strcasecmp("false", zoom_node) == 0)) {
        zoom = 0;
    }

    if (pantilt && zoom) {
        sprintf(sys_command, service_ctx.ptz_node.move_stop, "all");
        log_debug("PTZ: Executing stop command: %s", sys_command);
        system(sys_command);
    } else if (pantilt) {
        sprintf(sys_command, service_ctx.ptz_node.move_stop, "pantilt");
        system(sys_command);
    } else if (zoom) {
        sprintf(sys_command, service_ctx.ptz_node.move_stop, "zoom");
        system(sys_command);
    }

    long size = cat(NULL, "ptz_service_files/Stop.xml", 0);

    output_http_headers(size);

    return cat("stdout", "ptz_service_files/Stop.xml", 0);
}

int ptz_get_status()
{
    char utctime[32];
    time_t timestamp = time(NULL);
    struct tm *tm = gmtime(&timestamp);
    int ret = 0;
    FILE *fp;
    double x, y, z = 1.0;
    int i = 0;
    char out[256], sx[128], sy[128], sz[128], si[128];
    mxml_node_t *node;

    node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoProfile",
                   "No profile",
                   "The requested profile token ProfileToken does not exist");
        return -1;
    }

    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoPTZProfile",
                   "No PTZ profile",
                   "The requested profile token does not reference a PTZ configuration");
        return -2;
    }

    sprintf(utctime, "%04d-%02d-%02dT%02d:%02d:%02dZ", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

    // Run command that returns to stdout x and y position in the form x,y
    if (service_ctx.ptz_node.get_position != NULL) {
        fp = popen(service_ctx.ptz_node.get_position, "r");
        if (fp == NULL) {
            ret = -3;
        } else {
            if (fgets(out, sizeof(out), fp) == NULL) {
                ret = -4;
            } else {
                if (sscanf(out, "%lf,%lf,%lf", &x, &y, &z) < 2) {
                    ret = -5;
                }
            }
            pclose(fp);
        }
    } else {
        // If the cam doesn't know the status, return a fault
        ret = -6;
    }

    // Run command that returns to stdout if PTZ is moving (1) or not (0)
    if (service_ctx.ptz_node.is_moving != NULL) {
        fp = popen(service_ctx.ptz_node.is_moving, "r");
        if (fp == NULL) {
            ret = -7;
        } else {
            if (fgets(out, sizeof(out), fp) == NULL) {
                ret = -8;
            } else {
                if (sscanf(out, "%d", &i) < 1) {
                    ret = -9;
                }
            }
            pclose(fp);
        }
    } else {
        // If the cam doesn't know the status, return IDLE
        i = 0;
    }

    if (ret == 0) {
        double pan_norm = ptz_range_to_normalized(x, service_ctx.ptz_node.min_step_x, service_ctx.ptz_node.max_step_x);
        double tilt_norm = ptz_range_to_normalized(y, service_ctx.ptz_node.min_step_y, service_ctx.ptz_node.max_step_y);
        ptz_apply_reverse(&pan_norm, &tilt_norm);
        double zoom_norm = ptz_zoom_range_to_normalized(z, service_ctx.ptz_node.min_step_z, service_ctx.ptz_node.max_step_z);
        snprintf(sx, sizeof(sx), "%.4f", pan_norm);
        snprintf(sy, sizeof(sy), "%.4f", tilt_norm);
        snprintf(sz, sizeof(sz), "%.4f", zoom_norm);
        if (i == 1)
            strcpy(si, "MOVING");
        else
            strcpy(si, "IDLE");

        long size = cat(NULL,
                        "ptz_service_files/GetStatus.xml",
                        12,
                        "%X%",
                        sx,
                        "%Y%",
                        sy,
                        "%Z%",
                        sz,
                        "%MOVE_STATUS_PT%",
                        si,
                        "%MOVE_STATUS_ZOOM%",
                        "IDLE",
                        "%TIME%",
                        utctime);

        output_http_headers(size);

        return cat("stdout",
                   "ptz_service_files/GetStatus.xml",
                   12,
                   "%X%",
                   sx,
                   "%Y%",
                   sy,
                   "%Z%",
                   sz,
                   "%MOVE_STATUS_PT%",
                   si,
                   "%MOVE_STATUS_ZOOM%",
                   "IDLE",
                   "%TIME%",
                   utctime);
    } else {
        send_fault("ptz_service", "Receiver", "ter:Action", "ter:NoStatus", "No status", "No PTZ status is available in the requested Media Profile");
        return ret;
    }
}

int ptz_set_preset()
{
    int i;
    char sys_command[MAX_LEN];
    const char *preset_name;
    char preset_name_out[UUID_LEN + 8];
    const char *preset_token;
    mxml_node_t *node;
    char preset_token_out[16];
    int preset_number = -1;
    // Validate ProfileToken maps to an existing profile
    const char *profile_token = get_element("ProfileToken", "Body");
    if (profile_token == NULL) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile token does not exist");
        return -2;
    }
    int profile_ok = 0;
    for (int pi = 0; pi < service_ctx.profiles_num; ++pi) {
        if (service_ctx.profiles[pi].name && strcasecmp(service_ctx.profiles[pi].name, profile_token) == 0) {
            profile_ok = 1;
            break;
        }
    }
    if (!profile_ok) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile token does not exist");
        return -2;
    }

    // Spec 5.4.1: fail if PTZ device is moving
    if (service_ctx.ptz_node.is_moving != NULL) {
        FILE *fp_mv = popen(service_ctx.ptz_node.is_moving, "r");
        if (fp_mv != NULL) {
            char buf_mv[32];
            if (fgets(buf_mv, sizeof(buf_mv), fp_mv) != NULL) {
                int moving = 0;
                if (sscanf(buf_mv, "%d", &moving) == 1 && moving == 1) {
                    send_fault("ptz_service", "Receiver", "ter:Action", "ter:MovingPTZ", "Moving PTZ", "Preset cannot be set while PTZ unit is moving");
                    pclose(fp_mv);
                    return -3;
                }
            }
            pclose(fp_mv);
        }
    }

    int preset_found;
    int presets_total_number;
    char name_uuid[UUID_LEN + 1];

    node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile token does not exist");
        return -1;
    }

    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoPTZProfile",
                   "No PTZ profile",
                   "The requested profile token does not reference a PTZ configuration");
        return -2;
    }

    preset_name = get_element("PresetName", "Body");
    preset_token = get_element("PresetToken", "Body");
    init_presets();
    presets_total_number = presets.count;

    if (preset_token == NULL) {
        // Add new preset
        if (preset_name == NULL) {
            // No name and no token, how to identify it? Create a random name.
            gen_uuid(name_uuid);
            sprintf(preset_name_out, "Preset_%s", name_uuid);
        } else {
            strcpy(preset_name_out, preset_name);
        }

        if ((strchr(preset_name_out, ' ') != NULL) || (strlen(preset_name_out) == 0) || (strlen(preset_name_out) > 64)) {
            destroy_presets();
            send_fault("ptz_service",
                       "Sender",
                       "ter:InvalidArgVal",
                       "ter:InvalidPresetName",
                       "Invalid preset name",
                       "The preset name is either too long or contains invalid characters");
            return -3;
        }
        for (i = 0; i < presets.count; i++) {
            if (strcasecmp(presets.items[i].name, preset_name_out) == 0) {
                destroy_presets();
                send_fault("ptz_service",
                           "Sender",
                           "ter:InvalidArgVal",
                           "ter:PresetExist",
                           "Preset exists",
                           "The requested name already exist for another preset");
                return -4;
            }
        }

        preset_number = -1;

    } else {
        // Update existing preset
        if (sscanf(preset_token, "PresetToken_%d", &preset_number) != 1) {
            destroy_presets();
            send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoToken", "No token", "The requested preset token does not exist");
            return -5;
        }

        preset_found = 0;
        for (i = 0; i < presets.count; i++) {
            if (presets.items[i].number == preset_number) {
                strcpy(preset_name_out, presets.items[i].name);
                preset_found = 1;
                break;
            }
        }
        if (preset_found == 0) {
            destroy_presets();
            send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoToken", "No token", "The requested preset token does not exist");
            return -6;
        }

        if (preset_name != NULL) {
            // Overwrite the name with the new one
            memset(preset_name_out, '\0', sizeof(preset_name_out));
            strncpy(preset_name_out, preset_name, strlen(preset_name));
        }

        if ((strchr(preset_name_out, ' ') != NULL) || (strlen(preset_name_out) == 0) || (strlen(preset_name_out) > 64)) {
            destroy_presets();
            send_fault("ptz_service",
                       "Sender",
                       "ter:InvalidArgVal",
                       "ter:InvalidPresetName",
                       "Invalid preset name",
                       "The preset name is either too long or contains invalid characters");
            return -7;
        }

        for (i = 0; i < presets.count; i++) {
            if ((presets.items[i].number != preset_number) && (strcasecmp(presets.items[i].name, preset_name_out) == 0)) {
                destroy_presets();
                send_fault("ptz_service",
                           "Sender",
                           "ter:InvalidArgVal",
                           "ter:PresetExist",
                           "Preset exists",
                           "The requested name already exist for another preset");
                return -8;
            }
        }
    }

    if (service_ctx.ptz_node.set_preset == NULL) {
        destroy_presets();
        send_action_failed_fault("ptz_service", -9);
        return -9;
    }

    destroy_presets();

    // Unhandled race condition
    sprintf(sys_command, service_ctx.ptz_node.set_preset, preset_number, (char *) preset_name_out);
    system(sys_command);
    sleep(1);

    init_presets();
    if ((preset_token == NULL) && (presets_total_number == presets.count)) {
        destroy_presets();
        send_fault("ptz_service", "Receiver", "ter:Action", "ter:TooManyPresets", "Too many presets", "Maximum number of presets reached");
        return -10;
    }
    preset_token_out[0] = '\0';
    for (i = 0; i < presets.count; i++) {
        if (strcasecmp(presets.items[i].name, preset_name_out) == 0) {
            sprintf(preset_token_out, "PresetToken_%d", presets.items[i].number);
            break;
        }
    }
    destroy_presets();

    long size = cat(NULL, "ptz_service_files/SetPreset.xml", 2, "%PRESET_TOKEN%", preset_token_out);

    output_http_headers(size);

    return cat("stdout", "ptz_service_files/SetPreset.xml", 2, "%PRESET_TOKEN%", preset_token_out);
}

int ptz_set_home_position()
{
    char sys_command[MAX_LEN];
    mxml_node_t *node;

    node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoProfile",
                   "No profile",
                   "The requested profile token ProfileToken does not exist");
        return -1;
    }

    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoPTZProfile",
                   "No PTZ profile",
                   "The requested profile token does not reference a PTZ configuration");
        return -2;
    }

    if (service_ctx.ptz_node.set_home_position == NULL) {
        send_action_failed_fault("ptz_service", -3);
        return -3;
    }

    strcpy(sys_command, service_ctx.ptz_node.set_home_position);
    system(sys_command);

    long size = cat(NULL, "ptz_service_files/SetHomePosition.xml", 0);

    output_http_headers(size);

    return cat("stdout", "ptz_service_files/SetHomePosition.xml", 0);
}

int ptz_send_auxiliary_command()
{
    mxml_node_t *node;
    node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoProfile",
                   "No profile",
                   "The requested profile token ProfileToken does not exist");
        return -1;
    }

    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoPTZProfile",
                   "No PTZ profile",
                   "The requested profile token does not reference a PTZ configuration");
        return -2;
    }

    const char *aux = get_element("AuxiliaryData", "Body");
    if (aux == NULL) {
        aux = "";
    }

    long size2 = cat(NULL, "ptz_service_files/SendAuxiliaryCommand.xml", 2, "%AUX_RESPONSE%", aux);

    output_http_headers(size2);

    return cat("stdout", "ptz_service_files/SendAuxiliaryCommand.xml", 2, "%AUX_RESPONSE%", aux);
}

int ptz_get_preset_tours()
{
    mxml_node_t *node;
    node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoProfile",
                   "No profile",
                   "The requested profile token ProfileToken does not exist");
        return -1;
    }
    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoPTZProfile",
                   "No PTZ profile",
                   "The requested profile token does not reference a PTZ configuration");
        return -2;
    }
    tours_ensure_loaded();

    long size = 0, total = 0;
    char dest_a[] = "stdout";
    char *dest;
    for (int pass = 0; pass < 2; pass++) {
        dest = (pass == 0) ? NULL : dest_a;
        size = cat(dest, "ptz_service_files/GetPresetTours_1.xml", 0);
        if (pass == 0)
            total = size;
        else
            fflush(stdout);
        for (int i = 0; i < g_tours_count; i++) {
            size = cat(dest,
                       "ptz_service_files/GetPresetTours_item.xml",
                       6,
                       "%TOKEN%",
                       g_tours[i].token,
                       "%NAME%",
                       g_tours[i].name[0] ? g_tours[i].name : "",
                       "%STATUS%",
                       g_tours[i].status[0] ? g_tours[i].status : "Idle");
            if (pass == 0)
                total += size;
            else
                fflush(stdout);
        }
        size = cat(dest, "ptz_service_files/GetPresetTours_3.xml", 0);
        if (pass == 0)
            total += size;
        else
            fflush(stdout);
        if (pass == 0)
            output_http_headers(total);
    }
    return total;
}

int ptz_get_preset_tour()
{
    mxml_node_t *node;
    node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoProfile",
                   "No profile",
                   "The requested profile token ProfileToken does not exist");
        return -1;
    }
    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoPTZProfile",
                   "No PTZ profile",
                   "The requested profile token does not reference a PTZ configuration");
        return -2;
    }
    const char *tour_token = get_element("PresetTourToken", "Body");
    if (!tour_token) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoToken", "No token", "The requested preset tour token does not exist");
        return -3;
    }
    tours_ensure_loaded();
    int idx = tour_index_by_token(tour_token);
    if (idx < 0) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoToken", "No token", "The requested preset tour token does not exist");
        return -4;
    }
    long size = cat(NULL,
                    "ptz_service_files/GetPresetTour.xml",
                    6,
                    "%TOKEN%",
                    g_tours[idx].token,
                    "%NAME%",
                    g_tours[idx].name[0] ? g_tours[idx].name : "",
                    "%STATUS%",
                    g_tours[idx].status[0] ? g_tours[idx].status : "Idle");
    output_http_headers(size);
    return cat("stdout",
               "ptz_service_files/GetPresetTour.xml",
               6,
               "%TOKEN%",
               g_tours[idx].token,
               "%NAME%",
               g_tours[idx].name[0] ? g_tours[idx].name : "",
               "%STATUS%",
               g_tours[idx].status[0] ? g_tours[idx].status : "Idle");
}

int ptz_get_preset_tour_options()
{
    mxml_node_t *node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoProfile",
                   "No profile",
                   "The requested profile token ProfileToken does not exist");
        return -1;
    }
    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoPTZProfile",
                   "No PTZ profile",
                   "The requested profile token does not reference a PTZ configuration");
        return -2;
    }
    long size = cat(NULL, "ptz_service_files/GetPresetTourOptions.xml", 0);
    output_http_headers(size);
    return cat("stdout", "ptz_service_files/GetPresetTourOptions.xml", 0);
}

static int next_tour_number()
{
    int maxn = 0;
    int n;
    for (int i = 0; i < g_tours_count; i++) {
        if (sscanf(g_tours[i].token, "PresetTourToken_%d", &n) == 1) {
            if (n > maxn)
                maxn = n;
        }
    }
    return maxn + 1;
}

int ptz_create_preset_tour()
{
    mxml_node_t *node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoProfile",
                   "No profile",
                   "The requested profile token ProfileToken does not exist");
        return -1;
    }
    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoPTZProfile",
                   "No PTZ profile",
                   "The requested profile token does not reference a PTZ configuration");
        return -2;
    }
    tours_ensure_loaded();
    if (service_ctx.ptz_node.max_preset_tours > 0 && g_tours_count >= service_ctx.ptz_node.max_preset_tours) {
        send_action_failed_fault("ptz_service", -3);
        return -3;
    }
    const char *name = get_element("Name", "Body");
    if (!name)
        name = "";
    int num = next_tour_number();
    char token[64];
    snprintf(token, sizeof(token), "PresetTourToken_%d", num);
    g_tours = (preset_tour_t *) realloc(g_tours, sizeof(preset_tour_t) * (g_tours_count + 1));
    memset(&g_tours[g_tours_count], 0, sizeof(preset_tour_t));
    strncpy(g_tours[g_tours_count].token, token, sizeof(g_tours[g_tours_count].token) - 1);
    if (name && name[0])
        strncpy(g_tours[g_tours_count].name, name, sizeof(g_tours[g_tours_count].name) - 1);
    strncpy(g_tours[g_tours_count].status, "Idle", sizeof(g_tours[g_tours_count].status) - 1);
    g_tours_count++;
    tours_save();
    long size = cat(NULL, "ptz_service_files/CreatePresetTour.xml", 2, "%TOKEN%", token);
    output_http_headers(size);
    return cat("stdout", "ptz_service_files/CreatePresetTour.xml", 2, "%TOKEN%", token);
}

int ptz_modify_preset_tour()
{
    mxml_node_t *node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoProfile",
                   "No profile",
                   "The requested profile token ProfileToken does not exist");
        return -1;
    }
    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoPTZProfile",
                   "No PTZ profile",
                   "The requested profile token does not reference a PTZ configuration");
        return -2;
    }
    const char *tour_token = get_element("PresetTourToken", "Body");
    if (!tour_token) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoToken", "No token", "The requested preset tour token does not exist");
        return -3;
    }
    tours_ensure_loaded();
    int idx = tour_index_by_token(tour_token);
    if (idx < 0) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoToken", "No token", "The requested preset tour token does not exist");
        return -4;
    }
    const char *name = get_element("Name", "Body");
    if (name && name[0]) {
        memset(g_tours[idx].name, 0, sizeof(g_tours[idx].name));
        strncpy(g_tours[idx].name, name, sizeof(g_tours[idx].name) - 1);
    }
    tours_save();
    long size = cat(NULL, "ptz_service_files/ModifyPresetTour.xml", 0);
    output_http_headers(size);
    return cat("stdout", "ptz_service_files/ModifyPresetTour.xml", 0);
}

int ptz_operate_preset_tour()
{
    mxml_node_t *node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoProfile",
                   "No profile",
                   "The requested profile token ProfileToken does not exist");
        return -1;
    }
    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoPTZProfile",
                   "No PTZ profile",
                   "The requested profile token does not reference a PTZ configuration");
        return -2;
    }
    const char *tour_token = get_element("PresetTourToken", "Body");
    const char *operation = get_element("Operation", "Body");
    if (!tour_token || !operation) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoToken", "No token", "Missing parameters");
        return -3;
    }
    tours_ensure_loaded();
    int idx = tour_index_by_token(tour_token);
    if (idx < 0) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoToken", "No token", "The requested preset tour token does not exist");
        return -4;
    }
    char cmd[MAX_LEN];
    int ok = 0;
    if (strcasecmp(operation, "Start") == 0) {
        if (service_ctx.ptz_node.preset_tour_start == NULL) {
            send_action_failed_fault("ptz_service", -5);
            return -5;
        }
        snprintf(cmd, sizeof(cmd), service_ctx.ptz_node.preset_tour_start, tour_token);
        ok = 1;
        strncpy(g_tours[idx].status, "Touring", sizeof(g_tours[idx].status) - 1);
    } else if (strcasecmp(operation, "Stop") == 0) {
        if (service_ctx.ptz_node.preset_tour_stop == NULL) {
            send_action_failed_fault("ptz_service", -6);
            return -6;
        }
        snprintf(cmd, sizeof(cmd), service_ctx.ptz_node.preset_tour_stop, tour_token);
        ok = 1;
        strncpy(g_tours[idx].status, "Idle", sizeof(g_tours[idx].status) - 1);
    } else if (strcasecmp(operation, "Pause") == 0) {
        if (service_ctx.ptz_node.preset_tour_pause == NULL) {
            send_action_failed_fault("ptz_service", -7);
            return -7;
        }
        snprintf(cmd, sizeof(cmd), service_ctx.ptz_node.preset_tour_pause, tour_token);
        ok = 1;
        strncpy(g_tours[idx].status, "Paused", sizeof(g_tours[idx].status) - 1);
    } else {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:ActionNotSupported", "Not supported", "Operation not supported");
        return -8;
    }
    if (ok) {
        system(cmd);
        tours_save();
    }
    long size = cat(NULL, "ptz_service_files/OperatePresetTour.xml", 0);
    output_http_headers(size);
    return cat("stdout", "ptz_service_files/OperatePresetTour.xml", 0);
}

int ptz_remove_preset_tour()
{
    mxml_node_t *node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoProfile",
                   "No profile",
                   "The requested profile token ProfileToken does not exist");
        return -1;
    }
    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoPTZProfile",
                   "No PTZ profile",
                   "The requested profile token does not reference a PTZ configuration");
        return -2;
    }
    const char *tour_token = get_element("PresetTourToken", "Body");
    if (!tour_token) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoToken", "No token", "The requested preset tour token does not exist");
        return -3;
    }
    tours_ensure_loaded();
    int idx = tour_index_by_token(tour_token);
    if (idx < 0) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoToken", "No token", "The requested preset tour token does not exist");
        return -4;
    }
    for (int i = idx; i < g_tours_count - 1; i++)
        g_tours[i] = g_tours[i + 1];
    g_tours_count--;
    tours_save();
    long size = cat(NULL, "ptz_service_files/RemovePresetTour.xml", 0);
    output_http_headers(size);
    return cat("stdout", "ptz_service_files/RemovePresetTour.xml", 0);
}

int ptz_move_and_start_tracking()
{
    mxml_node_t *node;
    char sys_command[MAX_LEN];

    node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoProfile",
                   "No profile",
                   "The requested profile token ProfileToken does not exist");
        return -1;
    }

    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoPTZProfile",
                   "No PTZ profile",
                   "The requested profile token does not reference a PTZ configuration");
        return -2;
    }

    if (service_ctx.ptz_node.start_tracking == NULL) {
        // Backend not configured yet
        send_action_failed_fault("ptz_service", -3);
        return -3;
    }

    // Optional move to PresetToken
    const char *preset_token = get_element("PresetToken", "Body");
    if (preset_token && service_ctx.ptz_node.move_preset != NULL) {
        int preset_number = 0;
        if (sscanf(preset_token, "PresetToken_%d", &preset_number) == 1) {
            snprintf(sys_command, sizeof(sys_command), service_ctx.ptz_node.move_preset, preset_number);
            system(sys_command);
        }
    } else {
        // Optional move to PTZVector position
        mxml_node_t *pos = get_element_ptr(NULL, "Position", "Body");
        if (pos && service_ctx.ptz_node.jump_to_abs != NULL) {
            mxml_node_t *pt = get_element_in_element_ptr("PanTilt", pos);
            mxml_node_t *zm = get_element_in_element_ptr("Zoom", pos);
            double dx = 0.0, dy = 0.0, dz = 0.0;
            int pantilt_present = 0;
            int zoom_present = 0;

            if (pt) {
                const char *space_attr = get_attribute(pt, "space");
                if (space_attr != NULL && strcmp(space_attr, PTZ_URI_PANTILT_ABS_GENERIC) != 0) {
                    send_fault("ptz_service",
                               "Sender",
                               "ter:InvalidArgVal",
                               "ter:SpaceNotSupported",
                               "Space not supported",
                               "Pan/Tilt absolute space is not supported");
                    return -4;
                }
                const char *x = get_attribute(pt, "x");
                const char *y = get_attribute(pt, "y");
                if (x && y) {
                    double pan_norm = ptz_decode_absolute_normalized(x, service_ctx.ptz_node.min_step_x, service_ctx.ptz_node.max_step_x);
                    double tilt_norm = ptz_decode_absolute_normalized(y, service_ctx.ptz_node.min_step_y, service_ctx.ptz_node.max_step_y);
                    ptz_apply_reverse(&pan_norm, &tilt_norm);
                    dx = ptz_normalized_to_range(pan_norm, service_ctx.ptz_node.min_step_x, service_ctx.ptz_node.max_step_x);
                    dy = ptz_normalized_to_range(tilt_norm, service_ctx.ptz_node.min_step_y, service_ctx.ptz_node.max_step_y);
                    pantilt_present = 1;
                }
            }
            if (zm) {
                const char *space_attr = get_attribute(zm, "space");
                if (space_attr != NULL && strcmp(space_attr, PTZ_URI_ZOOM_ABS_GENERIC) != 0) {
                    send_fault("ptz_service",
                               "Sender",
                               "ter:InvalidArgVal",
                               "ter:SpaceNotSupported",
                               "Space not supported",
                               "Zoom absolute space is not supported");
                    return -4;
                }
                const char *z = get_attribute(zm, "x");
                if (z) {
                    double zoom_norm = ptz_decode_zoom_normalized(z, service_ctx.ptz_node.min_step_z, service_ctx.ptz_node.max_step_z);
                    dz = ptz_zoom_normalized_to_range(zoom_norm, service_ctx.ptz_node.min_step_z, service_ctx.ptz_node.max_step_z);
                    zoom_present = 1;
                }
            }
            if (pantilt_present || zoom_present) {
                snprintf(sys_command, sizeof(sys_command), service_ctx.ptz_node.jump_to_abs, dx, dy, dz);
                system(sys_command);
            }
        }
    }

    // Start tracking
    strncpy(sys_command, service_ctx.ptz_node.start_tracking, sizeof(sys_command) - 1);
    sys_command[sizeof(sys_command) - 1] = '\0';
    system(sys_command);

    long size = cat(NULL, "ptz_service_files/MoveAndStartTracking.xml", 0);

    output_http_headers(size);

    return cat("stdout", "ptz_service_files/MoveAndStartTracking.xml", 0);
}

int ptz_remove_preset()
{
    char sys_command[MAX_LEN];
    const char *preset_token;
    int preset_number;
    mxml_node_t *node;

    node = get_element_ptr(NULL, "ProfileToken", "Body");
    if (node == NULL) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoProfile",
                   "No profile",
                   "The requested profile token ProfileToken does not exist");
        return -1;
    }

    if (service_ctx.ptz_node.enable == 0) {
        send_fault("ptz_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoPTZProfile",
                   "No PTZ profile",
                   "The requested profile token does not reference a PTZ configuration");
        return -2;
    }
    preset_token = get_element("PresetToken", "Body");
    if (sscanf(preset_token, "PresetToken_%d", &preset_number) != 1) {
        send_fault("ptz_service", "Sender", "ter:InvalidArgVal", "ter:NoToken", "No token", "The requested preset token does not exist");
        return -3;
    }

    if (service_ctx.ptz_node.remove_preset == NULL) {
        send_action_failed_fault("ptz_service", -4);
        return -4;
    }

    sprintf(sys_command, service_ctx.ptz_node.remove_preset, preset_number);
    system(sys_command);

    long size = cat(NULL, "ptz_service_files/RemovePreset.xml", 0);

    output_http_headers(size);

    return cat("stdout", "ptz_service_files/RemovePreset.xml", 0);
}

int ptz_unsupported(const char *method)
{
    if (service_ctx.adv_fault_if_unknown == 1)
        send_action_failed_fault("ptz_service", -1);
    else
        send_empty_response("tptz", (char *) method);
    return -1;
}
