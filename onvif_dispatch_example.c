// Example of a much cleaner dispatch system for ONVIF methods

#include <string.h>
#include <stdio.h>

// Function pointer type for ONVIF method handlers
typedef void (*onvif_handler_t)(void);

// Structure to map method names to handlers
typedef struct {
    const char* service;
    const char* method;
    onvif_handler_t handler;
} onvif_method_entry_t;

// Forward declarations of handler functions
void device_get_services(void);
void device_get_capabilities(void);
void device_get_device_information(void);
void device_get_system_date_and_time(void);
void device_unsupported(const char* method);
void media_get_profiles(void);
void media_get_stream_uri(void);
void media_unsupported(const char* method);

// Dispatch table - much cleaner than nested if/else
static const onvif_method_entry_t onvif_dispatch_table[] = {
    // Device service methods
    {"device_service", "GetServices", device_get_services},
    {"device_service", "GetCapabilities", device_get_capabilities},
    {"device_service", "GetDeviceInformation", device_get_device_information},
    {"device_service", "GetSystemDateAndTime", device_get_system_date_and_time},
    
    // Media service methods
    {"media_service", "GetProfiles", media_get_profiles},
    {"media_service", "GetStreamUri", media_get_stream_uri},
    
    // Add more services/methods here...
    
    // Sentinel entry
    {NULL, NULL, NULL}
};

// Clean dispatch function
void dispatch_onvif_method(const char* service, const char* method) {
    log_debug("DEBUG: Dispatching service='%s' method='%s'", service, method);
    
    // Search the dispatch table
    for (const onvif_method_entry_t* entry = onvif_dispatch_table; entry->service != NULL; entry++) {
        if (strcasecmp(entry->service, service) == 0 && strcasecmp(entry->method, method) == 0) {
            log_debug("DEBUG: Found handler for %s::%s", service, method);
            entry->handler();
            log_debug("DEBUG: Handler completed for %s::%s", service, method);
            return;
        }
    }
    
    // No handler found - call appropriate unsupported method
    log_debug("DEBUG: No handler found for %s::%s, calling unsupported", service, method);
    
    if (strcasecmp("device_service", service) == 0) {
        device_unsupported(method);
    } else if (strcasecmp("media_service", service) == 0) {
        media_unsupported(method);
    } else {
        // Generic unsupported service
        log_debug("DEBUG: Unsupported service: %s", service);
        device_unsupported(method); // Default fallback
    }
}

// Usage in main() would be:
void example_main_usage(const char* prog_name, const char* method) {
    if (auth_error == 0) {
        log_debug("DEBUG: Authentication passed, dispatching to %s::%s", prog_name, method);
        dispatch_onvif_method(prog_name, method);
        log_debug("DEBUG: Method dispatch completed");
    } else {
        log_debug("DEBUG: Authentication failed");
        send_authentication_error();
    }
}
