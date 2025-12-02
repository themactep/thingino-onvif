/*
 * Copyright (c) 2025 Thingino
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef XML_LOGGER_H
#define XML_LOGGER_H

/**
 * Initialize XML logging system
 * Should be called once at startup
 */
void xml_logger_init(void);

/**
 * Log raw XML request to external storage (general XML logger)
 * @param xml_content The raw XML content to log
 * @param xml_size Size of the XML content
 * @param remote_addr Remote IP address (from REMOTE_ADDR env var)
 * @return 0 on success, -1 on error
 */
int log_xml_request(const char *xml_content, int xml_size, const char *remote_addr);

/**
 * Log raw XML response to external storage (general XML logger)
 * @param xml_content The raw XML content to log
 * @param xml_size Size of the XML content
 * @param remote_addr Remote IP address (from REMOTE_ADDR env var)
 * @return 0 on success, -1 on error
 */
int log_xml_response(const char *xml_content, int xml_size, const char *remote_addr);

/**
 * Error-time raw XML capture (parameter validation failure or SOAP Fault)
 * Independent of log_level; writes only when log_directory is a mounted,
 * writable external filesystem per policy. On failure, emits throttled WARN.
 * All string parameters may be NULL.
 */
int log_xml_error_request(const char *xml_content,
                          int xml_size,
                          const char *client_ip,
                          const char *service,
                          const char *method,
                          const char *reason,
                          const char *request_uri,
                          const char *query_string);

/**
 * Destination readiness check for error-time logging.
 * Returns 1 if log_directory is configured, exists, writable, and mounted
 * on an external filesystem; 0 otherwise. If emit_warn is non-zero, emits a
 * throttled WARN when not ready.
 */
int xml_error_log_destination_ready(int emit_warn);

/**
 * Check if XML logging is enabled (general request/response logger)
 * @return 1 if enabled, 0 if disabled
 */
int xml_logger_is_enabled(void);

#endif // XML_LOGGER_H
