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
#include "onvif_dispatch.h"
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
    // VERY FIRST THING - write to a file to prove we got here
    FILE* debug_file = fopen("/tmp/onvif_debug.log", "w");
    if (debug_file) {
        fprintf(debug_file, "MAIN STARTED - argc=%d\n", argc);
        fflush(debug_file);
        fclose(debug_file);
    }

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
    int conf_file_specified = 0;  // Flag to track if user provided -c parameter

    // Debug checkpoint 1
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "About to fprintf stderr\n");
        fflush(debug_file);
        fclose(debug_file);
    }

    // Don't use stderr in CGI - it might be corrupted
    // fprintf(stderr, "ONVIF server starting...\n");

    // Debug checkpoint 2
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "About to malloc conf_file\n");
        fflush(debug_file);
        fclose(debug_file);
    }

    // Use static buffer instead of malloc to avoid heap issues
    static char conf_file_buffer[256];
    strcpy(conf_file_buffer, DEFAULT_JSON_CONF_FILE);
    conf_file = conf_file_buffer;

    // Debug checkpoint 3
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "conf_file set to static buffer\n");
        fflush(debug_file);
        fclose(debug_file);
    }

    // Don't use stderr in CGI - it might be corrupted
    // fprintf(stderr, "Allocated conf_file memory\n");
    // fprintf(stderr, "Set default config file path\n");

    // Debug checkpoint 4
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "About to enter while loop\n");
        fflush(debug_file);
        fclose(debug_file);
    }

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
                conf_file_specified = 1;  // Mark that user provided config file
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

    // Debug checkpoint 5
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "Finished argument parsing\n");
        fflush(debug_file);
        fclose(debug_file);
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

    // Debug checkpoint 6
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "About to call basename\n");
        fflush(debug_file);
        fclose(debug_file);
    }

    prog_name = basename(tmp);

    // Debug checkpoint 7
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "basename completed, prog_name=%p\n", (void*)prog_name);
        if (prog_name) {
            fprintf(debug_file, "prog_name value: %.20s\n", prog_name);
        } else {
            fprintf(debug_file, "prog_name is NULL!\n");
        }
        fflush(debug_file);
        fclose(debug_file);
    }

    // Debug checkpoint 8
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "About to fprintf prog_name to stderr\n");
        fflush(debug_file);
        fclose(debug_file);
    }

    // Don't use stderr in CGI - it might be corrupted
    // fprintf(stderr, "Program name determined: %s\n", prog_name ? prog_name : "NULL");

    // Debug checkpoint 9
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "Skipped stderr fprintf, continuing...\n");
        fflush(debug_file);
        fclose(debug_file);
    }

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

    // Debug checkpoint 10
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "About to initialize logging\n");
        fflush(debug_file);
        fclose(debug_file);
    }

    // Don't use stderr in CGI
    // fprintf(stderr, "About to initialize logging\n");
    log_init("onvif_simple_server", LOG_DAEMON, debug, 1);
    // fprintf(stderr, "Logging initialized\n");
    log_info("Starting program.");

    // Debug checkpoint 11
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "Logging initialized, about to dump env\n");
        fflush(debug_file);
        fclose(debug_file);
    }

    // Don't use stderr in CGI
    // fprintf(stderr, "About to dump environment\n");
    dump_env();
    // fprintf(stderr, "Environment dumped\n");

    // Debug checkpoint 12
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "Environment dumped, about to find config\n");
        fflush(debug_file);
        fclose(debug_file);
    }

    // Try to find config file: first in same directory as binary, then in /etc/
    // Don't use stderr in CGI
    // fprintf(stderr, "About to find configuration file\n");
    char* final_conf_file = NULL;

    // If no config file specified via -c, try to find it automatically
    if (!conf_file_specified) {
        // Use static buffers to avoid malloc issues in CGI
        static char argv0_copy[PATH_MAX];
        static char final_conf_buffer[PATH_MAX];
        strncpy(argv0_copy, argv[0], sizeof(argv0_copy) - 1);
        argv0_copy[sizeof(argv0_copy) - 1] = '\0';

        // Get directory of the binary using static buffer
        char* binary_dir = dirname(argv0_copy);
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

    // Debug checkpoint 13
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "About to process config file\n");
        fflush(debug_file);
        fclose(debug_file);
    }

    // Don't use stderr in CGI
    // fprintf(stderr, "About to process configuration file: %s\n", final_conf_file);
    itmp = process_json_conf_file(final_conf_file);

    // Debug checkpoint 14
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "Config processed, result=%d\n", itmp);
        fflush(debug_file);
        fclose(debug_file);
    }

    // Don't use stderr in CGI
    // fprintf(stderr, "Configuration file processed, result: %d\n", itmp);
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

    // Debug checkpoint 14.5
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "Config cleanup complete, about to check log level\n");
        fflush(debug_file);
        fclose(debug_file);
    }

    // Apply log level from config if CLI -d was not provided
    if (!debug_cli_set) {
        if (service_ctx.loglevel >= LOG_LVL_FATAL && service_ctx.loglevel <= LOG_LVL_TRACE) {
            log_set_level(service_ctx.loglevel);
            debug = service_ctx.loglevel;
            log_info("Log level set from config: %d", debug);
        }
    }

    tmp = getenv("REQUEST_METHOD");
    if ((tmp == NULL) || (strcmp("POST", tmp) != 0)) {
        fprintf(stdout, "Content-type: text/html\r\n");
        fprintf(stdout, "Content-Length: 86\r\n\r\n");
        fprintf(stdout, "<html><head><title>Error</title></head><body>HTTP method not supported</body></html>\r\n");
        log_fatal("HTTP method not supported");

        free(conf_file);
        exit(EXIT_FAILURE);
    }

    // Debug checkpoint 15
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "About to allocate input buffer\n");
        fflush(debug_file);
        fclose(debug_file);
    }

    int input_size;
    // Use static buffer instead of malloc to avoid heap issues
    static char input_buffer[16 * 1024];
    char* input = input_buffer;
    log_debug("Static buffer allocated, input pointer points to %p", input);

    // Debug checkpoint 16
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "Input buffer allocated, about to read stdin\n");
        fflush(debug_file);
        fclose(debug_file);
    }
    // Static buffer can't be NULL
    // if (input == NULL) {
    //     log_fatal("Memory error");
    //     free(conf_file);
    //     exit(EXIT_FAILURE);
    // }

    input_size = fread(input, 1, 16 * 1024 * sizeof(char), stdin);

    // Debug checkpoint 17
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "Read %d bytes from stdin\n", input_size);
        fflush(debug_file);
        fclose(debug_file);
    }

    if (input_size == 0) {
        log_fatal("Error: input is empty");
        // Don't free static buffer: free(input);
        // Don't free static buffer: free(conf_file);
        exit(EXIT_FAILURE);
    }

    // No need to realloc with static buffer - just use what we read
    // char* temp_input = (char*) realloc(input, input_size * sizeof(char));
    char* temp_input = input;
    // Static buffer doesn't need realloc
    // if (temp_input == NULL) {
    //     log_fatal("Memory error trying to allocate %d bytes", input_size);
    //     free(input);  // Free the original input pointer since realloc failed
    //     free(conf_file);
    //     exit(EXIT_FAILURE);
    // }
    // input = temp_input;  // Only update input if realloc succeeded

    log_debug("Using static buffer, input pointer is %p", input);

    // Debug checkpoint 18
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "Input processing complete, about to parse\n");
        fflush(debug_file);
        fclose(debug_file);
    }
    // Raw request logging to file if configured
    if (service_ctx.raw_xml_log_file && service_ctx.raw_xml_log_file[0] != '\0') {

        FILE* rf = fopen(service_ctx.raw_xml_log_file, "a");
        if (rf) {
            const char* hdr = "\n==== REQUEST BEGIN ====\n";
            fwrite(hdr, 1, strlen(hdr), rf);
            fwrite(input, 1, (size_t) input_size, rf);
            const char* end_hdr = "\n==== REQUEST END ====\n";
            fwrite(end_hdr, 1, strlen(end_hdr), rf);
            fclose(rf);
        } else {
            log_warn("Cannot open raw_xml_log_file %s: %s", service_ctx.raw_xml_log_file, strerror(errno));
        }
    }
    log_debug("Url: %s", prog_name);

    // Debug checkpoint 19
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "About to init XML parsing\n");
        fflush(debug_file);
        fclose(debug_file);
    }

    // Warning: init_xml changes the input string
    init_xml(input, input_size);

    // Debug checkpoint 20
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "XML initialized, about to get method\n");
        fflush(debug_file);
        fclose(debug_file);
    }

    method = get_method(1);
    if (method == NULL) {
        log_fatal("XML parsing error");
        close_xml();
        // Don't free static buffer: free(input);
        // Don't free static buffer: free(conf_file);
        exit(EXIT_FAILURE);
    }

    // Debug checkpoint 21
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "Method extracted successfully\n");
        fflush(debug_file);
        fclose(debug_file);
    }

    log_debug("Method: %s", method);

    // Debug checkpoint 22
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "About to check authentication\n");
        fflush(debug_file);
        fclose(debug_file);
    }

    if (service_ctx.username != NULL) {
        // Debug checkpoint 23
        debug_file = fopen("/tmp/onvif_debug.log", "a");
        if (debug_file) {
            fprintf(debug_file, "Username configured, checking security headers\n");
            fflush(debug_file);
            fclose(debug_file);
        }

        if ((get_element("Security", "Header") != NULL) && (get_element("UsernameToken", "Header") != NULL)) {
            // Debug checkpoint 24
            debug_file = fopen("/tmp/onvif_debug.log", "a");
            if (debug_file) {
                fprintf(debug_file, "Security headers found, processing auth\n");
                fflush(debug_file);
                fclose(debug_file);
            }
            unsigned char nonce[128];
            unsigned long nonce_size = sizeof(nonce);
            unsigned char auth[128];
            unsigned long auth_size = sizeof(auth);
            char sha1[20];
            unsigned long sha1_size = sizeof(sha1);
            char digest[128];
            unsigned long digest_size = sizeof(digest) - 1; // leave room for NUL terminator

            security.enable = 1;

            // Debug checkpoint 25
            debug_file = fopen("/tmp/onvif_debug.log", "a");
            if (debug_file) {
                fprintf(debug_file, "About to extract username\n");
                fflush(debug_file);
                fclose(debug_file);
            }

            security.username = get_element("Username", "Header");
            if (security.username != NULL) {
                log_debug("Security: username = %s", security.username);
            } else {
                auth_error = 1;
            }

            // Debug checkpoint 26
            debug_file = fopen("/tmp/onvif_debug.log", "a");
            if (debug_file) {
                fprintf(debug_file, "About to extract password\n");
                fflush(debug_file);
                fclose(debug_file);
            }

            security.password = get_element("Password", "Header");

            // Debug checkpoint 27
            debug_file = fopen("/tmp/onvif_debug.log", "a");
            if (debug_file) {
                fprintf(debug_file, "Password extracted, about to log it\n");
                fflush(debug_file);
                fclose(debug_file);
            }

            if (security.password != NULL) {
                // Debug checkpoint 28
                debug_file = fopen("/tmp/onvif_debug.log", "a");
                if (debug_file) {
                    fprintf(debug_file, "Password is not NULL, about to log_debug\n");
                    fflush(debug_file);
                    fclose(debug_file);
                }

                log_debug("Security: password = %s", security.password);

                // Debug checkpoint 29
                debug_file = fopen("/tmp/onvif_debug.log", "a");
                if (debug_file) {
                    fprintf(debug_file, "Password log_debug completed\n");
                    fflush(debug_file);
                    fclose(debug_file);
                }
            } else {
                auth_error = 2;
            }

            // Debug checkpoint 30
            debug_file = fopen("/tmp/onvif_debug.log", "a");
            if (debug_file) {
                fprintf(debug_file, "About to extract nonce\n");
                fflush(debug_file);
                fclose(debug_file);
            }

            security.nonce = get_element("Nonce", "Header");
            if (security.nonce != NULL) {
                log_debug("Security: nonce = %s", security.nonce);

                // Debug checkpoint 31
                debug_file = fopen("/tmp/onvif_debug.log", "a");
                if (debug_file) {
                    fprintf(debug_file, "About to decode nonce\n");
                    fflush(debug_file);
                    fclose(debug_file);
                }

                b64_decode((unsigned char*) security.nonce, strlen(security.nonce), nonce, &nonce_size);
            } else {
                auth_error = 3;
            }

            // Debug checkpoint 32
            debug_file = fopen("/tmp/onvif_debug.log", "a");
            if (debug_file) {
                fprintf(debug_file, "About to extract created timestamp\n");
                fflush(debug_file);
                fclose(debug_file);
            }

            security.created = get_element("Created", "Header");

            // Debug checkpoint 33
            debug_file = fopen("/tmp/onvif_debug.log", "a");
            if (debug_file) {
                fprintf(debug_file, "Created timestamp extracted\n");
                fflush(debug_file);
                fclose(debug_file);
            }

            if (security.created != NULL) {
                // Debug checkpoint 34
                debug_file = fopen("/tmp/onvif_debug.log", "a");
                if (debug_file) {
                    fprintf(debug_file, "Created is not NULL, about to log_debug\n");
                    fflush(debug_file);
                    fclose(debug_file);
                }

                log_debug("Security: created = %s", security.created);

                // Debug checkpoint 35
                debug_file = fopen("/tmp/onvif_debug.log", "a");
                if (debug_file) {
                    fprintf(debug_file, "Created log_debug completed\n");
                    fflush(debug_file);
                    fclose(debug_file);
                }
            } else {
                auth_error = 4;
            }

            // Debug checkpoint 36
            debug_file = fopen("/tmp/onvif_debug.log", "a");
            if (debug_file) {
                fprintf(debug_file, "About to check auth_error and calculate digest\n");
                fflush(debug_file);
                fclose(debug_file);
            }

            if (auth_error == 0) {
                // Debug checkpoint 37
                debug_file = fopen("/tmp/onvif_debug.log", "a");
                if (debug_file) {
                    fprintf(debug_file, "auth_error is 0, starting digest calculation\n");
                    fflush(debug_file);
                    fclose(debug_file);
                }

                // Calculate digest and check the password
                // Digest = B64ENCODE( SHA1( B64DECODE( Nonce ) + Date + Password ) )
                if (nonce_size + strlen(security.created) + strlen(service_ctx.password) > sizeof(auth)) {
                    log_error("Authentication data too large");
                    auth_error = 10;
                } else {
                    // Debug checkpoint 38
                    debug_file = fopen("/tmp/onvif_debug.log", "a");
                    if (debug_file) {
                        fprintf(debug_file, "About to copy auth data\n");
                        fflush(debug_file);
                        fclose(debug_file);
                    }

                    memcpy(auth, nonce, nonce_size);
                    memcpy(&auth[nonce_size], security.created, strlen(security.created));
                    memcpy(&auth[nonce_size + strlen(security.created)], service_ctx.password, strlen(service_ctx.password));
                    auth_size = nonce_size + strlen(security.created) + strlen(service_ctx.password);

                    // Debug checkpoint 39
                    debug_file = fopen("/tmp/onvif_debug.log", "a");
                    if (debug_file) {
                        fprintf(debug_file, "About to calculate SHA1 hash\n");
                        fflush(debug_file);
                        fclose(debug_file);
                    }

                    hashSHA1((char*)auth, auth_size, sha1, (int)sha1_size);

                    // Debug checkpoint 40
                    debug_file = fopen("/tmp/onvif_debug.log", "a");
                    if (debug_file) {
                        fprintf(debug_file, "SHA1 calculated, about to base64 encode\n");
                        fflush(debug_file);
                        fclose(debug_file);
                    }

                    b64_encode((unsigned char*)sha1, (unsigned int)sha1_size, (unsigned char*)digest, &digest_size);
                    // Ensure null-termination for safe string operations/logging
                    if (digest_size >= sizeof(digest)) {
                        digest[sizeof(digest) - 1] = '\0';
                    } else {
                        digest[digest_size] = '\0';
                    }

                    // Debug checkpoint 41
                    debug_file = fopen("/tmp/onvif_debug.log", "a");
                    if (debug_file) {
                        fprintf(debug_file, "About to log calculated digest\n");
                        fflush(debug_file);
                        fclose(debug_file);
                    }

                    log_debug("Calculated digest: %s", digest);

                    // Debug checkpoint 42
                    debug_file = fopen("/tmp/onvif_debug.log", "a");
                    if (debug_file) {
                        fprintf(debug_file, "Calculated digest logged, about to log received digest\n");
                        fflush(debug_file);
                        fclose(debug_file);
                    }

                    log_debug("Received digest: %s", security.password);

                    // Debug checkpoint 43
                    debug_file = fopen("/tmp/onvif_debug.log", "a");
                    if (debug_file) {
                        fprintf(debug_file, "Both digests logged, about to compare\n");
                        fflush(debug_file);
                        fclose(debug_file);
                    }

                    // Debug checkpoint 44
                    debug_file = fopen("/tmp/onvif_debug.log", "a");
                    if (debug_file) {
                        fprintf(debug_file, "About to compare username and digest\n");
                        fflush(debug_file);
                        fclose(debug_file);
                    }

                    if ((strcmp(service_ctx.username, security.username) != 0) || (strcmp(security.password, digest) != 0)) {
                        auth_error = 10;
                    }

                    // Debug checkpoint 45
                    debug_file = fopen("/tmp/onvif_debug.log", "a");
                    if (debug_file) {
                        fprintf(debug_file, "Comparison completed, auth_error=%d\n", auth_error);
                        fflush(debug_file);
                        fclose(debug_file);
                    }
                }
            }
        } else {
            auth_error = 11;
        }
    } else {
        security.enable = 0;
    }

    // Debug checkpoint 46
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "Authentication processing complete, checking method exceptions\n");
        fflush(debug_file);
        fclose(debug_file);
    }

    if ((strcasecmp("device_service", prog_name) == 0)
        && ((strcasecmp("GetSystemDateAndTime", method) == 0) || (strcasecmp("GetUsers", method) == 0) || (strcasecmp("GetCapabilities", method) == 0)
            || (strcasecmp("GetServices", method) == 0) || (strcasecmp("GetServiceCapabilities", method) == 0))) {
        auth_error = 0;
    }

    // Debug checkpoint 47
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "Method exception check complete, about to log auth result\n");
        fflush(debug_file);
        fclose(debug_file);
    }

    // Debug checkpoint 48
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "About to log final auth result\n");
        fflush(debug_file);
        fclose(debug_file);
    }

    if (security.enable == 1) {
        if (auth_error == 0) {
            // Skip problematic log_info in CGI: log_info("Authentication ok");
            // Debug checkpoint 49
            debug_file = fopen("/tmp/onvif_debug.log", "a");
            if (debug_file) {
                fprintf(debug_file, "Authentication OK (skipped log_info)\n");
                fflush(debug_file);
                fclose(debug_file);
            }
        } else {
            // Skip problematic log_error in CGI: log_error("Authentication error");
            // Debug checkpoint 50
            debug_file = fopen("/tmp/onvif_debug.log", "a");
            if (debug_file) {
                fprintf(debug_file, "Authentication ERROR (skipped log_error)\n");
                fflush(debug_file);
                fclose(debug_file);
            }
        }
    }

    // Debug checkpoint 51
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "Authentication result logged, continuing\n");
        fflush(debug_file);
        fclose(debug_file);
    }

    // Skip problematic log_debug in CGI: log_debug("DEBUG: Right after authentication check");

    // Debug checkpoint 52
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "About to start method dispatch\n");
        fflush(debug_file);
        fclose(debug_file);
    }

    // Skip problematic log_debug calls in CGI:
    // log_debug("DEBUG: About to log authentication details");
    // log_debug("Authentication completed, auth_error = %d", auth_error);
    // log_debug("About to process method: %s for service: %s", method ? method : "NULL", prog_name ? prog_name : "NULL");

    if (auth_error == 0) {
        // Debug checkpoint 53
        debug_file = fopen("/tmp/onvif_debug.log", "a");
        if (debug_file) {
            fprintf(debug_file, "auth_error is 0, starting method dispatch\n");
            fflush(debug_file);
            fclose(debug_file);
        }

        // Debug checkpoint 54
        debug_file = fopen("/tmp/onvif_debug.log", "a");
        if (debug_file) {
            fprintf(debug_file, "About to call dispatch_onvif_method\n");
            fflush(debug_file);
            fclose(debug_file);
        }

        // Use clean dispatch table instead of massive if/else ladder
        dispatch_onvif_method(prog_name, method);

        // Debug checkpoint 55
        debug_file = fopen("/tmp/onvif_debug.log", "a");
        if (debug_file) {
            fprintf(debug_file, "dispatch_onvif_method completed\n");
            fflush(debug_file);
            fclose(debug_file);
        } else {
            // Debug checkpoint 56
            debug_file = fopen("/tmp/onvif_debug.log", "a");
            if (debug_file) {
                fprintf(debug_file, "Authentication failed, sending auth error\n");
                fflush(debug_file);
                fclose(debug_file);
            }

            send_authentication_error();
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

    // Debug checkpoint 57
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "ONVIF method completed, starting cleanup\n");
        fflush(debug_file);
        fclose(debug_file);
    }

    close_xml();

    // Debug checkpoint 58
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "XML closed, about to free resources\n");
        fflush(debug_file);
        fclose(debug_file);
    }

    // Don't free static buffers: free(input);
    // Don't free static buffers: free(conf_file);

    // Now safe to free configuration memory
    free_conf_file();

    // Debug checkpoint 59
    debug_file = fopen("/tmp/onvif_debug.log", "a");
    if (debug_file) {
        fprintf(debug_file, "Program terminating successfully\n");
        fflush(debug_file);
        fclose(debug_file);
    }

    // Skip problematic logging in CGI:
    // log_debug("About to terminate program");
    // log_info("Program terminated.");

    return 0;
}
