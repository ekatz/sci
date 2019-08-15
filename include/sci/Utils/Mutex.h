#ifndef SCI_UTILS_MUTEX_H
#define SCI_UTILS_MUTEX_H

#include "sci/Utils/Types.h"

#if defined(__WINDOWS__)

#ifdef CreateMutex
#undef CreateMutex
#endif

typedef CRITICAL_SECTION Mutex;

#else

#include <pthread.h>

typedef pthread_mutex_t Mutex;
#endif

void CreateMutex(Mutex *mutex, bool recursive);
void DestroyMutex(Mutex *mutex);

#if defined(__WINDOWS__)
#define LockMutex(mutex)    EnterCriticalSection(mutex)
#define UnlockMutex(mutex)  LeaveCriticalSection(mutex)
#define TryLockMutex(mutex) (TryEnterCriticalSection(mutex) != FALSE)
#else
#define LockMutex(mutex)    pthread_mutex_lock(mutex)
#define UnlockMutex(mutex)  pthread_mutex_unlock(mutex)
#define TryLockMutex(mutex) (pthread_mutex_trylock(mutex) == 0)
#endif

#endif // SCI_UTILS_MUTEX_H
