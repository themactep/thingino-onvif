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

#include "media2_service.h"

#include "conf.h"
#include "fault.h"
#include "log.h"
#include "mxml_wrapper.h"
#include "onvif_simple_server.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>

extern service_context_t service_ctx;

static int media2_audio_decoder_profile_count()
{
    int count = 0;
    for (int i = 0; i < service_ctx.profiles_num; i++) {
        if (service_ctx.profiles[i].audio_decoder != AUDIO_NONE)
            count++;
    }
    return count;
}

static int media2_audio_output_supported()
{
    if (!service_ctx.audio.output_enabled || media2_audio_decoder_profile_count() == 0) {
        send_fault("media2_service",
                   "Receiver",
                   "ter:ActionNotSupported",
                   "ter:AudioOutputNotSupported",
                   "AudioOutputNotSupported",
                   "Audio or Audio Outputs are not supported by the device");
        return 0;
    }
    return 1;
}

int media2_get_service_capabilities()
{
    char cap[256];

    cap[0] = '\0';
    strcat(cap, "VideoSource VideoEncoder");
    if ((service_ctx.profiles[0].audio_encoder != AUDIO_NONE)
        || ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_encoder != AUDIO_NONE))) {
        strcat(cap, " AudioSource AudioEncoder");
    }
    if (service_ctx.audio.output_enabled) {
        strcat(cap, " AudioOutput");
    }
    if ((service_ctx.profiles[0].audio_decoder != AUDIO_NONE)
        || ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_decoder != AUDIO_NONE))) {
        strcat(cap, " AudioDecoder");
    }
    if (service_ctx.ptz_node.enable == 1) {
        strcat(cap, " PTZ");
    }

    long size = cat(NULL, "media2_service_files/GetServiceCapabilities.xml", 2, "%CAPABILITIES%", cap);

    output_http_headers(size);

    return cat("stdout", "media2_service_files/GetServiceCapabilities.xml", 2, "%CAPABILITIES%", cap);
}

