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

#include "onvif_simple_server.h"

#include "conf.h"
#include "device_service.h"
#include "deviceio_service.h"
#include "events_service.h"
#include "ezxml_wrapper.h"
#include "fault.h"
#include "log.h"
#include "media2_service.h"
#include "media_service.h"
#include "ptz_service.h"
#include "utils.h"

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

#define DEFAULT_JSON_CONF_FILE "/etc/onvif.json"

service_context_t service_ctx;

int debug = 0;

void dump_env()
{
    log_debug("Dump environment variables");
    log_debug("AUTH_TYPE: %s", getenv("AUTH_TYPE"));
    log_debug("CONTENT_LENGTH: %s", getenv("CONTENT_LENGTH"));
    log_debug("CONTENT_TYPE: %s", getenv("CONTENT_TYPE"));
    log_debug("DOCUMENT_ROOT: %s", getenv("DOCUMENT_ROOT"));
    log_debug("GATEWAY_INTERFACE: %s", getenv("GATEWAY_INTERFACE"));
    log_debug("HTTP_ACCEPT: %s", getenv("HTTP_ACCEPT"));
    log_debug("HTTP_COOKIE: %s", getenv("HTTP_COOKIE"));
    log_debug("HTTP_FROM: %s", getenv("HTTP_FROM"));
    log_debug("HTTP_REFERER: %s", getenv("HTTP_REFERER"));
    log_debug("HTTP_USER_AGENT: %s", getenv("HTTP_USER_AGENT"));
    log_debug("PATH_INFO: %s", getenv("PATH_INFO"));
    log_debug("PATH_TRANSLATED: %s", getenv("PATH_TRANSLATED"));
    log_debug("QUERY_STRING: %s", getenv("QUERY_STRING"));
    log_debug("REMOTE_ADDR: %s", getenv("REMOTE_ADDR"));
    log_debug("REMOTE_HOST: %s", getenv("REMOTE_HOST"));
    log_debug("REMOTE_PORT: %s", getenv("REMOTE_PORT"));
    log_debug("REMOTE_IDENT: %s", getenv("REMOTE_IDENT"));
    log_debug("REMOTE_USER: %s", getenv("REMOTE_USER"));
    log_debug("REQUEST_METHOD: %s", getenv("REQUEST_METHOD"));
    log_debug("REQUEST_URI: %s", getenv("REQUEST_URI"));
    log_debug("SCRIPT_FILENAME: %s", getenv("SCRIPT_FILENAME"));
    log_debug("SCRIPT_NAME: %s", getenv("SCRIPT_NAME"));
    log_debug("SERVER_NAME: %s", getenv("SERVER_NAME"));
    log_debug("SERVER_PORT: %s", getenv("SERVER_PORT"));
    log_debug("SERVER_PROTOCOL: %s", getenv("SERVER_PROTOCOL"));
    log_debug("SERVER_SOFTWARE: %s\n", getenv("SERVER_SOFTWARE"));
}

void print_usage(char* progname)
{
    fprintf(stderr, "\nUsage: %s [-c JSON_CONF_FILE] [-d]\n\n", progname);
    fprintf(stderr, "\t-c JSON_CONF_FILE, --conf_file JSON_CONF_FILE\n");
    fprintf(stderr, "\t\tpath of the JSON configuration file (default %s)\n", DEFAULT_JSON_CONF_FILE);
    fprintf(stderr, "\t-d LEVEL, --debug LEVEL\n");
    fprintf(stderr, "\t\tverbosity 0..5 (0=FATAL only, 5=TRACE; default 0)\n");
    fprintf(stderr, "\t-h, --help\n");
    fprintf(stderr, "\t\tprint this help\n");
}

