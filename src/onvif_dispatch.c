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

#include "onvif_dispatch.h"

#include "conf.h"
#include "device_service.h"
#include "deviceio_service.h"
#include "events_service.h"
#include "fault.h"
#include "imaging_service.h"
#include "log.h"
#include "media2_service.h"
#include "media_service.h"
#include "onvif_simple_server.h"
#include "ptz_service.h"

#include <stdio.h>
#include <string.h>

// Condition functions for conditional methods
static int condition_adv_fault_if_set(void)
{
    return service_ctx.adv_fault_if_set == 1;
}

static int condition_adv_enable_media2(void)
{
    return service_ctx.adv_enable_media2 == 1;
}

static int condition_synology_nvr(void)
{
    return service_ctx.adv_synology_nvr == 1;
}

// Dispatch table - clean, maintainable, and extensible
static const onvif_method_entry_t onvif_dispatch_table[] = {
    // Device service methods (no conditions)
    {"device_service", "GetServices", device_get_services, NULL},
    {"device_service", "GetServiceCapabilities", device_get_service_capabilities, NULL},
    {"device_service", "GetDeviceInformation", device_get_device_information, NULL},
    {"device_service", "GetSystemDateAndTime", device_get_system_date_and_time, NULL},
    {"device_service", "SystemReboot", device_system_reboot, NULL},
    {"device_service", "GetScopes", device_get_scopes, NULL},
    {"device_service", "GetUsers", device_get_users, NULL},
    {"device_service", "GetWsdlUrl", device_get_wsdl_url, NULL},
    {"device_service", "GetHostname", device_get_hostname, NULL},
    {"device_service", "GetEndpointReference", device_get_endpoint_reference, NULL},
    {"device_service", "GetCapabilities", device_get_capabilities, NULL},
    {"device_service", "GetNetworkInterfaces", device_get_network_interfaces, NULL},
    {"device_service", "GetDiscoveryMode", device_get_discovery_mode, NULL},

    // DeviceIO service methods (no conditions)
    {"deviceio_service", "GetVideoSources", deviceio_get_video_sources, NULL},
    {"deviceio_service", "GetServiceCapabilities", deviceio_get_service_capabilities, NULL},
    {"deviceio_service", "GetAudioOutputs", deviceio_get_audio_outputs, NULL},
    {"deviceio_service", "GetAudioSources", deviceio_get_audio_sources, NULL},
    {"deviceio_service", "GetRelayOutputs", deviceio_get_relay_outputs, NULL},
    {"deviceio_service", "GetRelayOutputOptions", deviceio_get_relay_output_options, NULL},
    {"deviceio_service", "SetRelayOutputSettings", deviceio_set_relay_output_settings, NULL},
    {"deviceio_service", "SetRelayOutputState", deviceio_set_relay_output_state, NULL},

    // Media service methods (no conditions)
    {"media_service", "GetServiceCapabilities", media_get_service_capabilities, NULL},
    {"media_service", "GetVideoSources", media_get_video_sources, NULL},
    {"media_service", "GetVideoSourceConfigurations", media_get_video_source_configurations, NULL},
    {"media_service", "GetVideoSourceConfiguration", media_get_video_source_configuration, NULL},
    {"media_service", "GetCompatibleVideoSourceConfigurations", media_get_compatible_video_source_configurations, NULL},
    {"media_service", "GetVideoSourceConfigurationOptions", media_get_video_source_configuration_options, NULL},
    {"media_service", "GetProfiles", media_get_profiles, NULL},
    {"media_service", "GetProfile", media_get_profile, NULL},
    {"media_service", "CreateProfile", media_create_profile, NULL},
    {"media_service", "GetVideoEncoderConfigurations", media_get_video_encoder_configurations, NULL},
    {"media_service", "GetVideoEncoderConfiguration", media_get_video_encoder_configuration, NULL},
    {"media_service", "GetCompatibleVideoEncoderConfigurations", media_get_compatible_video_encoder_configurations, NULL},
    {"media_service", "GetGuaranteedNumberOfVideoEncoderInstances", media_get_guaranteed_number_of_video_encoder_instances, NULL},
    {"media_service", "GetVideoEncoderConfigurationOptions", media_get_video_encoder_configuration_options, NULL},
    {"media_service", "GetSnapshotUri", media_get_snapshot_uri, NULL},
    {"media_service", "GetStreamUri", media_get_stream_uri, NULL},
    {"media_service", "GetAudioSources", media_get_audio_sources, NULL},
    {"media_service", "GetAudioSourceConfigurations", media_get_audio_source_configurations, NULL},
    {"media_service", "GetAudioSourceConfiguration", media_get_audio_source_configuration, NULL},
    {"media_service", "GetAudioSourceConfigurationOptions", media_get_audio_source_configuration_options, NULL},
    {"media_service", "GetAudioEncoderConfiguration", media_get_audio_encoder_configuration, NULL},
    {"media_service", "GetAudioEncoderConfigurations", media_get_audio_encoder_configurations, NULL},
    {"media_service", "GetAudioEncoderConfigurationOptions", media_get_audio_encoder_configuration_options, NULL},
    {"media_service", "GetAudioDecoderConfiguration", media_get_audio_decoder_configuration, NULL},
    {"media_service", "GetAudioDecoderConfigurations", media_get_audio_decoder_configurations, NULL},
    {"media_service", "GetAudioDecoderConfigurationOptions", media_get_audio_decoder_configuration_options, NULL},
    {"media_service", "GetAudioOutputs", media_get_audio_outputs, NULL},
    {"media_service", "GetAudioOutputConfiguration", media_get_audio_output_configuration, NULL},
    {"media_service", "GetAudioOutputConfigurations", media_get_audio_output_configurations, NULL},
    {"media_service", "GetAudioOutputConfigurationOptions", media_get_audio_output_configuration_options, NULL},
    {"media_service", "GetCompatibleAudioSourceConfigurations", media_get_compatible_audio_source_configurations, NULL},
    {"media_service", "GetCompatibleAudioEncoderConfigurations", media_get_compatible_audio_encoder_configurations, NULL},
    {"media_service", "GetCompatibleAudioDecoderConfigurations", media_get_compatible_audio_decoder_configurations, NULL},
    {"media_service", "GetCompatibleAudioOutputConfigurations", media_get_compatible_audio_output_configurations, NULL},
    // Media service SET methods (conditional on adv_fault_if_set)
    {"media_service", "SetVideoSourceConfiguration", media_set_video_source_configuration, condition_adv_fault_if_set},
    {"media_service", "SetAudioSourceConfiguration", media_set_audio_source_configuration, condition_adv_fault_if_set},
    {"media_service", "SetVideoEncoderConfiguration", media_set_video_encoder_configuration, condition_adv_fault_if_set},
    {"media_service", "SetAudioEncoderConfiguration", media_set_audio_encoder_configuration, condition_adv_fault_if_set},
    {"media_service", "SetAudioOutputConfiguration", media_set_audio_output_configuration, condition_adv_fault_if_set},

    // Imaging service methods (exposed only when configured)
    {"imaging_service", "GetServiceCapabilities", imaging_get_service_capabilities, NULL},
    {"imaging_service", "GetImagingSettings", imaging_get_imaging_settings, NULL},
    {"imaging_service", "GetOptions", imaging_get_options, NULL},
    {"imaging_service", "SetImagingSettings", imaging_set_imaging_settings, NULL},
    {"imaging_service", "Move", imaging_move, NULL},
    {"imaging_service", "GetMoveOptions", imaging_get_move_options, NULL},
    {"imaging_service", "Stop", imaging_stop, NULL},
    {"imaging_service", "GetStatus", imaging_get_status, NULL},
    {"imaging_service", "GetPresets", imaging_get_presets, NULL},
    {"imaging_service", "GetCurrentPreset", imaging_get_current_preset, NULL},
    {"imaging_service", "SetCurrentPreset", imaging_set_current_preset, NULL},

    // PTZ service methods (no conditions)
    {"ptz_service", "GetServiceCapabilities", ptz_get_service_capabilities, NULL},
    {"ptz_service", "GetConfigurations", ptz_get_configurations, NULL},
    {"ptz_service", "GetConfiguration", ptz_get_configuration, NULL},
    {"ptz_service", "GetConfigurationOptions", ptz_get_configuration_options, NULL},
    {"ptz_service", "GetNodes", ptz_get_nodes, NULL},
    {"ptz_service", "GetNode", ptz_get_node, NULL},
    {"ptz_service", "GetPresets", ptz_get_presets, NULL},
    {"ptz_service", "GotoPreset", ptz_goto_preset, NULL},
    {"ptz_service", "GotoHomePosition", ptz_goto_home_position, NULL},
    {"ptz_service", "ContinuousMove", ptz_continuous_move, NULL},
    {"ptz_service", "RelativeMove", ptz_relative_move, NULL},
    {"ptz_service", "SendAuxiliaryCommand", ptz_send_auxiliary_command, NULL},
    {"ptz_service", "MoveAndStartTracking", ptz_move_and_start_tracking, NULL},
    // Preset Tours
    {"ptz_service", "GetPresetTours", ptz_get_preset_tours, NULL},
    {"ptz_service", "GetPresetTour", ptz_get_preset_tour, NULL},
    {"ptz_service", "GetPresetTourOptions", ptz_get_preset_tour_options, NULL},
    {"ptz_service", "CreatePresetTour", ptz_create_preset_tour, NULL},
    {"ptz_service", "ModifyPresetTour", ptz_modify_preset_tour, NULL},
    {"ptz_service", "OperatePresetTour", ptz_operate_preset_tour, NULL},
    {"ptz_service", "RemovePresetTour", ptz_remove_preset_tour, NULL},
    {"ptz_service", "AbsoluteMove", ptz_absolute_move, NULL},
    {"ptz_service", "Stop", ptz_stop, NULL},
    {"ptz_service", "GetStatus", ptz_get_status, NULL},
    {"ptz_service", "SetPreset", ptz_set_preset, NULL},
    {"ptz_service", "SetHomePosition", ptz_set_home_position, NULL},
    {"ptz_service", "RemovePreset", ptz_remove_preset, NULL},

    // Events service methods (no conditions)
    {"events_service", "GetServiceCapabilities", events_get_service_capabilities, NULL},
    {"events_service", "CreatePullPointSubscription", events_create_pull_point_subscription, NULL},
    {"events_service", "PullMessages", events_pull_messages, NULL},
    {"events_service", "Subscribe", events_subscribe, NULL},
    {"events_service", "Renew", events_renew, NULL},
    {"events_service", "Unsubscribe", events_unsubscribe, NULL},
    {"events_service", "GetEventProperties", events_get_event_properties, NULL},
    {"events_service", "SetSynchronizationPoint", events_set_synchronization_point, NULL},

    // Media2 service methods (conditional on adv_enable_media2)
    {"media2_service", "GetServiceCapabilities", media2_get_service_capabilities, condition_adv_enable_media2},
    {"media2_service", "GetProfiles", media2_get_profiles, condition_adv_enable_media2},
    {"media2_service", "GetVideoSourceModes", media2_get_video_source_modes, condition_adv_enable_media2},
    {"media2_service", "GetVideoSourceConfigurations", media2_get_video_source_configurations, condition_adv_enable_media2},
    {"media2_service", "GetVideoSourceConfigurationOptions", media2_get_video_source_configuration_options, condition_adv_enable_media2},
    {"media2_service", "GetVideoEncoderConfigurations", media2_get_video_encoder_configurations, condition_adv_enable_media2},
    {"media2_service", "GetVideoEncoderConfigurationOptions", media2_get_video_encoder_configuration_options, condition_adv_enable_media2},
    {"media2_service", "GetAudioSourceConfigurations", media2_get_audio_source_configurations, condition_adv_enable_media2},
    {"media2_service", "GetAudioSourceConfigurationOptions", media2_get_audio_source_configuration_options, condition_adv_enable_media2},
    {"media2_service", "GetAudioEncoderConfigurations", media2_get_audio_encoder_configurations, condition_adv_enable_media2},
    {"media2_service", "GetAudioEncoderConfigurationOptions", media2_get_audio_encoder_configuration_options, condition_adv_enable_media2},
    {"media2_service", "GetAudioOutputConfigurations", media2_get_audio_output_configurations, condition_adv_enable_media2},
    {"media2_service", "GetAudioOutputConfigurationOptions", media2_get_audio_output_configuration_options, condition_adv_enable_media2},
    {"media2_service", "GetAudioDecoderConfigurations", media2_get_audio_decoder_configurations, condition_adv_enable_media2},
    {"media2_service", "GetAudioDecoderConfigurationOptions", media2_get_audio_decoder_configuration_options, condition_adv_enable_media2},
    {"media2_service", "GetSnapshotUri", media2_get_snapshot_uri, condition_adv_enable_media2},
    {"media2_service", "GetStreamUri", media2_get_stream_uri, condition_adv_enable_media2},

    // Sentinel entry - marks end of table
    {NULL, NULL, NULL, NULL}};