int media2_get_profiles()
{
    char profiles_num[8];
    char audio_profiles_num[8];
    char stmp_w_h[16], stmp_h_h[16];
    char stmp_w_l[16], stmp_h_l[16];
    char audio_enc_h[16], audio_enc_l[16];
    char video_enc_h[16], video_enc_l[16];
    const char *profile_token = get_element("Token", "Body");
    mxml_node_t *x_type;
    int n_type;
    char xml_name[MAX_LEN];
    long size;
    int q, h;
    char *h264profile[] = {"High", "Main"};
    char *profile[] = {"Profile_0", "Profile_1"};
    int c;
    char dest_a[] = "stdout";
    char *dest;
    int typeVSC = 0, typeASC = 0, typeVEC = 0, typeAEC = 0, typePTZ = 0, typeAOC = 0, typeADC = 0;
    char min_x[256], max_x[256], min_y[256], max_y[256], min_z[256], max_z[256];

    audio_enc_h[0] = '\0';
    audio_enc_l[0] = '\0';
    video_enc_h[0] = '\0';
    video_enc_l[0] = '\0';
    snprintf(profiles_num, sizeof(profiles_num), "%d", service_ctx.profiles_num);
    snprintf(audio_profiles_num, sizeof(audio_profiles_num), "%d", media2_audio_decoder_profile_count());
    const char *audio_output_name = service_ctx.audio.backchannel.name ? service_ctx.audio.backchannel.name : "";
    const char *audio_output_token = service_ctx.audio.backchannel.token ? service_ctx.audio.backchannel.token : "";
    const char *audio_output_config_token = service_ctx.audio.backchannel.configuration_token ? service_ctx.audio.backchannel.configuration_token
                                                                                              : "";
    char audio_output_level[8];
    snprintf(audio_output_level, sizeof(audio_output_level), "%d", service_ctx.audio.backchannel.output_level);
    sprintf(min_x, "%.1f", service_ctx.ptz_node.min_step_x);
    sprintf(max_x, "%.1f", service_ctx.ptz_node.max_step_x);
    sprintf(min_y, "%.1f", service_ctx.ptz_node.min_step_y);
    sprintf(max_y, "%.1f", service_ctx.ptz_node.max_step_y);
    sprintf(min_z, "%.1f", service_ctx.ptz_node.min_step_z);
    sprintf(max_z, "%.1f", service_ctx.ptz_node.max_step_z);

    x_type = get_element_ptr(NULL, "Type", "Body");
    n_type = 0;
    do {
        if (x_type != NULL) {
            const char *type_text = mxmlGetText(x_type, NULL);
            if (type_text != NULL) {
                if (strstr(type_text, "VideoSource") != NULL)
                    typeVSC = 1;
                if (strstr(type_text, "AudioSource") != NULL)
                    typeASC = 1;
                if (strstr(type_text, "VideoEncoder") != NULL)
                    typeVEC = 1;
                if (strstr(type_text, "AudioEncoder") != NULL)
                    typeAEC = 1;
                if (strstr(type_text, "PTZ") != NULL)
                    typePTZ = 1;
                if (strstr(type_text, "AudioOutput") != NULL)
                    typeAOC = 1;
                if (strstr(type_text, "AudioDecoder") != NULL)
                    typeADC = 1;
                if (strstr(type_text, "All") != NULL) {
                    typeVSC = 1;
                    typeASC = 1;
                    typeVEC = 1;
                    typeAEC = 1;
                    typePTZ = 1;
                    typeAOC = 1;
                    typeADC = 1;
                }
            }
            n_type++;
            x_type = mxmlGetNextSibling(x_type);
        }
    } while (x_type != NULL);

    if (service_ctx.profiles_num == 0) {
        q = 0;
        size = cat(NULL, "media2_service_files/GetProfiles_none.xml", 0);

        output_http_headers(size);

        return cat("stdout", "media2_service_files/GetProfiles_none.xml", 0);

    } else if (profile_token == NULL) {
        if (service_ctx.profiles_num == 1) {
            q = 1;
            h = 0;
        } else if (service_ctx.profiles_num == 2) {
            q = 2;
        }
    } else if ((service_ctx.profiles_num > 0) && (strcasecmp(service_ctx.profiles[0].name, profile_token) == 0)) {
        q = 1;
        h = 0;
    } else if ((service_ctx.profiles_num == 2) && (strcasecmp(service_ctx.profiles[1].name, profile_token) == 0)) {
        q = 1;
        h = 1;
    } else {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile does not exist");
        return -1;
    }

    if (q == 0) {
        size = cat(NULL, "media2_service_files/GetProfiles_none.xml", 0);

        output_http_headers(size);

        return cat("stdout", "media2_service_files/GetProfiles_none.xml", 0);

    } else if (q == 1) {
        // We need 1st step to evaluate content length
        for (c = 0; c < 2; c++) {
            if (c == 0) {
                dest = NULL;
            } else {
                dest = dest_a;
                output_http_headers(size);
            }

            size = cat(dest, "media2_service_files/GetProfiles_header.xml", 2, "%PROFILE%", profile[h]);

            if (n_type > 0) {
                size += cat(dest, "media2_service_files/GetProfiles_confstart.xml", 0);

                if (typeVSC) {
                    // Get VideoSource from Profile_0
                    sprintf(stmp_w_h, "%d", service_ctx.profiles[0].width);
                    sprintf(stmp_h_h, "%d", service_ctx.profiles[0].height);
                    size += cat(dest,
                                "media2_service_files/GetProfiles_VSC.xml",
                                6,
                                "%PROFILES_NUM%",
                                profiles_num,
                                "%VSC_WIDTH%",
                                stmp_w_h,
                                "%VSC_HEIGHT%",
                                stmp_h_h);
                }
                if (typeASC) {
                    if (((service_ctx.profiles_num > 0) && (service_ctx.profiles[0].audio_encoder != AUDIO_NONE))
                        || ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_encoder != AUDIO_NONE))) {
                        size += cat(dest, "media2_service_files/GetProfiles_ASC.xml", 2, "%PROFILES_NUM%", profiles_num);
                    }
                }
                if (typeVEC) {
                    sprintf(stmp_w_h, "%d", service_ctx.profiles[0].width);
                    sprintf(stmp_h_h, "%d", service_ctx.profiles[0].height);
                    set_video_codec(video_enc_h, 16, service_ctx.profiles[h].type, 2);
                    size += cat(dest,
                                "media2_service_files/GetProfiles_VEC.xml",
                                10,
                                "%H264PROFILE%",
                                h264profile[h],
                                "%PROFILE%",
                                profile[h],
                                "%VIDEO_ENCODING%",
                                video_enc_h,
                                "%VEC_WIDTH%",
                                stmp_w_h,
                                "%VEC_HEIGHT%",
                                stmp_h_h);
                }
                if (typeAEC) {
                    if (service_ctx.profiles[h].audio_encoder != AUDIO_NONE) {
                        set_audio_codec(audio_enc_h, 16, service_ctx.profiles[0].audio_encoder, 2);
                        size += cat(dest, "media2_service_files/GetProfiles_AEC.xml", 10, "%PROFILE%", profile[h], "%AUDIO_ENCODING%", audio_enc_h);
                    }
                }
                if (typePTZ) {
                    if (service_ctx.ptz_node.enable == 1) {
                        size += cat(dest, "media2_service_files/GetProfiles_PTZ.xml", 0);
                    }
                }
                if (service_ctx.profiles[h].audio_decoder != AUDIO_NONE) {
                    if (typeAOC && service_ctx.audio.output_enabled) {
                        size += cat(dest,
                                    "media2_service_files/GetProfiles_AOC.xml",
                                    10,
                                    "%PROFILES_NUM%",
                                    audio_profiles_num,
                                    "%AUDIO_OUTPUT_CONFIG_TOKEN%",
                                    audio_output_config_token,
                                    "%AUDIO_OUTPUT_NAME%",
                                    audio_output_name,
                                    "%AUDIO_OUTPUT_TOKEN%",
                                    audio_output_token,
                                    "%AUDIO_OUTPUT_LEVEL%",
                                    audio_output_level);
                    }
                    if (typeADC) {
                        size += cat(dest, "media2_service_files/GetProfiles_ADC.xml", 2, "%PROFILE%", profile[h]);
                    }
                }

                size += cat(dest, "media2_service_files/GetProfiles_confend.xml", 0);
            }
            size += cat(dest, "media2_service_files/GetProfiles_footer.xml", 0);
        }
    } else if (q == 2) {
        // We need 1st step to evaluate content length
        for (c = 0; c < 2; c++) {
            if (c == 0) {
                dest = NULL;
            } else {
                dest = dest_a;
                output_http_headers(size);
            }

            h = 0;
            size = cat(dest, "media2_service_files/GetProfiles_header.xml", 2, "%PROFILE%", profile[h]);

            if (n_type > 0) {
                size += cat(dest, "media2_service_files/GetProfiles_confstart.xml", 0);

                if (typeVSC) {
                    // Get VideoSource from Profile_0
                    sprintf(stmp_w_h, "%d", service_ctx.profiles[0].width);
                    sprintf(stmp_h_h, "%d", service_ctx.profiles[0].height);
                    size += cat(dest,
                                "media2_service_files/GetProfiles_VSC.xml",
                                6,
                                "%PROFILES_NUM%",
                                profiles_num,
                                "%VSC_WIDTH%",
                                stmp_w_h,
                                "%VSC_HEIGHT%",
                                stmp_h_h);
                }
                if (typeASC) {
                    if (((service_ctx.profiles_num > 0) && (service_ctx.profiles[0].audio_encoder != AUDIO_NONE))
                        || ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_encoder != AUDIO_NONE))) {
                        size += cat(dest, "media2_service_files/GetProfiles_ASC.xml", 2, "%PROFILES_NUM%", profiles_num);
                    }
                }
                if (typeVEC) {
                    sprintf(stmp_w_h, "%d", service_ctx.profiles[h].width);
                    sprintf(stmp_h_h, "%d", service_ctx.profiles[h].height);
                    set_video_codec(video_enc_h, 16, service_ctx.profiles[h].type, 2);
                    size += cat(dest,
                                "media2_service_files/GetProfiles_VEC.xml",
                                10,
                                "%H264PROFILE%",
                                h264profile[h],
                                "%PROFILE%",
                                profile[h],
                                "%VIDEO_ENCODING%",
                                video_enc_h,
                                "%VEC_WIDTH%",
                                stmp_w_h,
                                "%VEC_HEIGHT%",
                                stmp_h_h);
                }
                if (typeAEC) {
                    if (service_ctx.profiles[h].audio_encoder != AUDIO_NONE) {
                        set_audio_codec(audio_enc_h, 16, service_ctx.profiles[h].audio_encoder, 2);
                        size += cat(dest, "media2_service_files/GetProfiles_AEC.xml", 10, "%PROFILE%", profile[h], "%AUDIO_ENCODING%", audio_enc_h);
                    }
                }
                if (typePTZ) {
                    if (service_ctx.ptz_node.enable == 1) {
                        size += cat(dest, "media2_service_files/GetProfiles_PTZ.xml", 0);
                    }
                }
                if (service_ctx.profiles[h].audio_decoder != AUDIO_NONE) {
                    if (typeAOC && service_ctx.audio.output_enabled) {
                        size += cat(dest,
                                    "media2_service_files/GetProfiles_AOC.xml",
                                    10,
                                    "%PROFILES_NUM%",
                                    audio_profiles_num,
                                    "%AUDIO_OUTPUT_CONFIG_TOKEN%",
                                    audio_output_config_token,
                                    "%AUDIO_OUTPUT_NAME%",
                                    audio_output_name,
                                    "%AUDIO_OUTPUT_TOKEN%",
                                    audio_output_token,
                                    "%AUDIO_OUTPUT_LEVEL%",
                                    audio_output_level);
                    }
                    if (typeADC) {
                        size += cat(dest, "media2_service_files/GetProfiles_ADC.xml", 2, "%PROFILE%", profile[h]);
                    }
                }

                size += cat(dest, "media2_service_files/GetProfiles_confend.xml", 0);
            }

            h = 1;
            size += cat(dest, "media2_service_files/GetProfiles_middle.xml", 2, "%PROFILE%", profile[h]);

            if (n_type > 0) {
                size += cat(dest, "media2_service_files/GetProfiles_confstart.xml", 0);

                if (typeVSC) {
                    // Get VideoSource from Profile_0
                    sprintf(stmp_w_l, "%d", service_ctx.profiles[0].width);
                    sprintf(stmp_h_l, "%d", service_ctx.profiles[0].height);
                    size += cat(dest,
                                "media2_service_files/GetProfiles_VSC.xml",
                                6,
                                "%PROFILES_NUM%",
                                profiles_num,
                                "%VSC_WIDTH%",
                                stmp_w_l,
                                "%VSC_HEIGHT%",
                                stmp_h_l);
                }
                if (typeASC) {
                    if (((service_ctx.profiles_num > 0) && (service_ctx.profiles[0].audio_encoder != AUDIO_NONE))
                        || ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_encoder != AUDIO_NONE))) {
                        size += cat(dest, "media2_service_files/GetProfiles_ASC.xml", 2, "%PROFILES_NUM%", profiles_num);
                    }
                }
                if (typeVEC) {
                    sprintf(stmp_w_l, "%d", service_ctx.profiles[h].width);
                    sprintf(stmp_h_l, "%d", service_ctx.profiles[h].height);
                    set_video_codec(video_enc_l, 16, service_ctx.profiles[h].type, 2);
                    size += cat(dest,
                                "media2_service_files/GetProfiles_VEC.xml",
                                10,
                                "%H264PROFILE%",
                                h264profile[h],
                                "%PROFILE%",
                                profile[h],
                                "%VIDEO_ENCODING%",
                                video_enc_l,
                                "%VEC_WIDTH%",
                                stmp_w_l,
                                "%VEC_HEIGHT%",
                                stmp_h_l);
                }
                if (typeAEC) {
                    if (service_ctx.profiles[h].audio_encoder != AUDIO_NONE) {
                        set_audio_codec(audio_enc_l, 16, service_ctx.profiles[h].audio_encoder, 2);
                        size += cat(dest, "media2_service_files/GetProfiles_AEC.xml", 10, "%PROFILE%", profile[h], "%AUDIO_ENCODING%", audio_enc_l);
                    }
                }
                if (typePTZ) {
                    if (service_ctx.ptz_node.enable == 1) {
                        size += cat(dest, "media2_service_files/GetProfiles_PTZ.xml", 0);
                    }
                }
                if (service_ctx.profiles[h].audio_decoder != AUDIO_NONE) {
                    if (typeAOC && service_ctx.audio.output_enabled) {
                        size += cat(dest,
                                    "media2_service_files/GetProfiles_AOC.xml",
                                    10,
                                    "%PROFILES_NUM%",
                                    audio_profiles_num,
                                    "%AUDIO_OUTPUT_CONFIG_TOKEN%",
                                    audio_output_config_token,
                                    "%AUDIO_OUTPUT_NAME%",
                                    audio_output_name,
                                    "%AUDIO_OUTPUT_TOKEN%",
                                    audio_output_token,
                                    "%AUDIO_OUTPUT_LEVEL%",
                                    audio_output_level);
                    }
                    if (typeADC) {
                        size += cat(dest, "media2_service_files/GetProfiles_ADC.xml", 2, "%PROFILE%", profile[h]);
                    }
                }

                size += cat(dest, "media2_service_files/GetProfiles_confend.xml", 0);
            }
            size += cat(dest, "media2_service_files/GetProfiles_footer.xml", 0);
        }
    }
}

