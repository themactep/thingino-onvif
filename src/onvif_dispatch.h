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

#ifndef ONVIF_DISPATCH_H
#define ONVIF_DISPATCH_H

/**
 * Clean dispatch system for ONVIF method routing
 * Replaces the nested if/else mess with a maintainable table-driven approach
 */

// Function pointer type for ONVIF method handlers
typedef int (*onvif_handler_t)(void);

// Function pointer type for condition checks
typedef int (*onvif_condition_t)(void);

// Structure to map service/method names to handlers with optional conditions
typedef struct {
    const char* service;
    const char* method;
    onvif_handler_t handler;
    onvif_condition_t condition; // Optional condition function (NULL if always enabled)
} onvif_method_entry_t;

/**
 * Dispatch an ONVIF method call to the appropriate handler
 * @param service The service name (e.g., "device_service", "media_service")
 * @param method The method name (e.g., "GetDeviceInformation", "GetProfiles")
 * @return 0 on success, non-zero on error
 */
int dispatch_onvif_method(const char* service, const char* method);

/**
 * Initialize the dispatch system
 * @return 0 on success, non-zero on error
 */
int onvif_dispatch_init(void);

/**
 * Cleanup the dispatch system
 */
void onvif_dispatch_cleanup(void);

#endif // ONVIF_DISPATCH_H
