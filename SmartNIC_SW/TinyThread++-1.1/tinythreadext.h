#ifndef __tinythreadext_h__
#define __tinythreadext_h__

#include <stdbool.h>
#include <errno.h>
#include <sys/time.h>

#include "fast_mutex.h"
#include "tinythread.h"

/**
 *  Implementation of some obvious and usefull stuff that is missing from tinythreads.
 *
 *  Mutex wait with a timeout and thread join with a timeout.
 */
namespace tthread
{
#if defined(_TTHREAD_WIN32_)
    template <class _mutexT>
    inline bool _wait(_mutexT &aMutex, int milliSeconds)
    {
        // Wait for either event to become signaled due to notify_one() or
        // notify_all() being called or timeout
        int result = WaitForMultipleObjects(2, mEvents, FALSE, milliSeconds);
        if (result == WAIT_TIMEOUT)
        {
            return false;
        }

        // Check if we are the last waiter
        EnterCriticalSection(&mWaitersCountLock);
        -- mWaitersCount;
        bool lastWaiter = (result == (WAIT_OBJECT_0 + _CONDITION_EVENT_ALL)) &&
                            (mWaitersCount == 0);
        LeaveCriticalSection(&mWaitersCountLock);

        // If we are the last waiter to be notified to stop waiting, reset the event
        if(lastWaiter)
            ResetEvent(mEvents[_CONDITION_EVENT_ALL]);
        return true;
    }
#else
    // Help struct to access private members in other classes
    struct Cheat
    {
        condition_variable * pCondition;
        pthread_cond_t conditionHandle;
        mutex * pMutex;
        pthread_mutex_t mutexHandle;
    };

    // Specialize condition_variable template method wait so that
    // we can cheat access to private variable mHandle in class condition_variable:
    template<> inline void condition_variable::wait(Cheat & cheat)
    {
        cheat.conditionHandle = cheat.pCondition->mHandle;
        // Luckily condition_variable is a friend of mutex so we have full access!
        cheat.mutexHandle = cheat.pMutex->mHandle;
    }

#endif // _TTHREAD_WIN32_

    /// Wait for the condition with a timeout.
    /// The function will block the calling thread until the condition variable
    /// is woken by @c notify_one(), @c notify_all() or a spurious wake up or times out.
    /// @param[in] aMutex A mutex that will be unlocked when the wait operation
    ///   starts, an locked again as soon as the wait operation is finished.
    template <class _mutexT>
    inline int wait(condition_variable & condition, _mutexT & aMutex, int milliSeconds)
    {
#if defined(_TTHREAD_WIN32_)
        // Increment number of waiters
        EnterCriticalSection(&mWaitersCountLock);
        ++ mWaitersCount;
        LeaveCriticalSection(&mWaitersCountLock);

        // Release the mutex while waiting for the condition (will decrease
        // the number of waiters when done)...
        aMutex.unlock();
        bool signalled = _wait(aMutex, milliSeconds);
        aMutex.lock();
        if (signalled)
        {
            return 0;
        }
        return ETIMEDOUT;
#elif defined(_TTHREAD_POSIX_)
        // This is a nasty but completely legal way to cheat access to private member
        // variables condition.mHandle and aMutex.mHandle.
        Cheat cheat;
        cheat.pCondition = &condition;
        cheat.pMutex = &aMutex;
        condition.wait(cheat);
        // Values of condition.mHandle and aMutex.mHandle are now available in cheat.

        struct timeval now;
        ::gettimeofday(&now, NULL);
        struct timespec waitTime; // Absolute wait time in future
        waitTime.tv_sec = now.tv_sec + time_t(milliSeconds) / 1000U;
        waitTime.tv_nsec = (now.tv_usec + 1000L * (milliSeconds % 1000L)) * 1000L;
        waitTime.tv_sec += time_t(waitTime.tv_nsec / 1000000000L);
        waitTime.tv_nsec = waitTime.tv_nsec % 1000000000L;
        int result = pthread_cond_timedwait(&cheat.conditionHandle, &cheat.mutexHandle, &waitTime);
        return result;
#endif
    }

#ifndef __FreeBSD__ // FreeBSD seems to be missing a pthread_timedjoin_np function
    /// Wait for the thread to finish with a timeout (join execution flows).
    /// After calling @c join(...) returning true, the thread object is no longer associated with
    /// a thread of execution (i.e. it is not joinable, and you may not join
    /// with it nor detach from it).
    /// @return Returns false if timed out, true otherwise, i.e. the thread was successfully joined.
    inline bool join(tthread::thread & thread, int milliseconds)
    {
        if (milliseconds < 0)
        {
            thread.join();
            return true;
        }

        if (thread.joinable())
        {
    #if defined(_TTHREAD_WIN32_)
            DWORD result = WaitForSingleObject(mHandle, milliseconds);
            bool returnValue = result == WAIT_OBJECT_0;
            if (returnValue)
                thread.detach(); // Why does join() not call detach()?
            return returnValue;
    #elif defined(_TTHREAD_POSIX_)
            struct timespec timeout;
            timeout.tv_sec = milliseconds / 1000;
            timeout.tv_nsec = (milliseconds % 1000) * 1000000L;
            int result = pthread_timedjoin_np(thread.native_handle(), NULL, &timeout);
            if (result == 0)
                thread.detach(); // Why does join() not call detach()?
            return result == 0;
    #endif
        }

        return true;
    }
#endif // __FreeBSD__

} // namespace tthread

#endif // __tinythreadext_h__