int media2_get_video_source_modes()
{
    const char *token = get_element("VideoSourceToken", "Body");
    char stmp_w[16], stmp_h[16], video_enc[16];

    if (strcasecmp("VideoSourceToken", token) == 0) {
        // Get data from profile 0
        sprintf(stmp_w, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_h, "%d", service_ctx.profiles[0].height);
        set_video_codec(video_enc, 16, service_ctx.profiles[0].type, 2);
        long size = cat(NULL, "media2_service_files/GetVideoSourceModes.xml", 6, "%WIDTH%", stmp_w, "%HEIGHT%", stmp_h, "%VIDEO_ENCODING%", video_enc);

        output_http_headers(size);

        return cat("stdout", "media2_service_files/GetVideoSourceModes.xml", 6, "%WIDTH%", stmp_w, "%HEIGHT%", stmp_h, "%VIDEO_ENCODING%", video_enc);

    } else {
        send_fault("media2_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoVideoSource",
                   "No video source",
                   "The requested video source does not exist");
        return -1;
    }
}

int media2_get_video_source_configurations()
{
    const char *configuration_token = get_element("ConfigurationToken", "Body");
    const char *profile_token = get_element("ProfileToken", "Body");
    char token[23];
    char profiles_num[2], stmp_w[16], stmp_h[16];

    memset(token, '\0', sizeof(token));
    if (profile_token != NULL) {
        // Extract "Profile_x" from token Profile_x
        strncpy(token, profile_token, 9);
    } else if (configuration_token != NULL) {
        strncpy(token, configuration_token, 22);
    } else if (service_ctx.profiles_num > 0) {
        strncpy(token, service_ctx.profiles[0].name, 9);
    }

    if ((strcasecmp(service_ctx.profiles[0].name, token) == 0) || (strcasecmp(service_ctx.profiles[1].name, token) == 0)
        || (strcasecmp("VideoSourceConfigToken", token) == 0)) {
        sprintf(profiles_num, "%d", service_ctx.profiles_num);
        sprintf(stmp_w, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_h, "%d", service_ctx.profiles[0].height);
        long size = cat(NULL,
                        "media2_service_files/GetVideoSourceConfigurations.xml",
                        6,
                        "%PROFILES_NUM%",
                        profiles_num,
                        "%WIDTH%",
                        stmp_w,
                        "%HEIGHT%",
                        stmp_h);

        output_http_headers(size);

        return cat("stdout",
                   "media2_service_files/GetVideoSourceConfigurations.xml",
                   6,
                   "%PROFILES_NUM%",
                   profiles_num,
                   "%WIDTH%",
                   stmp_w,
                   "%HEIGHT%",
                   stmp_h);
    } else {
        send_fault("media2_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoConfig",
                   "No config",
                   "The requested configuration indicated does not exist");
        return -1;
    }
}

int media2_get_video_source_configuration_options()
{
    const char *configuration_token = get_element("ConfigurationToken", "Body");
    const char *profile_token = get_element("ProfileToken", "Body");
    char token[23];
    char stmp_w[16], stmp_h[16];

    memset(token, '\0', sizeof(token));
    if (profile_token != NULL) {
        // Extract "Profile_x" from token Profile_x
        strncpy(token, profile_token, 9);
    } else if (configuration_token != NULL) {
        strncpy(token, configuration_token, 22);
    } else if (service_ctx.profiles_num > 0) {
        strncpy(token, service_ctx.profiles[0].name, 9);
    }

    if ((strcasecmp(service_ctx.profiles[0].name, token) == 0) || (strcasecmp(service_ctx.profiles[1].name, token) == 0)
        || (strcasecmp("VideoSourceConfigToken", token) == 0)) {
        sprintf(stmp_w, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_h, "%d", service_ctx.profiles[0].height);
        long size = cat(NULL, "media2_service_files/GetVideoSourceConfigurationOptions.xml", 4, "%WIDTH%", stmp_w, "%HEIGHT%", stmp_h);

        output_http_headers(size);

        return cat("stdout", "media2_service_files/GetVideoSourceConfigurationOptions.xml", 4, "%WIDTH%", stmp_w, "%HEIGHT%", stmp_h);

    } else {
        send_fault("media2_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoConfig",
                   "No config",
                   "The requested configuration indicated does not exist");
        return -1;
    }
}

int media2_get_video_encoder_configurations()
{
    char stmp_w_l[16], stmp_h_l[16];
    char stmp_w_h[16], stmp_h_h[16];
    const char *profile_token = get_element("ProfileToken", "Body");
    const char *configuration_token = get_element("ConfigurationToken", "Body");
    char token[10];
    char video_enc_h[16], video_enc_l[16];

    memset(token, '\0', sizeof(token));
    if (configuration_token != NULL) {
        // Extract "Profile_x" from token Profile_x_VideoEncoderConfig
        strncpy(token, configuration_token, 9);
    } else if (profile_token != NULL) {
        // Extract "Profile_x" from token Profile_x
        strncpy(token, profile_token, 9);
    }

    if (token[0] == '\0') {
        if (service_ctx.profiles_num == 1) {
            sprintf(stmp_w_h, "%d", service_ctx.profiles[0].width);
            sprintf(stmp_h_h, "%d", service_ctx.profiles[0].height);
            set_video_codec(video_enc_h, 16, service_ctx.profiles[0].type, 2);
            long size = cat(NULL,
                            "media2_service_files/GetVideoEncoderConfigurations.xml",
                            10,
                            "%H264PROFILE%",
                            "High",
                            "%PROFILE%",
                            "Profile_0",
                            "%WIDTH%",
                            stmp_w_h,
                            "%HEIGHT%",
                            stmp_h_h,
                            "%VIDEO_ENCODING%",
                            video_enc_h);

            output_http_headers(size);

            return cat("stdout",
                       "media2_service_files/GetVideoEncoderConfigurations.xml",
                       10,
                       "%H264PROFILE%",
                       "High",
                       "%PROFILE%",
                       "Profile_0",
                       "%WIDTH%",
                       stmp_w_h,
                       "%HEIGHT%",
                       stmp_h_h,
                       "%VIDEO_ENCODING%",
                       video_enc_h);

        } else if (service_ctx.profiles_num == 2) {
            sprintf(stmp_w_h, "%d", service_ctx.profiles[0].width);
            sprintf(stmp_h_h, "%d", service_ctx.profiles[0].height);
            sprintf(stmp_w_l, "%d", service_ctx.profiles[1].width);
            sprintf(stmp_h_l, "%d", service_ctx.profiles[1].height);
            set_video_codec(video_enc_h, 16, service_ctx.profiles[0].type, 2);
            set_video_codec(video_enc_l, 16, service_ctx.profiles[1].type, 2);
            long size = cat(NULL,
                            "media2_service_files/GetVideoEncoderConfigurations_both.xml",
                            12,
                            "%WIDTH_HIGH%",
                            stmp_w_h,
                            "%HEIGHT_HIGH%",
                            stmp_h_h,
                            "%WIDTH_LOW%",
                            stmp_w_l,
                            "%HEIGHT_LOW%",
                            stmp_h_l,
                            "%VIDEO_ENCODING_HIGH%",
                            video_enc_h,
                            "%VIDEO_ENCODING_LOW%",
                            video_enc_l);

            output_http_headers(size);

            return cat("stdout",
                       "media2_service_files/GetVideoEncoderConfigurations_both.xml",
                       12,
                       "%WIDTH_HIGH%",
                       stmp_w_h,
                       "%HEIGHT_HIGH%",
                       stmp_h_h,
                       "%WIDTH_LOW%",
                       stmp_w_l,
                       "%HEIGHT_LOW%",
                       stmp_h_l,
                       "%VIDEO_ENCODING_HIGH%",
                       video_enc_h,
                       "%VIDEO_ENCODING_LOW%",
                       video_enc_l);
        }
    } else if ((service_ctx.profiles_num > 0) && (strcasecmp(service_ctx.profiles[0].name, token) == 0)) {
        sprintf(stmp_w_h, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_h_h, "%d", service_ctx.profiles[0].height);
        set_video_codec(video_enc_h, 16, service_ctx.profiles[0].type, 2);
        long size = cat(NULL,
                        "media2_service_files/GetVideoEncoderConfigurations.xml",
                        10,
                        "%H264PROFILE%",
                        "High",
                        "%PROFILE%",
                        "Profile_0",
                        "%WIDTH%",
                        stmp_w_h,
                        "%HEIGHT%",
                        stmp_h_h,
                        "%VIDEO_ENCODING%",
                        video_enc_h);

        output_http_headers(size);

        return cat("stdout",
                   "media2_service_files/GetVideoEncoderConfigurations.xml",
                   10,
                   "%H264PROFILE%",
                   "High",
                   "%PROFILE%",
                   "Profile_0",
                   "%WIDTH%",
                   stmp_w_h,
                   "%HEIGHT%",
                   stmp_h_h,
                   "%VIDEO_ENCODING%",
                   video_enc_h);

    } else if ((service_ctx.profiles_num == 2) && (strcasecmp(service_ctx.profiles[1].name, token) == 0)) {
        sprintf(stmp_w_l, "%d", service_ctx.profiles[1].width);
        sprintf(stmp_h_l, "%d", service_ctx.profiles[1].height);
        set_video_codec(video_enc_l, 16, service_ctx.profiles[1].type, 2);
        long size = cat(NULL,
                        "media2_service_files/GetVideoEncoderConfigurations.xml",
                        10,
                        "%H264PROFILE%",
                        "Main",
                        "%PROFILE%",
                        "Profile_1",
                        "%WIDTH%",
                        stmp_w_l,
                        "%HEIGHT%",
                        stmp_h_l,
                        "%VIDEO_ENCODING%",
                        video_enc_l);

        output_http_headers(size);

        return cat("stdout",
                   "media2_service_files/GetVideoEncoderConfigurations.xml",
                   10,
                   "%H264PROFILE%",
                   "Main",
                   "%PROFILE%",
                   "Profile_1",
                   "%WIDTH%",
                   stmp_w_l,
                   "%HEIGHT%",
                   stmp_h_l,
                   "%VIDEO_ENCODING%",
                   video_enc_l);
    } else {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile does not exist");
        return -1;
    }
}