int main(int argc, char** argv)
{
    char* tmp;
    int errno;
    char* endptr;
    int c, ret, i, itmp;
    int debug_cli_set = 0;
    char* conf_file;
    char* prog_name;
    const char* method;
    username_token_t security;
    int auth_error = 0;

    conf_file = (char*) malloc((strlen(DEFAULT_JSON_CONF_FILE) + 1) * sizeof(char));
    strcpy(conf_file, DEFAULT_JSON_CONF_FILE);

    while (1) {
        static struct option long_options[] = {{"conf_file", required_argument, 0, 'c'},
                                               {"debug", required_argument, 0, 'd'},
                                               {"help", no_argument, 0, 'h'},
                                               {0, 0, 0, 0}};
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long(argc, argv, "c:d:h", long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c) {
        case 'c':
            /* Check for various possible errors */
            if (strlen(optarg) < MAX_LEN - 1) {
                free(conf_file);
                conf_file = (char*) malloc((strlen(optarg) + 1) * sizeof(char));
                strcpy(conf_file, optarg);
            } else {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'd':
            debug = strtol(optarg, &endptr, 10);

            /* Check for various possible errors */
            if ((errno == ERANGE && (debug == LONG_MAX || debug == LONG_MIN)) || (errno != 0 && debug == 0)) {
                print_usage(argv[0]);
                free(conf_file);
                exit(EXIT_FAILURE);
            }
            if (endptr == optarg) {
                print_usage(argv[0]);
                free(conf_file);
                exit(EXIT_FAILURE);
            }

            if ((debug < LOG_LVL_FATAL) || (debug > LOG_LVL_TRACE)) {
                print_usage(argv[0]);
                free(conf_file);
                exit(EXIT_FAILURE);
            }
            /* level set directly: 0=FATAL..5=TRACE */
            debug_cli_set = 1;
            break;

        case 'h':
            print_usage(argv[0]);
            free(conf_file);
            exit(EXIT_SUCCESS);
            break;

        case '?':
            /* getopt_long already printed an error message. */
            break;

        default:
            print_usage(argv[0]);
            free(conf_file);
            exit(EXIT_SUCCESS);
        }
    }

    // Check if the service name is sent as a last argument
    if (argc > 1) {
        tmp = argv[argc - 1];
        if ((strstr(tmp, "device_service") != NULL) || (strstr(tmp, "media_service") != NULL) || (strstr(tmp, "media2_service") != NULL)
            || (strstr(tmp, "ptz_service") != NULL) || (strstr(tmp, "events_service") != NULL) || (strstr(tmp, "deviceio_service") != NULL)) {
            tmp = argv[argc - 1];
        } else {
            tmp = argv[0];
        }
    } else {
        tmp = argv[0];
    }
    prog_name = basename(tmp);

    if (conf_file[0] == '\0') {
        print_usage(argv[0]);
        free(conf_file);
        exit(EXIT_SUCCESS);
    }
    if (strlen(conf_file) <= 5) {
        print_usage(argv[0]);
        free(conf_file);
        exit(EXIT_SUCCESS);
    }

    log_init("onvif_simple_server", LOG_DAEMON, debug, 1);
    log_info("Starting program.");

    dump_env();

    log_info("Processing configuration file %s...", conf_file);
    itmp = process_json_conf_file(conf_file);
    if (itmp == -1) {
        log_fatal("Unable to find configuration file %s", conf_file);

        free(conf_file);
        exit(EXIT_FAILURE);
    } else if (itmp < -1) {
        log_fatal("Wrong syntax in configuration file %s", conf_file);

        free(conf_file);
        exit(EXIT_FAILURE);
    }
    log_info("Completed.");

    // Apply log level from config if CLI -d was not provided
    if (!debug_cli_set) {
        if (service_ctx.loglevel >= LOG_LVL_FATAL && service_ctx.loglevel <= LOG_LVL_TRACE) {
            log_set_level(service_ctx.loglevel);
            debug = service_ctx.loglevel;
            log_info("Log level set from config: %d", debug);
        }
    }
    // Configure utils to log raw responses if enabled in config
    utils_set_raw_xml_log_file(service_ctx.raw_xml_log_file);

    tmp = getenv("REQUEST_METHOD");
    if ((tmp == NULL) || (strcmp("POST", tmp) != 0)) {
        fprintf(stdout, "Content-type: text/html\r\n");
        fprintf(stdout, "Content-Length: 86\r\n\r\n");
        fprintf(stdout, "<html><head><title>Error</title></head><body>HTTP method not supported</body></html>\r\n");
        log_fatal("HTTP method not supported");

        free(conf_file);
        exit(EXIT_FAILURE);
    }
    int input_size;
    char* input = (char*) malloc(16 * 1024 * sizeof(char));
    log_debug("Malloc finished,input pointer points to %p", input);
    if (input == NULL) {
        log_fatal("Memory error");

        free(conf_file);
        exit(EXIT_FAILURE);
    }
    input_size = fread(input, 1, 16 * 1024 * sizeof(char), stdin);
    if (input_size == 0) {
        log_fatal("Error: input is empty");
        free(input);

        free(conf_file);
        exit(EXIT_FAILURE);
    }
    input = (char*) realloc(input, input_size * sizeof(char));
    //realloc returns new address which is not always the same as "input", if not updated the old pointer address causes segmentation fault when trying to use teh variable
    log_debug("Realloc finished,input pointer is now pointing to %p", input);
    if (input == NULL) {
        log_fatal("Memory error trying to allocate %d bytes", input_size);
        free(input);

        free(conf_file);
        exit(EXIT_FAILURE);
    }
    // Raw request logging to file if configured
    if (service_ctx.raw_xml_log_file && service_ctx.raw_xml_log_file[0] != '\0') {
        // Reset response logging flag for this new request
        utils_reset_response_logging();
        FILE* rf = fopen(service_ctx.raw_xml_log_file, "a");
        if (rf) {
            const char* hdr = "==== REQUEST BEGIN ====\n";
            fwrite(hdr, 1, strlen(hdr), rf);
            fwrite(input, 1, (size_t) input_size, rf);
            const char nl = '\n';
            fwrite(&nl, 1, 1, rf);
            fclose(rf);
        } else {
            log_warn("Cannot open raw_xml_log_file %s: %s", service_ctx.raw_xml_log_file, strerror(errno));
        }
    }
    log_debug("Url: %s", prog_name);

    // Warning: init_xml changes the input string
    init_xml(input, input_size);
    method = get_method(1);
    if (method == NULL) {
        log_fatal("XML parsing error");
        close_xml();
        free(input);

        free(conf_file);
        exit(EXIT_FAILURE);
    }

    log_debug("Method: %s", method);

    if (service_ctx.username != NULL) {
        if ((get_element("Security", "Header") != NULL) && (get_element("UsernameToken", "Header") != NULL)) {
            unsigned long nonce_size = 128;
            unsigned long auth_size = 128;
            unsigned long sha1_size = 20;
            unsigned long digest_size = 128;
            unsigned char nonce[128];
            unsigned char auth[128];
            char sha1[20];
            char digest[128];

            security.enable = 1;
            security.username = get_element("Username", "Header");
            if (security.username != NULL) {
                log_debug("Security: username = %s", security.username);
            } else {
                auth_error = 1;
            }
            security.password = get_element("Password", "Header");
            if (security.password != NULL) {
                log_debug("Security: password = ********");
            } else {
                auth_error = 2;
            }
            security.nonce = get_element("Nonce", "Header");
            if (security.nonce != NULL) {
                log_debug("Security: nonce = %s", security.nonce);
                b64_decode((unsigned char*) security.nonce, strlen(security.nonce), nonce, &nonce_size);
            } else {
                auth_error = 3;
            }
            security.created = get_element("Created", "Header");
            if (security.created != NULL) {
                log_debug("Security: created = %s", security.created);
            } else {
                auth_error = 4;
            }

            if (auth_error == 0) {
                // Calculate digest and check the password
                // Digest = B64ENCODE( SHA1( B64DECODE( Nonce ) + Date + Password ) )
                memcpy(auth, nonce, nonce_size);
                memcpy(&auth[nonce_size], security.created, strlen(security.created));
                memcpy(&auth[nonce_size + strlen(security.created)], service_ctx.password, strlen(service_ctx.password));
                auth_size = nonce_size + strlen(security.created) + strlen(service_ctx.password);
                hashSHA1(auth, auth_size, sha1, sha1_size);
                b64_encode(sha1, sha1_size, digest, &digest_size);
                log_debug("Calculated digest: %s", digest);
                log_debug("Received digest: %s", security.password);

                if ((strcmp(service_ctx.username, security.username) != 0) || (strcmp(security.password, digest) != 0)) {
                    auth_error = 10;
                }
            }
        } else {
            auth_error = 11;
        }
    } else {
        security.enable = 0;
    }

    if ((strcasecmp("device_service", prog_name) == 0)
        && ((strcasecmp("GetSystemDateAndTime", method) == 0) || (strcasecmp("GetUsers", method) == 0) || (strcasecmp("GetCapabilities", method) == 0)
            || (strcasecmp("GetServices", method) == 0) || (strcasecmp("GetServiceCapabilities", method) == 0))) {
        auth_error = 0;
    }

    if (security.enable == 1) {
        if (auth_error == 0)
            log_info("Authentication ok");
        else
            log_error("Authentication error");
    }

    if (auth_error == 0) {
        if (strcasecmp("device_service", prog_name) == 0) {
            if (strcasecmp(method, "GetServices") == 0) {
                device_get_services();
            } else if (strcasecmp(method, "GetServiceCapabilities") == 0) {
                device_get_service_capabilities();
            } else if (strcasecmp(method, "GetDeviceInformation") == 0) {
                device_get_device_information();
            } else if (strcasecmp(method, "GetSystemDateAndTime") == 0) {
                device_get_system_date_and_time();
            } else if (strcasecmp(method, "SystemReboot") == 0) {
                device_system_reboot();
            } else if (strcasecmp(method, "GetScopes") == 0) {
                device_get_scopes();
            } else if (strcasecmp(method, "GetUsers") == 0) {
                device_get_users();
            } else if (strcasecmp(method, "GetWsdlUrl") == 0) {
                device_get_wsdl_url();
            } else if (strcasecmp(method, "GetCapabilities") == 0) {
                device_get_capabilities();
            } else if (strcasecmp(method, "GetNetworkInterfaces") == 0) {
                device_get_network_interfaces();
            } else if (strcasecmp(method, "GetDiscoveryMode") == 0) {
                device_get_discovery_mode();
            } else {
                device_unsupported(method);
            }
        } else if (strcasecmp("media_service", prog_name) == 0) {
            if (strcasecmp(method, "GetServiceCapabilities") == 0) {
                media_get_service_capabilities();
            } else if (strcasecmp(method, "GetVideoSources") == 0) {
                media_get_video_sources();
            } else if (strcasecmp(method, "GetVideoSourceConfigurations") == 0) {
                media_get_video_source_configurations();
            } else if (strcasecmp(method, "GetVideoSourceConfiguration") == 0) {
                media_get_video_source_configuration();
            } else if (strcasecmp(method, "GetCompatibleVideoSourceConfigurations") == 0) {
                media_get_compatible_video_source_configurations();
            } else if (strcasecmp(method, "GetVideoSourceConfigurationOptions") == 0) {
                media_get_video_source_configuration_options();
            } else if (strcasecmp(method, "GetProfiles") == 0) {
                media_get_profiles();
            } else if (strcasecmp(method, "GetProfile") == 0) {
                media_get_profile();
            } else if (strcasecmp(method, "CreateProfile") == 0) {
                media_create_profile();
            } else if (strcasecmp(method, "GetVideoEncoderConfigurations") == 0) {
                media_get_video_encoder_configurations();
            } else if (strcasecmp(method, "GetVideoEncoderConfiguration") == 0) {
                media_get_video_encoder_configuration();
            } else if (strcasecmp(method, "GetCompatibleVideoEncoderConfigurations") == 0) {
                media_get_compatible_video_encoder_configurations();
            } else if (strcasecmp(method, "GetVideoEncoderConfigurationOptions") == 0) {
                media_get_video_encoder_configuration_options();
            } else if (strcasecmp(method, "GetGuaranteedNumberOfVideoEncoderInstances") == 0) {
                media_get_guaranteed_number_of_video_encoder_instances();
            } else if (strcasecmp(method, "GetSnapshotUri") == 0) {
                media_get_snapshot_uri();
            } else if (strcasecmp(method, "GetStreamUri") == 0) {
                media_get_stream_uri();
            } else if (strcasecmp(method, "GetAudioSources") == 0) {
                media_get_audio_sources();
            } else if (strcasecmp(method, "GetAudioSourceConfigurations") == 0) {
                media_get_audio_source_configurations();
            } else if (strcasecmp(method, "GetAudioSourceConfiguration") == 0) {
                media_get_audio_source_configuration();
            } else if (strcasecmp(method, "GetAudioSourceConfigurationOptions") == 0) {
                media_get_audio_source_configuration_options();
            } else if (strcasecmp(method, "GetAudioEncoderConfigurations") == 0) {
                media_get_audio_encoder_configurations();
            } else if (strcasecmp(method, "GetAudioEncoderConfiguration") == 0) {
                media_get_audio_encoder_configuration();
            } else if (strcasecmp(method, "GetAudioEncoderConfigurationOptions") == 0) {
                media_get_audio_encoder_configuration_options();
            } else if (strcasecmp(method, "GetAudioDecoderConfigurations") == 0) {
                media_get_audio_decoder_configurations();
            } else if (strcasecmp(method, "GetAudioDecoderConfiguration") == 0) {
                media_get_audio_decoder_configuration();
            } else if (strcasecmp(method, "GetAudioDecoderConfigurationOptions") == 0) {
                media_get_audio_decoder_configuration_options();
            } else if (strcasecmp(method, "GetAudioOutputs") == 0) {
                media_get_audio_outputs();
            } else if (strcasecmp(method, "GetAudioOutputConfiguration") == 0) {
                media_get_audio_output_configuration();
            } else if (strcasecmp(method, "GetAudioOutputConfigurations") == 0) {
                media_get_audio_output_configurations();
            } else if (strcasecmp(method, "GetAudioOutputConfigurationOptions") == 0) {
                media_get_audio_output_configuration_options();
            } else if (strcasecmp(method, "GetCompatibleAudioSourceConfigurations") == 0) {
                media_get_compatible_audio_source_configurations();
            } else if (strcasecmp(method, "GetCompatibleAudioEncoderConfigurations") == 0) {
                media_get_compatible_audio_encoder_configurations();
            } else if (strcasecmp(method, "GetCompatibleAudioDecoderConfigurations") == 0) {
                media_get_compatible_audio_decoder_configurations();
            } else if (strcasecmp(method, "GetCompatibleAudioOutputConfigurations") == 0) {
                media_get_compatible_audio_output_configurations();
            } else if ((service_ctx.adv_fault_if_set == 1) && (strcasecmp(method, "SetVideoSourceConfiguration") == 0)) {
                media_set_video_source_configuration();
            } else if ((service_ctx.adv_fault_if_set == 1) && (strcasecmp(method, "SetAudioSourceConfiguration") == 0)) {
                media_set_audio_source_configuration();
            } else if ((service_ctx.adv_fault_if_set == 1) && (strcasecmp(method, "SetVideoEncoderConfiguration") == 0)) {
                media_set_video_encoder_configuration();
            } else if ((service_ctx.adv_fault_if_set == 1) && (strcasecmp(method, "SetAudioEncoderConfiguration") == 0)) {
                media_set_audio_encoder_configuration();
            } else if ((service_ctx.adv_fault_if_set == 1) && (strcasecmp(method, "SetAudioOutputConfiguration") == 0)) {
                media_set_audio_output_configuration();
            } else {
                media_unsupported(method);
            }
        } else if ((service_ctx.adv_enable_media2 == 1) && (strcasecmp("media2_service", prog_name) == 0)) {
            if (strcasecmp(method, "GetServiceCapabilities") == 0) {
                media2_get_service_capabilities();
            } else if (strcasecmp(method, "GetProfiles") == 0) {
                media2_get_profiles();
            } else if (strcasecmp(method, "GetVideoSourceModes") == 0) {
                media2_get_video_source_modes();
            } else if (strcasecmp(method, "GetVideoSourceConfigurations") == 0) {
                media2_get_video_source_configurations();
            } else if (strcasecmp(method, "GetVideoSourceConfigurationOptions") == 0) {
                media2_get_video_source_configuration_options();
            } else if (strcasecmp(method, "GetVideoEncoderConfigurations") == 0) {
                media2_get_video_encoder_configurations();
            } else if (strcasecmp(method, "GetVideoEncoderConfigurationOptions") == 0) {
                media2_get_video_encoder_configuration_options();
            } else if (strcasecmp(method, "GetAudioSourceConfigurations") == 0) {
                media2_get_audio_source_configurations();
            } else if (strcasecmp(method, "GetAudioSourceConfigurationOptions") == 0) {
                media2_get_audio_source_configuration_options();
            } else if (strcasecmp(method, "GetAudioEncoderConfigurations") == 0) {
                media2_get_audio_encoder_configurations();
            } else if (strcasecmp(method, "GetAudioEncoderConfigurationOptions") == 0) {
                media2_get_audio_encoder_configuration_options();
            } else if (strcasecmp(method, "GetAudioOutputConfigurations") == 0) {
                media2_get_audio_output_configurations();
            } else if (strcasecmp(method, "GetAudioOutputConfigurationOptions") == 0) {
                media2_get_audio_output_configuration_options();
            } else if (strcasecmp(method, "GetAudioDecoderConfigurations") == 0) {
                media2_get_audio_decoder_configurations();
            } else if (strcasecmp(method, "GetAudioDecoderConfigurationOptions") == 0) {
                media2_get_audio_decoder_configuration_options();
            } else if (strcasecmp(method, "GetSnapshotUri") == 0) {
                media2_get_snapshot_uri();
            } else if (strcasecmp(method, "GetStreamUri") == 0) {
                media2_get_stream_uri();
            } else {
                media2_unsupported(method);
            }
        } else if (strcasecmp("ptz_service", prog_name) == 0) {
            if (strcasecmp(method, "GetServiceCapabilities") == 0) {
                ptz_get_service_capabilities();
            } else if (strcasecmp(method, "GetConfigurations") == 0) {
                ptz_get_configurations();
            } else if (strcasecmp(method, "GetConfiguration") == 0) {
                ptz_get_configuration();
            } else if (strcasecmp(method, "GetConfigurationOptions") == 0) {
                ptz_get_configuration_options();
            } else if (strcasecmp(method, "GetNodes") == 0) {
                ptz_get_nodes();
            } else if (strcasecmp(method, "GetNode") == 0) {
                ptz_get_node();
            } else if (strcasecmp(method, "GetPresets") == 0) {
                ptz_get_presets();
            } else if (strcasecmp(method, "GotoPreset") == 0) {
                ptz_goto_preset();
            } else if (strcasecmp(method, "GotoHomePosition") == 0) {
                ptz_goto_home_position();
            } else if (strcasecmp(method, "ContinuousMove") == 0) {
                ptz_continuous_move();
            } else if (strcasecmp(method, "RelativeMove") == 0) {
                ptz_relative_move();
            } else if (strcasecmp(method, "AbsoluteMove") == 0) {
                ptz_absolute_move();
            } else if (strcasecmp(method, "Stop") == 0) {
                ptz_stop();
            } else if (strcasecmp(method, "GetStatus") == 0) {
                ptz_get_status();
            } else if (strcasecmp(method, "SetPreset") == 0) {
                ptz_set_preset();
            } else if (strcasecmp(method, "SetHomePosition") == 0) {
                ptz_set_home_position();
            } else if (strcasecmp(method, "RemovePreset") == 0) {
                ptz_remove_preset();
            } else {
                ptz_unsupported(method);
            }
        } else if (strcasecmp("events_service", prog_name) == 0) {
            if (strcasecmp(method, "GetServiceCapabilities") == 0) {
                events_get_service_capabilities();
            } else if (strcasecmp(method, "CreatePullPointSubscription") == 0) {
                events_create_pull_point_subscription();
            } else if (strcasecmp(method, "PullMessages") == 0) {
                events_pull_messages();
            } else if (strcasecmp(method, "Subscribe") == 0) {
                events_subscribe();
            } else if (strcasecmp(method, "Renew") == 0) {
                events_renew();
            } else if (strcasecmp(method, "Unsubscribe") == 0) {
                events_unsubscribe();
            } else if (strcasecmp(method, "GetEventProperties") == 0) {
                events_get_event_properties();
            } else if (strcasecmp(method, "SetSynchronizationPoint") == 0) {
                events_set_synchronization_point();
            } else {
                events_unsupported(method);
            }
        } else if (strcasecmp("deviceio_service", prog_name) == 0) {
            if (strcasecmp(method, "GetVideoSources") == 0) {
                deviceio_get_video_sources();
            } else if (strcasecmp(method, "GetServiceCapabilities") == 0) {
                deviceio_get_service_capabilities();
            } else if (strcasecmp(method, "GetAudioOutputs") == 0) {
                deviceio_get_audio_outputs();
            } else if (strcasecmp(method, "GetAudioSources") == 0) {
                deviceio_get_audio_sources();
            } else if (strcasecmp(method, "GetRelayOutputs") == 0) {
                deviceio_get_relay_outputs();
            } else if (strcasecmp(method, "GetRelayOutputOptions") == 0) {
                deviceio_get_relay_output_options();
            } else if (strcasecmp(method, "SetRelayOutputSettings") == 0) {
                deviceio_set_relay_output_settings();
            } else if (strcasecmp(method, "SetRelayOutputState") == 0) {
                deviceio_set_relay_output_state();
            } else {
                deviceio_unsupported(method);
            }
        }
    } else {
        // hack to handle a bug with Synology
        if ((service_ctx.adv_synology_nvr == 1) && (strcasecmp("media_service", prog_name) == 0) && (strcasecmp("CreateProfile", method) == 0)) {
            send_fault("media_service",
                       "Receiver",
                       "ter:Action",
                       "ter:MaxNVTProfiles",
                       "Max profile number reached",
                       "The maximum number of supported profiles supported by the device has been reached");
        } else {
            send_authentication_error();
        }
    }

    close_xml();
    free(input);

    free_conf_file();

    free(conf_file);

    // Mark end of response in raw XML log (helps diagnose truncated responses)
    if (service_ctx.raw_xml_log_file && service_ctx.raw_xml_log_file[0] != '\0') {
        FILE* rf = fopen(service_ctx.raw_xml_log_file, "a");
        if (rf) {
            const char* endm = "==== RESPONSE END ====\n";
            fwrite(endm, 1, strlen(endm), rf);
            fclose(rf);
        }
    }

    log_info("Program terminated.");

    return 0;
}
