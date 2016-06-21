#include "Timer.h"
#include <time.h>
#if defined(__IOS__)
#include <mach/mach_time.h>
#elif defined(__ANDROID__)
#include <utils/Timers.h>
#elif defined(__WINDOWS__)
#include <Windows.h>
#elif defined(__BLACKBERRY__)
#include <sys/neutrino.h>
#endif

static uint64_t s_startHrTime = 0;

#if !defined(__WINDOWS__)
void Sleep(uint milliseconds)
{
    usleep(milliseconds * 1000);
}
#endif

#if defined(__IOS__)
static mach_timebase_info_data_t s_timebaseInfo = { 0 };

static uint64_t AbsoluteTimeToNanoseconds(uint64_t absoluteTime)
{
    if (s_timebaseInfo.denom == 0) {
        mach_timebase_info(&s_timebaseInfo);
    }
    return (absoluteTime * s_timebaseInfo.numer) / s_timebaseInfo.denom;
}

static uint64_t NanosecondsToAbsoluteTime(uint64_t nanoseconds)
{
    if (s_timebaseInfo.denom == 0) {
        mach_timebase_info(&s_timebaseInfo);
    }
    return (nanoseconds * s_timebaseInfo.denom) / s_timebaseInfo.numer;
}
#endif

uint64_t GetHighResolutionTime(void)
{
#if defined(__IOS__)
    return AbsoluteTimeToNanoseconds(mach_absolute_time());

#elif defined(__ANDROID__)
    return (uint64_t)systemTime(SYSTEM_TIME_MONOTONIC);

#elif defined(__WINDOWS__)
    static double s_frequencyNano = 0.0;
    LARGE_INTEGER now;
    if (s_frequencyNano == 0.0) {
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        s_frequencyNano = 1000000000.0 / (double)frequency.QuadPart;
    }
    QueryPerformanceCounter(&now);
    return (uint64_t)((double)now.QuadPart * s_frequencyNano);

#elif defined(__BLACKBERRY__)
    uint64_t time = 0;
    ClockTime(CLOCK_MONOTONIC, NULL, &time);
    return time;
#endif
}

uint RTickCount(void)
{
    uint     sysTicks;
    uint64_t now;

    if (s_startHrTime == 0) {
        s_startHrTime = GetHighResolutionTime();
        return 0;
    }

    now      = GetHighResolutionTime();
    sysTicks = (uint)(((now - s_startHrTime) * (uint64_t)TICKS_PER_SECOND) /
                      (uint64_t)1000000000);
    return sysTicks;
}

void WaitUntil(uint ticks)
{
    uint64_t now;
    uint64_t end;
    uint     milliseconds;

    if (s_startHrTime == 0) {
        s_startHrTime = GetHighResolutionTime();
    }

    end = s_startHrTime + (((uint64_t)ticks * (uint64_t)1000000000) /
                           (uint64_t)TICKS_PER_SECOND);
    now = GetHighResolutionTime();

    if (end > now) {
        milliseconds = (uint)((end - now) / (uint64_t)1000000);
        Sleep(milliseconds);
    }
}

uint SysTime(int func)
{
    struct tm *newtime;
    time_t     longTime;
    int        fmtTime;

    time(&longTime);
    newtime = localtime(&longTime);

    switch (func) {
        case 1:
            // return packed time
            // HHHH|MMMM|MMSS|SSSS - (hour 1-12)
            fmtTime = newtime->tm_hour;
            if (fmtTime == 0) {
                fmtTime = 12;
            }
            if (fmtTime > 12) {
                fmtTime -= 12;
            }
            fmtTime =
              (fmtTime << 12) | (newtime->tm_min << 6) | (newtime->tm_sec);
            break;

        case 2:
            // return packed time
            // HHHH|HMMM|MMMS|SSSS - (hour 0-24)
            //* note loss of SECOND resolution in this form
            fmtTime = (newtime->tm_hour << 11) | (newtime->tm_min << 5) |
                      (newtime->tm_sec >> 1);
            break;

        case 3:
            // return packed calendar date - (years since 1980)
            // YYYY|YYYM|MMMD|DDDD
            fmtTime = ((newtime->tm_year - 80) << 9) |
                      ((newtime->tm_mon + 1) << 5) | (newtime->tm_mday);
            break;

        default:
            fmtTime = 0;
            break;
    }

    return fmtTime;
}