int media2_get_video_encoder_configuration_options()
{
    char stmp_w[16], stmp_h[16];
    const char *configuration_token = get_element("ConfigurationToken", "Body");
    const char *profile_token = get_element("ProfileToken", "Body");
    char token[10];
    char video_enc[16];

    memset(token, '\0', sizeof(token));
    if (configuration_token != NULL) {
        // Extract "Profile_x" from token Profile_x_VideoEncoderConfig
        strncpy(token, configuration_token, 9);
    } else if (profile_token != NULL) {
        // Extract "Profile_x" from token Profile_x
        strncpy(token, profile_token, 9);
    } else if (service_ctx.profiles_num > 0) {
        strncpy(token, service_ctx.profiles[0].name, 9);
    }

    if ((service_ctx.profiles_num > 0) && (strcasecmp(service_ctx.profiles[0].name, token) == 0)) {
        sprintf(stmp_w, "%d", service_ctx.profiles[0].width);
        sprintf(stmp_h, "%d", service_ctx.profiles[0].height);
        set_video_codec(video_enc, 16, service_ctx.profiles[0].type, 2);
        long size = cat(NULL,
                        "media2_service_files/GetVideoEncoderConfigurationOptions.xml",
                        8,
                        "%WIDTH%",
                        stmp_w,
                        "%HEIGHT%",
                        stmp_h,
                        "%H264PROFILE%",
                        "High",
                        "%VIDEO_ENCODING%",
                        video_enc);

        output_http_headers(size);

        return cat("stdout",
                   "media2_service_files/GetVideoEncoderConfigurationOptions.xml",
                   8,
                   "%WIDTH%",
                   stmp_w,
                   "%HEIGHT%",
                   stmp_h,
                   "%H264PROFILE%",
                   "High",
                   "%VIDEO_ENCODING%",
                   video_enc);

    } else if ((service_ctx.profiles_num == 2) && (strcasecmp(service_ctx.profiles[1].name, token) == 0)) {
        sprintf(stmp_w, "%d", service_ctx.profiles[1].width);
        sprintf(stmp_h, "%d", service_ctx.profiles[1].height);
        set_video_codec(video_enc, 16, service_ctx.profiles[1].type, 2);
        long size = cat(NULL,
                        "media2_service_files/GetVideoEncoderConfigurationOptions.xml",
                        8,
                        "%WIDTH%",
                        stmp_w,
                        "%HEIGHT%",
                        stmp_h,
                        "%H264PROFILE%",
                        "Main",
                        "%VIDEO_ENCODING%",
                        video_enc);

        output_http_headers(size);

        return cat("stdout",
                   "media2_service_files/GetVideoEncoderConfigurationOptions.xml",
                   8,
                   "%WIDTH%",
                   stmp_w,
                   "%HEIGHT%",
                   stmp_h,
                   "%H264PROFILE%",
                   "Main",
                   "%VIDEO_ENCODING%",
                   video_enc);

    } else {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile does not exist");
        return -1;
    }
}

int media2_get_audio_source_configurations()
{
    const char *configuration_token = get_element("ConfigurationToken", "Body");
    const char *profile_token = get_element("ProfileToken", "Body");
    char token[23];
    char stmp_w[16], stmp_h[16];
    char s_profiles_num[2];
    int profiles_num;

    memset(token, '\0', sizeof(token));
    if (profile_token != NULL) {
        // Extract "Profile_x" from token Profile_x
        strncpy(token, profile_token, 9);
    } else if (configuration_token != NULL) {
        strncpy(token, configuration_token, 22);
    } else if (service_ctx.profiles_num > 0) {
        strncpy(token, service_ctx.profiles[0].name, 9);
    }

    if ((strcasecmp(service_ctx.profiles[0].name, token) == 0) || (strcasecmp(service_ctx.profiles[1].name, token) == 0)
        || (strcasecmp("AudioSourceConfigToken", token) == 0)) {
        profiles_num = service_ctx.profiles_num;

        if (profiles_num == 1) {
            if (service_ctx.profiles[0].audio_encoder == AUDIO_NONE) {
                profiles_num--;
            }
        } else if (profiles_num == 2) {
            if (service_ctx.profiles[0].audio_encoder == AUDIO_NONE) {
                profiles_num--;
            }
            if (service_ctx.profiles[1].audio_encoder == AUDIO_NONE) {
                profiles_num--;
            }
        }

        sprintf(s_profiles_num, "%d", profiles_num);

        if (profiles_num > 0) {
            long size = cat(NULL, "media2_service_files/GetAudioSourceConfigurations.xml", 2, "%PROFILES_NUM%", s_profiles_num);

            output_http_headers(size);

            return cat("stdout", "media2_service_files/GetAudioSourceConfigurations.xml", 2, "%PROFILES_NUM%", s_profiles_num);
        } else {
            send_fault("media2_service",
                       "Receiver",
                       "ter:ActionNotSupported",
                       "ter:AudioNotSupported",
                       "AudioNotSupported",
                       "The device does not support audio");
            return -1;
        }
    } else {
        send_fault("media2_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoConfig",
                   "No config",
                   "The requested configuration indicated does not exist");
        return -2;
    }
}

