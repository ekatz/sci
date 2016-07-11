#include "Log.h"
#include "Types.h"
#include <stdarg.h>

#if defined(__IOS__)
#include <asl.h>
#include <unistd.h>
#elif defined(__ANDROID__)
#include <android/log.h>
#elif defined(__BLACKBERRY__)
#include <cstdarg>
#include <slog2.h>
extern char *__progname;
#elif defined(__WINDOWS__)
#endif

void LogMessage(int level, const char *format, ...)
{
    va_list args;
    size_t  len;

    assert(format != NULL);
    if (format == NULL || *format == '\0') {
        return;
    }

    va_start(args, format);
    len = (size_t)vsnprintf(NULL, 0, format, args);
    if ((int)len >= 0) {
        char  stackBuffer[300];
        char *buffer = stackBuffer;

#if defined(__IOS__)
        static volatile s_firstTime = true;
        if (s_firstTime) {
            s_firstTime = false;
            asl_add_log_file(NULL, STDERR_FILENO);
        }

        int aslLevel;
        switch (level) {
            case LOG_LEVEL_INFO:
            default:
                aslLevel = ASL_LEVEL_NOTICE;
                break;

            case LOG_LEVEL_WARNING:
                aslLevel = ASL_LEVEL_WARNING;
                break;

            case LOG_LEVEL_ERROR:
                aslLevel = ASL_LEVEL_ERR;
                break;

            case LOG_LEVEL_DEBUG:
                aslLevel = ASL_LEVEL_NOTICE;
                break;
        }
#elif defined(__ANDROID__)
        int logLevel;
        switch (level) {
            case LOG_LEVEL_INFO:
            default:
                logLevel = ANDROID_LOG_INFO;
                break;

            case LOG_LEVEL_WARNING:
                logLevel = ANDROID_LOG_WARN;
                break;

            case LOG_LEVEL_ERROR:
                logLevel = ANDROID_LOG_ERROR;
                break;

            case LOG_LEVEL_DEBUG:
                logLevel = ANDROID_LOG_DEBUG;
                break;
        }
#else
#if defined(__BLACKBERRY__)
        static slog2_buffer_t    bufferHandle = NULL;
        static std::atomic<bool> firstTime(true);
        if (firstTime.load() && firstTime.exchange(false)) {
            slog2_buffer_set_config_t bufferConfig = {
                1, __progname, SLOG2_DEBUG1, { { "SCI", 8 } }
            };
            slog2_register(&bufferConfig, &bufferHandle, 0);
        }

        uint8_t severity;
        switch (level) {
            case LOG_LEVEL_INFO:
            default:
                severity = SLOG2_INFO;
                break;

            case LOG_LEVEL_WARNING:
                severity = SLOG2_WARNING;
                break;

            case LOG_LEVEL_ERROR:
                severity = SLOG2_ERROR;
                break;

            case LOG_LEVEL_DEBUG:
                severity = SLOG2_DEBUG1;
                break;
        }
#endif

#if !defined(__BLACKBERRY__) || defined(DEBUG)
        const char *levelStr;
        size_t      lenLevelStr = 0;
        switch (level) {
            case LOG_LEVEL_INFO:
            default:
                levelStr    = "Info";
                lenLevelStr = strlen(levelStr);
                break;

            case LOG_LEVEL_WARNING:
                levelStr    = "Warning";
                lenLevelStr = strlen(levelStr);
                break;

            case LOG_LEVEL_ERROR:
                levelStr    = "Error";
                lenLevelStr = strlen(levelStr);
                break;

            case LOG_LEVEL_DEBUG:
                levelStr    = "Debug";
                lenLevelStr = strlen(levelStr);
                break;
        }
        len += lenLevelStr + 2 + 1; // Add ": " and LF.
#endif
#endif
        if (len >= sizeof(stackBuffer)) {
            buffer = (char *)malloc(len + 1);
        }

        buffer[0] = '\0';
#if defined(__IOS__) || defined(__ANDROID__) || defined(__BLACKBERRY__)
        vsnprintf(buffer, len + 1, format, args);
#else
        strcpy(buffer, levelStr);
        strcat(buffer + lenLevelStr, ": ");
        vsnprintf(
          buffer + lenLevelStr + 2, len + 1 - (lenLevelStr + 2), format, args);
#endif
        buffer[len] = '\0';

#if defined(__IOS__)
        asl_log(NULL, NULL, aslLevel, "%s", buffer);
#elif defined(__ANDROID__)
        __android_log_write(logLevel, "SCI", buffer);
#else
#if defined(__BLACKBERRY__)
        slog2c(bufferHandle, 0, severity, buffer);
#endif

#if !defined(__BLACKBERRY__) || defined(DEBUG)
        buffer[len - 1] = '\n';
#if defined(_CONSOLE) || defined(__BLACKBERRY__)
        fprintf(stderr, "%s: %s", levelStr, buffer);
#else
        OutputDebugStringA(buffer);
#endif
#endif
#endif

        if (buffer != stackBuffer) {
            free(buffer);
        }
    }
    va_end(args);
}
