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

#include "xml_logger.h"

#include "log.h"
#include "onvif_simple_server.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

extern service_context_t service_ctx;

// Cached state to avoid repeated checks
static int xml_logging_enabled = -1;     // -1 = not initialized, 0 = disabled, 1 = enabled
static char current_timestamp[32] = {0}; // Shared timestamp for request/response pair

/**
 * Initialize XML logging system
 */
void xml_logger_init(void)
{
    xml_logging_enabled = -1; // Reset state
    current_timestamp[0] = '\0';
}

/**
 * Check if XML logging is enabled
 * @return 1 if enabled, 0 if disabled
 */
int xml_logger_is_enabled(void)
{
    // Check if already determined
    if (xml_logging_enabled != -1) {
        return xml_logging_enabled;
    }

    // Check if debug level is enabled (level 5 = DEBUG)
    if (service_ctx.loglevel < LOG_LVL_DEBUG) {
        log_debug("XML logging disabled: debug level not enabled (current: %d, required: %d)", service_ctx.loglevel, LOG_LVL_DEBUG);
        xml_logging_enabled = 0;
        return 0;
    }

    // Check if raw_log_directory is configured
    if (service_ctx.raw_log_directory == NULL || service_ctx.raw_log_directory[0] == '\0') {
        log_debug("XML logging disabled: raw_log_directory not configured");
        xml_logging_enabled = 0;
        return 0;
    }

    // Check if directory exists and is writable
    struct stat st;
    if (stat(service_ctx.raw_log_directory, &st) != 0) {
        log_warn("XML logging disabled: raw_log_directory '%s' does not exist: %s", service_ctx.raw_log_directory, strerror(errno));
        xml_logging_enabled = 0;
        return 0;
    }

    if (!S_ISDIR(st.st_mode)) {
        log_warn("XML logging disabled: raw_log_directory '%s' is not a directory", service_ctx.raw_log_directory);
        xml_logging_enabled = 0;
        return 0;
    }

    // Check if directory is writable
    if (access(service_ctx.raw_log_directory, W_OK) != 0) {
        log_warn("XML logging disabled: raw_log_directory '%s' is not writable: %s", service_ctx.raw_log_directory, strerror(errno));
        xml_logging_enabled = 0;
        return 0;
    }

    log_debug("XML logging enabled: raw_log_directory='%s'", service_ctx.raw_log_directory);
    xml_logging_enabled = 1;
    return 1;
}

/**
 * Generate timestamp for filename
 * Format: YYYYMMDD_HHMMSS
 * @param buffer Buffer to store timestamp (must be at least 16 bytes)
 */
static void generate_timestamp(char *buffer, size_t buffer_size)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, buffer_size, "%Y%m%d_%H%M%S", tm_info);
}

/**
 * Sanitize IP address for use in directory name
 * Replaces invalid characters with underscores
 * @param ip_addr The IP address to sanitize
 * @param buffer Buffer to store sanitized IP (must be at least as large as ip_addr)
 */
static void sanitize_ip_address(const char *ip_addr, char *buffer, size_t buffer_size)
{
    if (ip_addr == NULL || buffer == NULL || buffer_size == 0) {
        if (buffer && buffer_size > 0) {
            buffer[0] = '\0';
        }
        return;
    }

    size_t i;
    for (i = 0; i < buffer_size - 1 && ip_addr[i] != '\0'; i++) {
        char c = ip_addr[i];
        // Allow alphanumeric, dots, colons (for IPv6), and hyphens
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.' || c == ':' || c == '-') {
            buffer[i] = c;
        } else {
            buffer[i] = '_';
        }
    }
    buffer[i] = '\0';
}

/**
 * Create directory for IP address if it doesn't exist
 * @param base_dir Base directory path
 * @param ip_addr IP address for subdirectory
 * @param full_path Buffer to store full path (must be at least PATH_MAX bytes)
 * @return 0 on success, -1 on error
 */