int media2_get_audio_source_configuration_options()
{
    const char *configuration_token = get_element("ConfigurationToken", "Body");
    const char *profile_token = get_element("ProfileToken", "Body");
    char token[23];

    memset(token, '\0', sizeof(token));
    if (profile_token != NULL) {
        // Extract "Profile_x" from token Profile_x
        strncpy(token, profile_token, 9);
    } else if (configuration_token != NULL) {
        strncpy(token, configuration_token, 22);
    } else {
        if ((service_ctx.profiles_num > 0) && (service_ctx.profiles[0].audio_encoder != AUDIO_NONE)) {
            strncpy(token, service_ctx.profiles[0].name, 9);
        } else if ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_encoder != AUDIO_NONE)) {
            strncpy(token, service_ctx.profiles[1].name, 9);
        } else {
            send_fault("media2_service",
                       "Receiver",
                       "ter:ActionNotSupported",
                       "ter:AudioNotSupported",
                       "AudioNotSupported",
                       "The device does not support audio");
            return -1;
        }
    }

    if (((service_ctx.profiles_num > 0) && (service_ctx.profiles[0].audio_encoder != AUDIO_NONE))
        || ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_encoder != AUDIO_NONE))) {
        if (((service_ctx.profiles_num > 0) && (strcasecmp(service_ctx.profiles[0].name, token) == 0))
            || ((service_ctx.profiles_num == 2) && (strcasecmp(service_ctx.profiles[1].name, token) == 0))
            || (strcasecmp("AudioSourceConfigToken", token) == 0)) {
            long size = cat(NULL, "media2_service_files/GetAudioSourceConfigurationOptions.xml", 0);

            output_http_headers(size);

            return cat("stdout", "media2_service_files/GetAudioSourceConfigurationOptions.xml", 0);
        } else {
            send_fault("media2_service",
                       "Sender",
                       "ter:InvalidArgVal",
                       "ter:NoConfig",
                       "No config",
                       "The requested configuration indicated does not exist");
            return -2;
        }
    } else {
        send_fault("media2_service",
                   "Receiver",
                   "ter:ActionNotSupported",
                   "ter:AudioNotSupported",
                   "AudioNotSupported",
                   "The device does not support audio");
        return -3;
    }
}

int media2_get_audio_encoder_configurations()
{
    char audio_enc_h[16];
    char audio_enc_l[16];
    const char *profile_token = get_element("ProfileToken", "Body");
    const char *configuration_token = get_element("ConfigurationToken", "Body");
    char token[10];

    memset(token, '\0', sizeof(token));
    if (configuration_token != NULL) {
        // Extract "Profile_x" from token Profile_x_VideoEncoderConfig
        strncpy(token, configuration_token, 9);
    } else if (profile_token != NULL) {
        // Extract "Profile_x" from token Profile_x
        strncpy(token, profile_token, 9);
    }

    if (((service_ctx.profiles_num == 0) && (service_ctx.profiles[0].audio_encoder == AUDIO_NONE))
        || ((service_ctx.profiles_num == 2) && (service_ctx.profiles[0].audio_encoder == AUDIO_NONE)
            && (service_ctx.profiles[1].audio_encoder == AUDIO_NONE))) {
        send_fault("media2_service",
                   "Receiver",
                   "ter:ActionNotSupported",
                   "ter:AudioNotSupported",
                   "AudioNotSupported",
                   "The device does not support audio");
        return -1;
    }

    if (token[0] == '\0') {
        if (service_ctx.profiles_num == 1) {
            if (service_ctx.profiles[0].audio_encoder != AUDIO_NONE) {
                set_audio_codec(audio_enc_h, 16, service_ctx.profiles[0].audio_encoder, 2);
                long size = cat(NULL,
                                "media2_service_files/GetAudioEncoderConfigurations.xml",
                                4,
                                "%PROFILE%",
                                "Profile_0",
                                "%AUDIO_ENCODING%",
                                audio_enc_h);

                output_http_headers(size);

                return cat("stdout",
                           "media2_service_files/GetAudioEncoderConfigurations.xml",
                           4,
                           "%PROFILE%",
                           "Profile_0",
                           "%AUDIO_ENCODING%",
                           audio_enc_h);

            } else {
                send_fault("media2_service",
                           "Receiver",
                           "ter:ActionNotSupported",
                           "ter:AudioNotSupported",
                           "AudioNotSupported",
                           "The device does not support audio");
                return -2;
            }

        } else if (service_ctx.profiles_num == 2) {
            if ((service_ctx.profiles[0].audio_encoder != AUDIO_NONE) && (service_ctx.profiles[1].audio_encoder != AUDIO_NONE)) {
                set_audio_codec(audio_enc_h, 16, service_ctx.profiles[0].audio_encoder, 2);
                set_audio_codec(audio_enc_l, 16, service_ctx.profiles[1].audio_encoder, 2);
                long size = cat(NULL,
                                "media2_service_files/GetAudioEncoderConfigurations_both.xml",
                                4,
                                "%AUDIO_ENCODING_HIGH%",
                                audio_enc_h,
                                "%AUDIO_ENCODING_LOW%",
                                audio_enc_l);

                output_http_headers(size);

                return cat("stdout",
                           "media2_service_files/GetAudioEncoderConfigurations_both.xml",
                           4,
                           "%AUDIO_ENCODING_HIGH%",
                           audio_enc_h,
                           "%AUDIO_ENCODING_LOW%",
                           audio_enc_l);

            } else if (service_ctx.profiles[0].audio_encoder != AUDIO_NONE) {
                set_audio_codec(audio_enc_h, 16, service_ctx.profiles[0].audio_encoder, 2);
                set_audio_codec(audio_enc_l, 16, service_ctx.profiles[1].audio_encoder, 2);
                long size = cat(NULL,
                                "media2_service_files/GetAudioEncoderConfigurations.xml",
                                4,
                                "%PROFILE%",
                                "Profile_0",
                                "%AUDIO_ENCODING%",
                                audio_enc_h);

                output_http_headers(size);

                return cat("stdout",
                           "media2_service_files/GetAudioEncoderConfigurations.xml",
                           4,
                           "%PROFILE%",
                           "Profile_0",
                           "%AUDIO_ENCODING%",
                           audio_enc_h);

            } else if (service_ctx.profiles[1].audio_encoder != AUDIO_NONE) {
                set_audio_codec(audio_enc_h, 16, service_ctx.profiles[0].audio_encoder, 2);
                set_audio_codec(audio_enc_l, 16, service_ctx.profiles[1].audio_encoder, 2);
                long size = cat(NULL,
                                "media2_service_files/GetAudioEncoderConfigurations.xml",
                                4,
                                "%PROFILE%",
                                "Profile_1",
                                "%AUDIO_ENCODING%",
                                audio_enc_l);

                output_http_headers(size);

                return cat("stdout",
                           "media2_service_files/GetAudioEncoderConfigurations.xml",
                           4,
                           "%PROFILE%",
                           "Profile_1",
                           "%AUDIO_ENCODING%",
                           audio_enc_l);

            } else {
                send_fault("media2_service",
                           "Receiver",
                           "ter:ActionNotSupported",
                           "ter:AudioNotSupported",
                           "AudioNotSupported",
                           "The device does not support audio");
                return -3;
            }
        }
    } else if ((service_ctx.profiles_num > 0) && (strcasecmp(service_ctx.profiles[0].name, token) == 0)) {
        if (service_ctx.profiles[0].audio_encoder != AUDIO_NONE) {
            set_audio_codec(audio_enc_h, 16, service_ctx.profiles[0].audio_encoder, 2);
            long size
                = cat(NULL, "media2_service_files/GetAudioEncoderConfigurations.xml", 4, "%PROFILE%", "Profile_0", "%AUDIO_ENCODING%", audio_enc_h);

            output_http_headers(size);

            return cat("stdout",
                       "media2_service_files/GetAudioEncoderConfigurations.xml",
                       4,
                       "%PROFILE%",
                       "Profile_0",
                       "%AUDIO_ENCODING%",
                       audio_enc_h);
        } else {
            send_fault("media2_service",
                       "Receiver",
                       "ter:ActionNotSupported",
                       "ter:AudioNotSupported",
                       "AudioNotSupported",
                       "The device does not support audio");
            return -4;
        }

    } else if ((service_ctx.profiles_num == 2) && (strcasecmp(service_ctx.profiles[1].name, token) == 0)) {
        if (service_ctx.profiles[1].audio_encoder != AUDIO_NONE) {
            set_audio_codec(audio_enc_l, 16, service_ctx.profiles[1].audio_encoder, 2);
            long size
                = cat(NULL, "media2_service_files/GetAudioEncoderConfigurations.xml", 4, "%PROFILE%", "Profile_1", "%AUDIO_ENCODING%", audio_enc_l);

            output_http_headers(size);

            return cat("stdout",
                       "media2_service_files/GetAudioEncoderConfigurations.xml",
                       4,
                       "%PROFILE%",
                       "Profile_1",
                       "%AUDIO_ENCODING%",
                       audio_enc_l);
        } else {
            send_fault("media2_service",
                       "Receiver",
                       "ter:ActionNotSupported",
                       "ter:AudioNotSupported",
                       "AudioNotSupported",
                       "The device does not support audio");
            return -5;
        }
    } else {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile does not exist");
        return -6;
    }
}

