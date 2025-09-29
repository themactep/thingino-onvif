// Thingino ONVIF logging adapter: syslog primary, stderr optional (foreground)
#ifndef LOG_H
#define LOG_H

#include <stdarg.h>
#include <syslog.h>

// Clear, non-inverted level scale (0..5):
// 0=FATAL, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=TRACE
// Also supports textual levels: "FATAL", "ERROR", "WARN", "INFO", "DEBUG", "TRACE"
enum { LOG_LVL_FATAL = 0, LOG_LVL_ERROR = 1, LOG_LVL_WARN = 2, LOG_LVL_INFO = 3, LOG_LVL_DEBUG = 4, LOG_LVL_TRACE = 5 };

// Preserve existing macro names and call signature
#define log_trace(...) log_log(LOG_LVL_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define log_debug(...) log_log(LOG_LVL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...) log_log(LOG_LVL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...) log_log(LOG_LVL_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) log_log(LOG_LVL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define log_fatal(...) log_log(LOG_LVL_FATAL, __FILE__, __LINE__, __VA_ARGS__)

// Initialize logging. facility: e.g., LOG_DAEMON; level: 0..5 (numeric) or textual; to_stderr: 0/1
void log_init(const char* ident, int facility, int level, int to_stderr);
void log_set_level(int level); // 0..5 as above
void log_set_level_str(const char* level_str); // "FATAL", "ERROR", etc.

// Convert between textual and numeric log levels
int log_level_from_string(const char* level_str);
const char* log_level_to_string(int level);

// Core logging entry point
void log_log(int level, const char* file, int line, const char* fmt, ...);

#endif