int onvif_dispatch_init(void)
{
    return 0;
}

void onvif_dispatch_cleanup(void)
{
    // No cleanup needed for dispatch table
}

int dispatch_onvif_method(const char *service, const char *method)
{
    if (!service || !method) {
        return -1;
    }

    // Search the dispatch table
    for (const onvif_method_entry_t *entry = onvif_dispatch_table; entry->service != NULL; entry++) {
        if (strcasecmp(entry->service, service) == 0 && strcasecmp(entry->method, method) == 0) {
            // Check condition if one exists
            if (entry->condition != NULL) {
                if (!entry->condition()) {
                    continue; // Condition not met, skip this handler
                }
            }

            int result = entry->handler();
            return result;
        }
    }

    // No handler found - check for special cases first

    // Special case: Synology NVR CreateProfile hack
    if ((service_ctx.adv_synology_nvr == 1) && (strcasecmp("media_service", service) == 0) && (strcasecmp("CreateProfile", method) == 0)) {
        send_fault("media_service",
                   "Receiver",
                   "ter:Action",
                   "ter:MaxNVTProfiles",
                   "Max profile number reached",
                   "The maximum number of supported profiles supported by the device has been reached");
        return 0;
    }

    // Call appropriate unsupported method handler
    if (strcasecmp("device_service", service) == 0) {
        return device_unsupported(method);
    } else if (strcasecmp("media_service", service) == 0) {
        return media_unsupported(method);
    } else if (strcasecmp("media2_service", service) == 0) {
        return media2_unsupported(method);
    } else if (strcasecmp("deviceio_service", service) == 0) {
        return deviceio_unsupported(method);
    } else if (strcasecmp("ptz_service", service) == 0) {
        return ptz_unsupported(method);
    } else if (strcasecmp("events_service", service) == 0) {
        return events_unsupported(method);
    } else if (strcasecmp("imaging_service", service) == 0) {
        return imaging_unsupported(method);
    } else {
        return device_unsupported(method); // Default fallback
    }
}