int media2_get_audio_encoder_configuration_options()
{
    char audio_enc[16];
    const char *configuration_token = get_element("ConfigurationToken", "Body");
    const char *profile_token = get_element("ProfileToken", "Body");
    char token[10];

    char bitrate[4], samplerate[4];

    memset(token, '\0', sizeof(token));
    if (configuration_token != NULL) {
        // Extract "Profile_x" from token Profile_x_VideoEncoderConfig
        strncpy(token, configuration_token, 9);
    } else if (profile_token != NULL) {
        // Extract "Profile_x" from token Profile_x
        strncpy(token, profile_token, 9);
    } else {
        if ((service_ctx.profiles_num > 0) && (service_ctx.profiles[0].audio_encoder != AUDIO_NONE)) {
            strncpy(token, service_ctx.profiles[0].name, 9);
        } else if ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_encoder != AUDIO_NONE)) {
            strncpy(token, service_ctx.profiles[1].name, 9);
        } else {
            send_fault("media2_service",
                       "Receiver",
                       "ter:ActionNotSupported",
                       "ter:AudioNotSupported",
                       "AudioNotSupported",
                       "The device does not support audio");
            return -1;
        }
    }

    if ((service_ctx.profiles_num > 0) && (strcasecmp(service_ctx.profiles[0].name, token) == 0)) {
        // G726 is not suppoerted
        if ((service_ctx.profiles[0].audio_encoder != AUDIO_NONE) && (service_ctx.profiles[0].audio_encoder != G726)) {
            set_audio_codec(audio_enc, 16, service_ctx.profiles[0].audio_encoder, 2);

            if (service_ctx.profiles[0].audio_encoder == G711) {
                sprintf(bitrate, "%d", 64);
                sprintf(samplerate, "%d", 8);
            } else if (service_ctx.profiles[0].audio_encoder == AAC) {
                sprintf(bitrate, "%d", 50);
                sprintf(samplerate, "%d", 16);
            }
        } else {
            send_fault("media2_service",
                       "Receiver",
                       "ter:ActionNotSupported",
                       "ter:AudioNotSupported",
                       "AudioNotSupported",
                       "The device does not support audio");
            return -2;
        }
    } else if ((service_ctx.profiles_num == 2) && (strcasecmp(service_ctx.profiles[1].name, token) == 0)) {
        // G726 is not suppoerted
        if ((service_ctx.profiles[1].audio_encoder != AUDIO_NONE) && (service_ctx.profiles[1].audio_encoder != G726)) {
            set_audio_codec(audio_enc, 16, service_ctx.profiles[1].audio_encoder, 2);

            if (service_ctx.profiles[1].audio_encoder == G711) {
                sprintf(bitrate, "%d", 64);
                sprintf(samplerate, "%d", 8);
            } else if (service_ctx.profiles[1].audio_encoder == AAC) {
                sprintf(bitrate, "%d", 50);
                sprintf(samplerate, "%d", 16);
            }
        } else {
            send_fault("media2_service",
                       "Receiver",
                       "ter:ActionNotSupported",
                       "ter:AudioNotSupported",
                       "AudioNotSupported",
                       "The device does not support audio");
            return -3;
        }
    } else {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile does not exist");
        return -4;
    }

    long size = cat(NULL,
                    "media2_service_files/GetAudioEncoderConfigurationOptions.xml",
                    6,
                    "%AUDIO_ENCODING%",
                    audio_enc,
                    "%BITRATE%",
                    bitrate,
                    "%SAMPLERATE%",
                    samplerate);

    output_http_headers(size);

    return cat("stdout",
               "media2_service_files/GetAudioEncoderConfigurationOptions.xml",
               6,
               "%AUDIO_ENCODING%",
               audio_enc,
               "%BITRATE%",
               bitrate,
               "%SAMPLERATE%",
               samplerate);
}

int media2_get_audio_output_configurations()
{
    const char *configuration_token = get_element("ConfigurationToken", "Body");
    const char *profile_token = get_element("ProfileToken", "Body");
    char token[23];
    char stmp_w[16], stmp_h[16];
    char s_profiles_num[8];
    int profiles_num;

    memset(token, '\0', sizeof(token));
    if (profile_token != NULL) {
        // Extract "Profile_x" from token Profile_x
        strncpy(token, profile_token, 9);
    } else if (configuration_token != NULL) {
        strncpy(token, configuration_token, 22);
    } else if (service_ctx.profiles_num > 0) {
        strncpy(token, service_ctx.profiles[0].name, 9);
    }

    if ((strcasecmp(service_ctx.profiles[0].name, token) == 0) || (strcasecmp(service_ctx.profiles[1].name, token) == 0)
        || (strcasecmp("AudioOutputConfigToken", token) == 0)) {
        if (!media2_audio_output_supported())
            return -1;

        profiles_num = media2_audio_decoder_profile_count();
        if (profiles_num <= 0) {
            send_fault("media2_service",
                       "Receiver",
                       "ter:ActionNotSupported",
                       "ter:AudioOutputNotSupported",
                       "AudioOutputNotSupported",
                       "Audio or Audio Outputs are not supported by the device");
            return -1;
        }

        snprintf(s_profiles_num, sizeof(s_profiles_num), "%d", profiles_num);
        const char *config_token = service_ctx.audio.backchannel.configuration_token ? service_ctx.audio.backchannel.configuration_token : "";
        const char *name = service_ctx.audio.backchannel.name ? service_ctx.audio.backchannel.name : "";
        const char *output_token = service_ctx.audio.backchannel.token ? service_ctx.audio.backchannel.token : "";
        char output_level[8];
        snprintf(output_level, sizeof(output_level), "%d", service_ctx.audio.backchannel.output_level);

        long size = cat(NULL,
                        "media2_service_files/GetAudioOutputConfigurations.xml",
                        10,
                        "%PROFILES_NUM%",
                        s_profiles_num,
                        "%AUDIO_OUTPUT_CONFIG_TOKEN%",
                        config_token,
                        "%AUDIO_OUTPUT_NAME%",
                        name,
                        "%AUDIO_OUTPUT_TOKEN%",
                        output_token,
                        "%AUDIO_OUTPUT_LEVEL%",
                        output_level);

        output_http_headers(size);

        return cat("stdout",
                   "media2_service_files/GetAudioOutputConfigurations.xml",
                   10,
                   "%PROFILES_NUM%",
                   s_profiles_num,
                   "%AUDIO_OUTPUT_CONFIG_TOKEN%",
                   config_token,
                   "%AUDIO_OUTPUT_NAME%",
                   name,
                   "%AUDIO_OUTPUT_TOKEN%",
                   output_token,
                   "%AUDIO_OUTPUT_LEVEL%",
                   output_level);
    } else {
        send_fault("media2_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoConfig",
                   "No config",
                   "The requested configuration indicated does not exist");
        return -2;
    }
}

