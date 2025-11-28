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
#include "utils.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    long size = cat(NULL, "imaging_service_files/GetServiceCapabilities.xml", 0);
    output_http_headers(size);
    return cat("stdout", "imaging_service_files/GetServiceCapabilities.xml", 0);
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

    char ircut_value[8];
    snprintf(ircut_value, sizeof(ircut_value), "%s", ircut_mode_to_string(entry->ircut_mode));
    const char *effective_token = entry->video_source_token ? entry->video_source_token : "VideoSourceToken";

    long size = cat(NULL, "imaging_service_files/GetImagingSettings.xml", 4, "%VIDEO_SOURCE_TOKEN%", effective_token, "%IRCUT_FILTER%", ircut_value);

    output_http_headers(size);
    return cat("stdout", "imaging_service_files/GetImagingSettings.xml", 4, "%VIDEO_SOURCE_TOKEN%", effective_token, "%IRCUT_FILTER%", ircut_value);
}

static void build_ircut_modes_xml(const imaging_entry_t *entry, char *buffer, size_t buffer_len)
{
    buffer[0] = '\0';
    char *cursor = buffer;
    size_t remaining = buffer_len;

    const struct {
        int supported;
        const char *label;
    } modes[] = {{entry->supports_ircut_on, "On"}, {entry->supports_ircut_off, "Off"}, {entry->supports_ircut_auto, "Auto"}};

    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
        if (!modes[i].supported)
            continue;
        int written = snprintf(cursor, remaining, "        <tt:IrCutFilterModes>%s</tt:IrCutFilterModes>\n", modes[i].label);
        if (written < 0 || (size_t) written >= remaining) {
            cursor[0] = '\0';
            break;
        }
        cursor += written;
        remaining -= (size_t) written;
    }

    if (buffer[0] == '\0') {
        snprintf(buffer,
                 buffer_len,
                 "        <tt:IrCutFilterModes>On</tt:IrCutFilterModes>\n        <tt:IrCutFilterModes>Off</tt:IrCutFilterModes>\n");
    }
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

    char modes_xml[256];
    build_ircut_modes_xml(entry, modes_xml, sizeof(modes_xml));
    const char *effective_token = entry->video_source_token ? entry->video_source_token : "VideoSourceToken";

    long size = cat(NULL, "imaging_service_files/GetOptions.xml", 4, "%VIDEO_SOURCE_TOKEN%", effective_token, "%IRCUT_MODES%", modes_xml);

    output_http_headers(size);
    return cat("stdout", "imaging_service_files/GetOptions.xml", 4, "%VIDEO_SOURCE_TOKEN%", effective_token, "%IRCUT_MODES%", modes_xml);
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

    mxml_node_t *settings_node = get_element_ptr(NULL, "ImagingSettings", "Body");
    if (settings_node == NULL) {
        send_fault("imaging_service", "Sender", "ter:InvalidArgVal", "ter:NoSource", "Missing imaging settings", "ImagingSettings element is required");
        return -1;
    }

    const char *ircut = get_element_in_element("IrCutFilter", settings_node);
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

    long size = cat(NULL, "imaging_service_files/SetImagingSettings.xml", 0);
    output_http_headers(size);
    return cat("stdout", "imaging_service_files/SetImagingSettings.xml", 0);
}

int imaging_unsupported(const char *method)
{
    if (service_ctx.adv_fault_if_unknown == 1)
        send_action_failed_fault("imaging_service", -1);
    else
        send_empty_response("timg", (char *) method);
    return -1;
}
