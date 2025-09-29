// Thingino ONVIF logging adapter implementation
#include "log.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

static struct {
    int max_level;  // 0..5 (0=FATAL .. 5=TRACE)
    int to_stderr;  // foreground mode: also print to stderr
    int facility;   // syslog facility (e.g., LOG_DAEMON)
    char ident[32]; // openlog ident (truncated if longer)
    pthread_mutex_t err_mutex;
    int initialized;
} G = {.max_level = 0, .to_stderr = 0, .facility = LOG_DAEMON, .ident = {0}, .err_mutex = PTHREAD_MUTEX_INITIALIZER, .initialized = 0};

static const char* level_str(int lvl)
{
    switch (lvl) {
    case LOG_LVL_FATAL:
        return "FATAL";
    case LOG_LVL_ERROR:
        return "ERROR";
    case LOG_LVL_WARN:
        return "WARN";
    case LOG_LVL_INFO:
        return "INFO";
    case LOG_LVL_DEBUG:
        return "DEBUG";
    case LOG_LVL_TRACE:
        return "TRACE";
    default:
        return "UNK";
    }
}

static int syslog_prio(int lvl)
{
    switch (lvl) {
    case LOG_LVL_FATAL:
        return LOG_CRIT;
    case LOG_LVL_ERROR:
        return LOG_ERR;
    case LOG_LVL_WARN:
        return LOG_WARNING;
    case LOG_LVL_INFO:
        return LOG_INFO;
    case LOG_LVL_DEBUG:
        return LOG_DEBUG;
    case LOG_LVL_TRACE:
        return LOG_DEBUG; // map TRACE to DEBUG priority
    default:
        return LOG_DEBUG;
    }
}

void log_init(const char* ident, int facility, int level, int to_stderr)
{
    if (!ident)
        ident = "onvif";
    memset(G.ident, 0, sizeof(G.ident));
    strncpy(G.ident, ident, sizeof(G.ident) - 1);
    G.facility = facility;
    if (level < LOG_LVL_FATAL)
        level = LOG_LVL_FATAL;
    if (level > LOG_LVL_TRACE)
        level = LOG_LVL_TRACE;
    G.max_level = level;
    G.to_stderr = to_stderr ? 1 : 0;

    openlog(G.ident, LOG_PID, G.facility);
    setlogmask(LOG_UPTO(syslog_prio(G.max_level)));
    G.initialized = 1;
}

void log_set_level(int level)
{
    if (level < LOG_LVL_FATAL)
        level = LOG_LVL_FATAL;
    if (level > LOG_LVL_TRACE)
        level = LOG_LVL_TRACE;
    G.max_level = level;
    if (G.initialized)
        setlogmask(LOG_UPTO(syslog_prio(G.max_level)));
}

void log_set_level_str(const char* level_str)
{
    int level = log_level_from_string(level_str);
    if (level >= 0) {
        log_set_level(level);
    }
}

int log_level_from_string(const char* level_str)
{
    if (!level_str || *level_str == '\0')
        return -1;

    // Convert to uppercase for case-insensitive comparison
    char upper_str[16];
    int i;
    for (i = 0; i < sizeof(upper_str) - 1 && level_str[i]; i++) {
        if (level_str[i] >= 'a' && level_str[i] <= 'z') {
            upper_str[i] = level_str[i] - 'a' + 'A';
        } else {
            upper_str[i] = level_str[i];
        }
    }
    upper_str[i] = '\0';

    if (strcmp(upper_str, "FATAL") == 0) return LOG_LVL_FATAL;
    if (strcmp(upper_str, "ERROR") == 0) return LOG_LVL_ERROR;
    if (strcmp(upper_str, "WARN") == 0) return LOG_LVL_WARN;
    if (strcmp(upper_str, "WARNING") == 0) return LOG_LVL_WARN; // Accept both WARN and WARNING
    if (strcmp(upper_str, "INFO") == 0) return LOG_LVL_INFO;
    if (strcmp(upper_str, "DEBUG") == 0) return LOG_LVL_DEBUG;
    if (strcmp(upper_str, "TRACE") == 0) return LOG_LVL_TRACE;

    // Try to parse as numeric for backward compatibility
    char* endptr;
    long num = strtol(level_str, &endptr, 10);
    if (*endptr == '\0' && num >= LOG_LVL_FATAL && num <= LOG_LVL_TRACE) {
        return (int)num;
    }

    return -1; // Invalid level
}

const char* log_level_to_string(int level)
{
    return level_str(level);
}

void log_log(int level, const char* file, int line, const char* fmt, ...)
{
    if (level < LOG_LVL_FATAL || level > LOG_LVL_TRACE)
        return;
    if (level > G.max_level)
        return; // higher number = more verbose; clamp by max_level

    // Format user message once
    char msgbuf[4096]; // Increased to handle full XML messages (RECV_BUFFER_LEN)
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
    va_end(ap);

    // Trim trailing newline if present
    size_t len = strlen(msgbuf);
    if (len && msgbuf[len - 1] == '\n')
        msgbuf[len - 1] = '\0';

    // Build log message in format: [LEVEL:filename.ext:line]: message
    const char* fname = file ? file : "";
    const char* slash = fname;
    for (const char* p = fname; *p; ++p)
        if (*p == '/')
            slash = p + 1; // basename

    char outbuf[4200]; // Increased to accommodate larger msgbuf plus formatting overhead
    snprintf(outbuf, sizeof(outbuf), "[%s:%s:%d]: %s", level_str(level), slash, line, msgbuf);

    // Emit to syslog
    syslog(syslog_prio(level), "%s", outbuf);

    // Optionally mirror to stderr (thread-safe)
    if (G.to_stderr) {
        pthread_mutex_lock(&G.err_mutex);
        fprintf(stderr, "%s\n", outbuf);
        fflush(stderr);
        pthread_mutex_unlock(&G.err_mutex);
    }
}