int media2_get_audio_output_configuration_options()
{
    const char *configuration_token = get_element("ConfigurationToken", "Body");
    const char *profile_token = get_element("ProfileToken", "Body");
    char token[23];

    memset(token, '\0', sizeof(token));
    if (profile_token != NULL) {
        // Extract "Profile_x" from token Profile_x
        strncpy(token, profile_token, 9);
    } else if (configuration_token != NULL) {
        strncpy(token, configuration_token, 22);
    } else {
        if ((service_ctx.profiles_num > 0) && (service_ctx.profiles[0].audio_decoder != AUDIO_NONE)) {
            strncpy(token, service_ctx.profiles[0].name, 9);
        } else if ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_decoder != AUDIO_NONE)) {
            strncpy(token, service_ctx.profiles[1].name, 9);
        } else {
            send_fault("media2_service",
                       "Receiver",
                       "ter:ActionNotSupported",
                       "ter:AudioOutputNotSupported",
                       "AudioOutputNotSupported",
                       "Audio or Audio Outputs are not supported by the device");
            return -1;
        }
    }

    if (!media2_audio_output_supported())
        return -3;

    if (((service_ctx.profiles_num > 0) && (strcasecmp(service_ctx.profiles[0].name, token) == 0))
        || ((service_ctx.profiles_num == 2) && (strcasecmp(service_ctx.profiles[1].name, token) == 0))
        || (strcasecmp("AudioOutputConfigToken", token) == 0)) {
        const char *output_token = service_ctx.audio.backchannel.token ? service_ctx.audio.backchannel.token : "";
        char min_level[8];
        char max_level[8];
        snprintf(min_level, sizeof(min_level), "%d", service_ctx.audio.backchannel.output_level_min);
        snprintf(max_level, sizeof(max_level), "%d", service_ctx.audio.backchannel.output_level_max);

        long size = cat(NULL,
                        "media2_service_files/GetAudioOutputConfigurationOptions.xml",
                        6,
                        "%AUDIO_OUTPUT_TOKEN%",
                        output_token,
                        "%AUDIO_OUTPUT_LEVEL_MIN%",
                        min_level,
                        "%AUDIO_OUTPUT_LEVEL_MAX%",
                        max_level);

        output_http_headers(size);

        return cat("stdout",
                   "media2_service_files/GetAudioOutputConfigurationOptions.xml",
                   6,
                   "%AUDIO_OUTPUT_TOKEN%",
                   output_token,
                   "%AUDIO_OUTPUT_LEVEL_MIN%",
                   min_level,
                   "%AUDIO_OUTPUT_LEVEL_MAX%",
                   max_level);
    } else {
        send_fault("media2_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoConfig",
                   "No config",
                   "The requested configuration indicated does not exist");
        return -2;
    }
}

int media2_get_audio_decoder_configurations()
{
    const char *profile_token = get_element("ProfileToken", "Body");
    const char *configuration_token = get_element("ConfigurationToken", "Body");
    char token[10];

    memset(token, '\0', sizeof(token));
    if (configuration_token != NULL) {
        // Extract "Profile_x" from token Profile_x_VideoDecoderConfig
        strncpy(token, configuration_token, 9);
    } else if (profile_token != NULL) {
        // Extract "Profile_x" from token Profile_x
        strncpy(token, profile_token, 9);
    }

    if (token[0] == '\0') {
        if (service_ctx.profiles_num == 1) {
            if (service_ctx.profiles[0].audio_decoder != AUDIO_NONE) {
                long size = cat(NULL, "media2_service_files/GetAudioDecoderConfigurations.xml", 2, "%PROFILE%", "Profile_0");

                output_http_headers(size);

                return cat("stdout", "media2_service_files/GetAudioDecoderConfigurations.xml", 2, "%PROFILE%", "Profile_0");
            } else {
                send_fault("media2_service2",
                           "Receiver",
                           "ter:ActionNotSupported",
                           "ter:AudioDecodingNotSupported",
                           "AudioDecodingNotSupported",
                           "Audio or Audio decoding is not supported by the device");
                return -1;
            }
        } else if (service_ctx.profiles_num == 2) {
            if ((service_ctx.profiles[0].audio_decoder != AUDIO_NONE) && (service_ctx.profiles[1].audio_decoder != AUDIO_NONE)) {
                long size = cat(NULL, "media2_service_files/GetAudioDecoderConfigurations_both.xml", 0);

                output_http_headers(size);

                return cat("stdout", "media2_service_files/GetAudioDecoderConfigurations_both.xml", 0);

            } else if (service_ctx.profiles[0].audio_decoder != AUDIO_NONE) {
                long size = cat(NULL, "media2_service_files/GetAudioDecoderConfigurations.xml", 2, "%PROFILE%", "Profile_0");

                output_http_headers(size);

                return cat("stdout", "media2_service_files/GetAudioDecoderConfigurations.xml", 2, "%PROFILE%", "Profile_0");
            } else if (service_ctx.profiles[1].audio_decoder != AUDIO_NONE) {
                long size = cat(NULL, "media2_service_files/GetAudioDecoderConfigurations.xml", 2, "%PROFILE%", "Profile_1");

                output_http_headers(size);

                return cat("stdout", "media2_service_files/GetAudioDecoderConfigurations.xml", 2, "%PROFILE%", "Profile_1");
            } else {
                send_fault("media2_service2",
                           "Receiver",
                           "ter:ActionNotSupported",
                           "ter:AudioDecodingNotSupported",
                           "AudioDecodingNotSupported",
                           "Audio or Audio decoding is not supported by the device");
                return -2;
            }
        }
    } else if ((service_ctx.profiles_num > 0) && (strcasecmp(service_ctx.profiles[0].name, token) == 0)) {
        if (service_ctx.profiles[0].audio_decoder != AUDIO_NONE) {
            long size = cat(NULL, "media2_service_files/GetAudioDecoderConfigurations.xml", 2, "%PROFILE%", "Profile_0");

            output_http_headers(size);

            return cat("stdout", "media2_service_files/GetAudioDecoderConfigurations.xml", 2, "%PROFILE%", "Profile_0");
        } else {
            send_fault("media2_service2",
                       "Receiver",
                       "ter:ActionNotSupported",
                       "ter:AudioDecodingNotSupported",
                       "AudioDecodingNotSupported",
                       "Audio or Audio decoding is not supported by the device");
            return -3;
        }
    } else if ((service_ctx.profiles_num == 2) && (strcasecmp(service_ctx.profiles[1].name, token) == 0)) {
        if (service_ctx.profiles[1].audio_decoder != AUDIO_NONE) {
            long size = cat(NULL, "media2_service_files/GetAudioDecoderConfigurations.xml", 2, "%PROFILE%", "Profile_1");

            output_http_headers(size);

            return cat("stdout", "media2_service_files/GetAudioDecoderConfigurations.xml", 2, "%PROFILE%", "Profile_1");
        } else {
            send_fault("media2_service2",
                       "Receiver",
                       "ter:ActionNotSupported",
                       "ter:AudioDecodingNotSupported",
                       "AudioDecodingNotSupported",
                       "Audio or Audio decoding is not supported by the device");
            return -4;
        }
    } else {
        send_fault("media2_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:NoConfig",
                   "No config",
                   "The requested configuration indicated does not exist");
        return -5;
    }
}

int media2_get_audio_decoder_configuration_options()
{
    int decoder_type = AUDIO_NONE;
    const char *configuration_token = get_element("ConfigurationToken", "Body");
    const char *profile_token = get_element("ProfileToken", "Body");
    char token[10];
    char audio_decoder[16];
    char bitrate[4], samplerate[4];

    memset(token, '\0', sizeof(token));
    if (configuration_token != NULL) {
        // Extract "Profile_x" from token Profile_x_VideoEncoderConfig
        strncpy(token, configuration_token, 9);
    } else if (profile_token != NULL) {
        // Extract "Profile_x" from token Profile_x
        strncpy(token, profile_token, 9);
    } else {
        if ((service_ctx.profiles_num > 0) && (service_ctx.profiles[0].audio_decoder != AUDIO_NONE)) {
            strncpy(token, service_ctx.profiles[0].name, 9);
        } else if ((service_ctx.profiles_num == 2) && (service_ctx.profiles[1].audio_decoder != AUDIO_NONE)) {
            strncpy(token, service_ctx.profiles[1].name, 9);
        } else {
            send_fault("media2_service",
                       "Receiver",
                       "ter:ActionNotSupported",
                       "ter:AudioDecodingNotSupported",
                       "AudioDecodingNotSupported",
                       "Audio or Audio decoding is not supported by the device");
            return -1;
        }
    }

    if ((service_ctx.profiles_num > 0) && (strcasecmp(service_ctx.profiles[0].name, token) == 0)) {
        decoder_type = service_ctx.profiles[0].audio_decoder;
    } else if ((service_ctx.profiles_num == 2) && (strcasecmp(service_ctx.profiles[1].name, token) == 0)) {
        decoder_type = service_ctx.profiles[1].audio_decoder;
    }

    // G726 is not suppoerted
    if ((decoder_type != AUDIO_NONE) && (decoder_type != G726)) {
        set_audio_codec(audio_decoder, 16, decoder_type, 2);
        if (decoder_type == G711) {
            sprintf(bitrate, "%d", 64);
            sprintf(samplerate, "%d", 8);
        } else if (decoder_type == AAC) {
            sprintf(bitrate, "%d", 50);
            sprintf(samplerate, "%d", 16);
        }

        long size = cat(NULL,
                        "media2_service_files/GetAudioDecoderConfigurationOptions.xml",
                        6,
                        "%AUDIO_DECODING%",
                        audio_decoder,
                        "%BITRATE%",
                        bitrate,
                        "%SAMPLERATE%",
                        samplerate);

        output_http_headers(size);

        return cat("stdout",
                   "media2_service_files/GetAudioDecoderConfigurationOptions.xml",
                   6,
                   "%AUDIO_DECODING%",
                   audio_decoder,
                   "%BITRATE%",
                   bitrate,
                   "%SAMPLERATE%",
                   samplerate);

    } else {
        send_fault("media2_service",
                   "Receiver",
                   "ter:ActionNotSupported",
                   "ter:AudioDecodingNotSupported",
                   "AudioDecodingNotSupported",
                   "Audio or Audio decoding is not supported by the device");
        return -2;
    }
}