static int create_ip_directory(const char *base_dir, const char *ip_addr, char *full_path, size_t path_size)
{
    char sanitized_ip[256];
    sanitize_ip_address(ip_addr, sanitized_ip, sizeof(sanitized_ip));

    // Build full path
    int ret = snprintf(full_path, path_size, "%s/%s", base_dir, sanitized_ip);
    if (ret < 0 || (size_t) ret >= path_size) {
        log_error("XML logging: path too long for IP directory");
        return -1;
    }

    // Check if directory exists
    struct stat st;
    if (stat(full_path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return 0; // Directory already exists
        } else {
            log_error("XML logging: path '%s' exists but is not a directory", full_path);
            return -1;
        }
    }

    // Create directory
    if (mkdir(full_path, 0755) != 0) {
        log_error("XML logging: failed to create directory '%s': %s", full_path, strerror(errno));
        return -1;
    }

    log_debug("XML logging: created directory '%s'", full_path);
    return 0;
}

/**
 * Write XML content to file
 * @param filepath Full path to the file
 * @param xml_content XML content to write
 * @param xml_size Size of XML content
 * @return 0 on success, -1 on error
 */
static int write_xml_file(const char *filepath, const char *xml_content, int xml_size)
{
    FILE *fp = fopen(filepath, "w");
    if (fp == NULL) {
        log_error("XML logging: failed to open file '%s' for writing: %s", filepath, strerror(errno));
        return -1;
    }

    size_t written = fwrite(xml_content, 1, xml_size, fp);
    if (written != (size_t) xml_size) {
        log_error("XML logging: failed to write complete XML to '%s': wrote %zu of %d bytes", filepath, written, xml_size);
        fclose(fp);
        return -1;
    }

    if (fclose(fp) != 0) {
        log_error("XML logging: failed to close file '%s': %s", filepath, strerror(errno));
        return -1;
    }

    log_debug("XML logging: wrote %d bytes to '%s'", xml_size, filepath);
    return 0;
}

/**
 * Log raw XML request to external storage
 */
int log_xml_request(const char *xml_content, int xml_size, const char *remote_addr)
{
    // Check if logging is enabled
    if (!xml_logger_is_enabled()) {
        return 0; // Not an error, just disabled
    }

    // Validate inputs
    if (xml_content == NULL || xml_size <= 0) {
        log_debug("XML logging: skipping request log (empty content)");
        return 0;
    }

    if (remote_addr == NULL || remote_addr[0] == '\0') {
        remote_addr = "unknown";
    }

    // Generate timestamp for this request/response pair
    generate_timestamp(current_timestamp, sizeof(current_timestamp));

    // Create IP-specific directory
    char ip_dir[1024];
    if (create_ip_directory(service_ctx.raw_log_directory, remote_addr, ip_dir, sizeof(ip_dir)) != 0) {
        log_warn("XML logging: failed to create IP directory, skipping request log");
        return -1;
    }

    // Build filename
    char filepath[1280];
    int ret = snprintf(filepath, sizeof(filepath), "%s/%s_request.xml", ip_dir, current_timestamp);
    if (ret < 0 || (size_t) ret >= sizeof(filepath)) {
        log_error("XML logging: filepath too long for request");
        return -1;
    }

    // Write XML to file
    return write_xml_file(filepath, xml_content, xml_size);
}

/**
 * Log raw XML response to external storage
 */
int log_xml_response(const char *xml_content, int xml_size, const char *remote_addr)
{
    // Check if logging is enabled
    if (!xml_logger_is_enabled()) {
        return 0; // Not an error, just disabled
    }

    // Validate inputs
    if (xml_content == NULL || xml_size <= 0) {
        log_debug("XML logging: skipping response log (empty content)");
        return 0;
    }

    if (remote_addr == NULL || remote_addr[0] == '\0') {
        remote_addr = "unknown";
    }

    // Use the timestamp from the request (if available)
    if (current_timestamp[0] == '\0') {
        // No request was logged, generate new timestamp
        generate_timestamp(current_timestamp, sizeof(current_timestamp));
    }

    // Create IP-specific directory
    char ip_dir[1024];
    if (create_ip_directory(service_ctx.raw_log_directory, remote_addr, ip_dir, sizeof(ip_dir)) != 0) {
        log_warn("XML logging: failed to create IP directory, skipping response log");
        return -1;
    }

    // Build filename
    char filepath[1280];
    int ret = snprintf(filepath, sizeof(filepath), "%s/%s_response.xml", ip_dir, current_timestamp);
    if (ret < 0 || (size_t) ret >= sizeof(filepath)) {
        log_error("XML logging: filepath too long for response");
        return -1;
    }

    // Write XML to file
    return write_xml_file(filepath, xml_content, xml_size);
}
