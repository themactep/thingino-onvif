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
#include "fault.h"
#include "log.h"
#include "media2_service.h"
#include "media_service.h"
#include "mxml_wrapper.h"
#include "onvif_dispatch.h"
#include "ptz_service.h"
#include "utils.h"
#include "xml_logger.h"

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

service_context_t service_ctx;

// Store a copy of raw request for error-only logging
static char *g_raw_request_copy = NULL;
static size_t g_raw_request_size = 0;

const char *get_raw_request_data(size_t *out_size)
{
    if (out_size)
        *out_size = g_raw_request_size;
    return g_raw_request_copy;
}
// Track whether the last response was a SOAP fault (defined in utils.c)
extern int g_last_response_was_soap_fault;

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

void print_usage(char *progname)
{
    fprintf(stderr, "\nUsage: %s [-c JSON_CONF_FILE] [-d]\n\n", progname);
    fprintf(stderr, "\t-c JSON_CONF_FILE, --conf_file JSON_CONF_FILE\n");
    fprintf(stderr, "\t\tpath of the JSON configuration file (default %s)\n", DEFAULT_JSON_CONF_FILE);
    fprintf(stderr, "\t-d LEVEL, --debug LEVEL\n");
    fprintf(stderr, "\t\tlog level: FATAL, ERROR, WARN, INFO, DEBUG, TRACE or 0-5 (default FATAL)\n");
    fprintf(stderr, "\t-h, --help\n");
    fprintf(stderr, "\t\tprint this help\n");
}