int media2_get_snapshot_uri()
{
    char address[16];
    char netmask[16];
    char *s;
    char line[MAX_LEN];
    const char *profile_token = get_element("ProfileToken", "Body");

    memset(line, '\0', sizeof(line));

    get_ip_address(address, netmask, service_ctx.ifs);

    if (profile_token == NULL) {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile does not exist");
        return -1;
    }

    if ((service_ctx.profiles_num > 0) && (strcasecmp(service_ctx.profiles[0].name, profile_token) == 0)) {
        if (service_ctx.profiles[0].snapurl == NULL) {
            send_fault(
                "media2_service",
                "Receiver",
                "ter:Action",
                "ter:IncompleteConfiguration",
                "Incomplete configuration",
                "The specified media profile does not contain either a reference to a video encoder configuration or a reference to a video source configuration");
            return -2;
        }

        // Construct snapshot URL with embedded credentials if configured
        construct_uri_with_credentials(line, MAX_LEN, service_ctx.profiles[0].snapurl, address, service_ctx.username, service_ctx.password);

        // Escape html chars
        html_escape(line, MAX_LEN);

        long size = cat(NULL, "media2_service_files/GetSnapshotUri.xml", 2, "%URI%", line);

        output_http_headers(size);

        return cat("stdout", "media2_service_files/GetSnapshotUri.xml", 2, "%URI%", line);

    } else if ((service_ctx.profiles_num == 2) && (strcasecmp(service_ctx.profiles[1].name, profile_token) == 0)) {
        if (service_ctx.profiles[1].snapurl == NULL) {
            send_fault(
                "media2_service",
                "Receiver",
                "ter:Action",
                "ter:IncompleteConfiguration",
                "Incomplete configuration",
                "The specified media profile does not contain either a reference to a video encoder configuration or a reference to a video source configuration");
            return -3;
        }

        // Construct snapshot URL with embedded credentials if configured
        construct_uri_with_credentials(line, MAX_LEN, service_ctx.profiles[1].snapurl, address, service_ctx.username, service_ctx.password);

        // Escape html chars
        html_escape(line, MAX_LEN);

        long size = cat(NULL, "media2_service_files/GetSnapshotUri.xml", 2, "%URI%", line);

        output_http_headers(size);

        return cat("stdout", "media2_service_files/GetSnapshotUri.xml", 2, "%URI%", line);

    } else {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile does not exist");
        return -4;
    }
}

int media2_get_stream_uri()
{
    char address[16];
    char netmask[16];
    char *s;
    char line[MAX_LEN];
    const char *profile_token = get_element("ProfileToken", "Body");

    memset(line, '\0', sizeof(line));

    get_ip_address(address, netmask, service_ctx.ifs);

    if (profile_token == NULL) {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile does not exist");
        return -1;
    }

    if (strcasecmp(service_ctx.profiles[0].name, profile_token) == 0) {
        if (service_ctx.profiles[0].url == NULL) {
            send_fault("media2_service",
                       "Receiver",
                       "ter:Action",
                       "ter:IncompleteConfiguration",
                       "Incomplete configuration",
                       "The specified media profile does contain either unused sources or encoder configurations without a corresponding source");
            return -2;
        }

        // Construct RTSP URL with embedded credentials if configured
        construct_uri_with_credentials(line, MAX_LEN, service_ctx.profiles[0].url, address, service_ctx.username, service_ctx.password);

        // Escape html chars
        html_escape(line, MAX_LEN);

        long size = cat(NULL, "media2_service_files/GetStreamUri.xml", 2, "%URI%", line);

        output_http_headers(size);

        return cat("stdout", "media2_service_files/GetStreamUri.xml", 2, "%URI%", line);

    } else if ((service_ctx.profiles_num == 2) && (strcasecmp(service_ctx.profiles[1].name, profile_token) == 0)) {
        if (service_ctx.profiles[1].url == NULL) {
            send_fault("media2_service",
                       "Receiver",
                       "ter:Action",
                       "ter:IncompleteConfiguration",
                       "Incomplete configuration",
                       "The specified media profile does contain either unused sources or encoder configurations without a corresponding source");
            return -3;
        }

        // Construct RTSP URL with embedded credentials if configured
        construct_uri_with_credentials(line, MAX_LEN, service_ctx.profiles[1].url, address, service_ctx.username, service_ctx.password);

        // Escape html chars
        html_escape(line, MAX_LEN);

        long size = cat(NULL, "media2_service_files/GetStreamUri.xml", 2, "%URI%", line);

        output_http_headers(size);

        return cat("stdout", "media2_service_files/GetStreamUri.xml", 2, "%URI%", line);

    } else {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoProfile", "No profile", "The requested profile does not exist");
        return -4;
    }
}

int media2_set_video_source_configuration()
{
    const char *token = NULL;
    mxml_node_t *node;

    node = get_element_ptr(NULL, "Configuration", "Body");
    if (node != NULL) {
        token = get_attribute(node, "token");
    }

    if ((node == NULL) || (token == NULL)) {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The configuration does not exist");
        return -1;
    }

    if (strcasecmp("VideoSourceConfigToken", token) == 0) {
        send_fault("media2_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:ConfigModify",
                   "Config modify",
                   "The configuration parameters are not possible to set");
        return -2;
    } else {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The configuration does not exist");
        return -3;
    }
}

int media2_set_audio_source_configuration()
{
    const char *token = NULL;
    mxml_node_t *node;

    node = get_element_ptr(NULL, "Configuration", "Body");
    if (node != NULL) {
        token = get_attribute(node, "token");
    }

    if ((node == NULL) || (token == NULL)) {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The configuration does not exist");
        return -1;
    }

    if (strcasecmp("AudioSourceConfigToken", token) == 0) {
        send_fault("media2_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:ConfigModify",
                   "Config modify",
                   "The configuration parameters are not possible to set");
        return -2;
    } else {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The configuration does not exist");
        return -3;
    }
}

int media2_set_video_encoder_configuration()
{
    send_fault("media2_service",
               "Sender",
               "ter:InvalidArgVal",
               "ter:ConfigModify",
               "Config modify",
               "The configuration parameters are not possible to set");
    return -1;
}

int media2_set_audio_encoder_configuration()
{
    send_fault("media2_service",
               "Sender",
               "ter:InvalidArgVal",
               "ter:ConfigModify",
               "Config modify",
               "The configuration parameters are not possible to set");
    return -1;
}

int media2_set_audio_output_configuration()
{
    const char *token = NULL;
    mxml_node_t *node;

    node = get_element_ptr(NULL, "Configuration", "Body");
    if (node != NULL) {
        token = get_attribute(node, "token");
    }

    if ((node == NULL) || (token == NULL)) {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The configuration does not exist");
        return -1;
    }

    if (strcasecmp("AudioOutputConfigToken", token) == 0) {
        send_fault("media2_service",
                   "Sender",
                   "ter:InvalidArgVal",
                   "ter:ConfigModify",
                   "Config modify",
                   "The configuration parameters are not possible to set");
        return -2;
    } else {
        send_fault("media2_service", "Sender", "ter:InvalidArgVal", "ter:NoConfig", "No config", "The configuration does not exist");
        return -3;
    }
}

int media2_unsupported(const char *method)
{
    if (service_ctx.adv_fault_if_unknown == 1)
        send_action_failed_fault("media2_service", -1);
    else
        send_empty_response("tr2", (char *) method);
    return -1;
}
