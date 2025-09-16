// Thingino ONVIF logging adapter implementation
#include "log.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
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

    // Build structured key=value envelope
    const char* fname = file ? file : "";
    const char* slash = fname;
    for (const char* p = fname; *p; ++p)
        if (*p == '/')
            slash = p + 1; // basename

    char outbuf[4200]; // Increased to accommodate larger msgbuf plus formatting overhead
    snprintf(outbuf, sizeof(outbuf), "level=%s file=%s line=%d msg=\"%s\"", level_str(level), slash, line, msgbuf);

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