int main(int argc, char **argv)
{
    char *tmp;
    int errno;
    char *endptr;
    int c, ret, i, itmp;
    int debug_cli_set = 0;
    char *conf_file;
    char *prog_name;
    const char *method;
    username_token_t security;
    int auth_error = 0;
    int conf_file_specified = 0; // Flag to track if user provided -c parameter

    // Use static buffer instead of malloc to avoid heap issues
    static char conf_file_buffer[256];
    strcpy(conf_file_buffer, DEFAULT_JSON_CONF_FILE);
    conf_file = conf_file_buffer;

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
                // Don't free static buffer: free(conf_file);
                conf_file = (char *) malloc((strlen(optarg) + 1) * sizeof(char));
                strcpy(conf_file, optarg);
                conf_file_specified = 1; // Mark that user provided config file
            } else {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;

        case 'd':
            debug = log_level_from_string(optarg);
            if (debug < 0) {
                fprintf(stderr, "Invalid log level: %s\n", optarg);
                fprintf(stderr, "Valid levels: FATAL, ERROR, WARN, INFO, DEBUG, TRACE or 0-5\n");
                print_usage(argv[0]);
                // Don't free static buffer unless malloc'd: if (conf_file_specified) free(conf_file);
                exit(EXIT_FAILURE);
            }
            /* level set directly: textual or numeric */
            debug_cli_set = 1;
            break;

        case 'h':
            print_usage(argv[0]);
            // Don't free static buffer unless malloc'd: if (conf_file_specified) free(conf_file);
            exit(EXIT_SUCCESS);
            break;

        case '?':
            /* getopt_long already printed an error message. */
            break;

        default:
            print_usage(argv[0]);
            // Don't free static buffer unless malloc'd: if (conf_file_specified) free(conf_file);
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
        // Don't free static buffer: free(conf_file);
        exit(EXIT_SUCCESS);
    }
    if (strlen(conf_file) <= 5) {
        print_usage(argv[0]);
        // Don't free static buffer: free(conf_file);
        exit(EXIT_SUCCESS);
    }

    log_init("onvif_simple_server", LOG_DAEMON, debug, 1);
    log_info("Starting program.");

    dump_env();

    // Try to find config file: first in same directory as binary, then in /etc/
    char *final_conf_file = NULL;

    // If no config file specified via -c, try to find it automatically
    if (!conf_file_specified) {
        // Use static buffers to avoid malloc issues in CGI
        static char argv0_copy[PATH_MAX];
        static char final_conf_buffer[PATH_MAX];
        strncpy(argv0_copy, argv[0], sizeof(argv0_copy) - 1);
        argv0_copy[sizeof(argv0_copy) - 1] = '\0';

        // Get directory of the binary using static buffer
        char *binary_dir = dirname(argv0_copy);
        char local_conf_path[PATH_MAX];
        snprintf(local_conf_path, sizeof(local_conf_path), "%s/onvif.json", binary_dir);

        // Check if config file exists in binary directory
        if (access(local_conf_path, F_OK) == 0) {
            strncpy(final_conf_buffer, local_conf_path, sizeof(final_conf_buffer) - 1);
            final_conf_buffer[sizeof(final_conf_buffer) - 1] = '\0';
            final_conf_file = final_conf_buffer;
            log_info("Found configuration file in binary directory: %s", final_conf_file);
        } else {
            // Fall back to /etc/onvif.json
            strncpy(final_conf_buffer, DEFAULT_JSON_CONF_FILE, sizeof(final_conf_buffer) - 1);
            final_conf_buffer[sizeof(final_conf_buffer) - 1] = '\0';
            final_conf_file = final_conf_buffer;
            log_info("Using default configuration file: %s", final_conf_file);
        }
        // Don't free static buffer: free(binary_dir);
    } else {
        // Config file was specified via -c, use it as-is (it's already a static buffer)
        final_conf_file = conf_file;
    }

    log_info("Processing configuration file %s...", final_conf_file);

    itmp = process_json_conf_file(final_conf_file);

    if (itmp == -1) {
        log_fatal("Unable to find configuration file %s", final_conf_file);

        // Don't free static buffers: free(conf_file);
        // Don't free static buffers: free(final_conf_file);
        exit(EXIT_FAILURE);
    } else if (itmp < -1) {
        log_fatal("Wrong syntax in configuration file %s", final_conf_file);

        // Don't free static buffers: free(conf_file);
        // Don't free static buffers: free(final_conf_file);
        exit(EXIT_FAILURE);
    }
    // Don't free static buffer: free(final_conf_file);
    log_info("Completed.");

    // Apply log level from config if CLI -d was not provided
    if (!debug_cli_set) {
        if (service_ctx.loglevel >= LOG_LVL_FATAL && service_ctx.loglevel <= LOG_LVL_TRACE) {
            log_set_level(service_ctx.loglevel);
            debug = service_ctx.loglevel;
        }
    }

    tmp = getenv("REQUEST_METHOD");
    log_debug("REQUEST_METHOD: %s", tmp ? tmp : "NULL");
    if ((tmp == NULL) || (strcmp("POST", tmp) != 0)) {
        fprintf(stdout, "Content-type: text/html\r\n");
        fprintf(stdout, "Content-Length: 86\r\n");
        fprintf(stdout, "Connection: close\r\n\r\n");
        fprintf(stdout, "<html><head><title>Error</title></head><body>HTTP method not supported</body></html>\r\n");
        log_fatal("HTTP method not supported - got: %s", tmp ? tmp : "NULL");

        // Don't free static buffer unless malloc'd: if (conf_file_specified) free(conf_file);
        exit(EXIT_FAILURE);
    }

    int input_size;
    // Use static buffer instead of malloc to avoid heap issues
    static char input_buffer[16 * 1024];
    char *input = input_buffer;
    log_debug("Static buffer allocated, input pointer points to %p", input);
    // Static buffer can't be NULL
    // if (input == NULL) {
    //     log_fatal("Memory error");
    //     free(conf_file);
    //     exit(EXIT_FAILURE);
    // }

    input_size = fread(input, 1, 16 * 1024 * sizeof(char), stdin);

    if (input_size == 0) {
        log_fatal("Error: input is empty");
        // Don't free static buffer: free(input);
        // Don't free static buffer: free(conf_file);
        exit(EXIT_FAILURE);
    }

    // No need to realloc with static buffer - just use what we read
    // char* temp_input = (char*) realloc(input, input_size * sizeof(char));
    char *temp_input = input;
    // Static buffer doesn't need realloc
    // if (temp_input == NULL) {
    //     log_fatal("Memory error trying to allocate %d bytes", input_size);
    //     free(input);  // Free the original input pointer since realloc failed
    //     free(conf_file);
    //     exit(EXIT_FAILURE);
    // }
    // input = temp_input;  // Only update input if realloc succeeded

    log_debug("Using static buffer, input pointer is %p", input);

    log_debug("Url: %s", prog_name);

    // Initialize response buffer for XML logging
    response_buffer_init();

    // Log raw XML request if enabled; also keep a copy if error-time logging destination is ready
    tmp = getenv("REMOTE_ADDR");
    if (xml_logger_is_enabled()) {
        log_xml_request(input, input_size, tmp);
        response_buffer_enable(1);
    }
    if (xml_error_log_destination_ready(0) && g_raw_request_copy == NULL && input_size > 0) {
        g_raw_request_copy = (char *) malloc((size_t) input_size);
        if (g_raw_request_copy) {
            memcpy(g_raw_request_copy, input, (size_t) input_size);
            g_raw_request_size = (size_t) input_size;
        }
    }

    // Warning: init_xml changes the input string
    init_xml(input, input_size);

    method = get_method(1);
    if (method == NULL) {
        log_fatal("XML parsing error");
        close_xml();
        // Don't free static buffer: free(input);
        // Don't free static buffer: free(conf_file);
        exit(EXIT_FAILURE);
    }

    log_debug("Method: %s", method);

    log_debug("Authentication config: username=%s", service_ctx.username ? service_ctx.username : "NULL");
    if (service_ctx.username != NULL) {
        log_debug("Authentication required, checking for Security header");
        const char *security_header = get_element("Security", "Header");
        const char *username_token = get_element("UsernameToken", "Header");
        log_debug("Security header: %s", security_header ? "found" : "not found");
        log_debug("UsernameToken: %s", username_token ? "found" : "not found");

        if ((security_header != NULL) && (username_token != NULL)) {
            unsigned char nonce[128];
            unsigned long nonce_size = sizeof(nonce);
            unsigned char auth[128];
            unsigned long auth_size = sizeof(auth);
            char sha1[20];
            unsigned long sha1_size = sizeof(sha1);
            char digest[128];
            unsigned long digest_size = sizeof(digest) - 1; // leave room for NUL terminator

            security.enable = 1;

            security.username = get_element("Username", "Header");
            if (security.username != NULL) {
                log_debug("Security: username = %s", security.username);
            } else {
                auth_error = 1;
            }

            security.password = get_element("Password", "Header");

            if (security.password != NULL) {
                log_debug("Security: password = %s", security.password);
            } else {
                auth_error = 2;
            }

            security.nonce = get_element("Nonce", "Header");
            if (security.nonce != NULL) {
                log_debug("Security: nonce = %s", security.nonce);

                b64_decode((unsigned char *) security.nonce, strlen(security.nonce), nonce, &nonce_size);
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
                if (nonce_size + strlen(security.created) + strlen(service_ctx.password) > sizeof(auth)) {
                    log_error("Authentication data too large");
                    auth_error = 10;
                } else {
                    memcpy(auth, nonce, nonce_size);
                    memcpy(&auth[nonce_size], security.created, strlen(security.created));
                    memcpy(&auth[nonce_size + strlen(security.created)], service_ctx.password, strlen(service_ctx.password));
                    auth_size = nonce_size + strlen(security.created) + strlen(service_ctx.password);

                    hashSHA1((char *) auth, auth_size, sha1, (int) sha1_size);

                    b64_encode((unsigned char *) sha1, (unsigned int) sha1_size, (unsigned char *) digest, &digest_size);
                    // Ensure null-termination for safe string operations/logging
                    if (digest_size >= sizeof(digest)) {
                        digest[sizeof(digest) - 1] = '\0';
                    } else {
                        digest[digest_size] = '\0';
                    }

                    log_debug("Calculated digest: %s", digest);

                    log_debug("Received digest: %s", security.password);

                    if ((strcmp(service_ctx.username, security.username) != 0) || (strcmp(security.password, digest) != 0)) {
                        auth_error = 10;
                    }
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
            || (strcasecmp("GetServices", method) == 0) || (strcasecmp("GetServiceCapabilities", method) == 0)
            || (strcasecmp("GetDeviceInformation", method) == 0))) {
        auth_error = 0;
    }

    if (security.enable == 1) {
        if (auth_error == 0) {
            // Skip problematic log_info in CGI: log_info("Authentication ok");
        } else {
            // Skip problematic log_error in CGI: log_error("Authentication error");
        }
    }

    // Skip problematic log_debug in CGI: log_debug("DEBUG: Right after authentication check");

    // Skip problematic log_debug calls in CGI:
    // log_debug("DEBUG: About to log authentication details");
    // Reset SOAP fault flag before dispatch
    g_last_response_was_soap_fault = 0;

    // log_debug("Authentication completed, auth_error = %d", auth_error);
    // log_debug("About to process method: %s for service: %s", method ? method : "NULL", prog_name ? prog_name : "NULL");

    log_debug("Authentication check result: auth_error=%d", auth_error);
    if (auth_error == 0) {
        log_debug("Authentication passed, dispatching method: %s", method);
        // Use clean dispatch table instead of massive if/else ladder
        dispatch_onvif_method(prog_name, method);
    } else {
        log_error("Authentication failed, sending HTTP 401 Unauthorized");
        send_authentication_error();
    }

    // Synology hack handling
    if ((service_ctx.adv_synology_nvr == 1) && (strcasecmp("media_service", prog_name) == 0) && (strcasecmp("CreateProfile", method) == 0)) {
        send_fault("media_service",
                   "Receiver",
                   "ter:Action",
                   "ter:MaxNVTProfiles",
                   "Max profile number reached",
                   "The maximum number of supported profiles supported by the device has been reached");
    }

    close_xml();

    // Log XML response if enabled
    if (xml_logger_is_enabled()) {
        size_t response_size;
        const char *response_data = response_buffer_get(&response_size);
        if (response_size > 0) {
            tmp = getenv("REMOTE_ADDR");
            log_xml_response(response_data, response_size, tmp);
        }
    }
    response_buffer_clear();
    // Error-only raw XML logging on SOAP Fault responses (central hook)
    if (g_last_response_was_soap_fault) {
        size_t raw_sz = 0;
        const char *raw = get_raw_request_data(&raw_sz);
        const char *client_ip = getenv("REMOTE_ADDR");
        const char *request_uri = getenv("REQUEST_URI");
        const char *query = getenv("QUERY_STRING");
        log_xml_error_request(raw, (int) raw_sz, client_ip, prog_name, method, "SOAP Fault", request_uri, query);
    }

    // Don't free static buffers: free(input);
    // Don't free static buffers: free(conf_file);

    // Configuration memory will be freed automatically when the program exits
    // free_conf_file();

    // Free raw request copy if allocated
    if (g_raw_request_copy) {
        free(g_raw_request_copy);
        g_raw_request_copy = NULL;
        g_raw_request_size = 0;
    }

    // Skip problematic logging in CGI:
    // log_debug("About to terminate program");
    // log_info("Program terminated.");

    return 0;
}
