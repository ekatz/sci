#include "sci/Utils/Mutex.h"

void CreateMutex(Mutex *mutex, bool recursive)
{
#if defined(DEBUG)
#if defined(__WINDOWS__)
    InitializeCriticalSectionEx(mutex, 4000, 0);
#else
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    if (recursive) {
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    }
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutex_init(mutex, &attr);
    pthread_mutexattr_destroy(&attr);
#endif
#else
#if defined(__WINDOWS__)
    InitializeCriticalSectionEx(mutex, 4000, CRITICAL_SECTION_NO_DEBUG_INFO);
#else
    if (recursive) {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(mutex, &attr);
        pthread_mutexattr_destroy(&attr);
    } else {
        pthread_mutex_init(mutex, NULL);
    }
#endif
#endif
}

void DestroyMutex(Mutex *mutex)
{
#if defined(__WINDOWS__)
    DeleteCriticalSection(mutex);
#else
    pthread_mutex_destroy(mutex);
#endif
}
