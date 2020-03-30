#ifndef __CommonUtilities_h__
#define __CommonUtilities_h__

/*
***************************************************************************
*
* Copyright (c) 2008-2019, Silicom Denmark A/S
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
*
* 3. Neither the name of the Silicom nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* 4. This software may only be redistributed and used in connection with a
*  Silicom network adapter product.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
***************************************************************************/

/* C headers */
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

/* Find C and C++ standard supported by this compiler */
#if defined(__STDC__)
    #define PREDEF_STANDARD_C_1989
    #if defined(__STDC_VERSION__)
        #define PREDEF_STANDARD_C_1990
        #if (__STDC_VERSION__ >= 199409L)
            #define PREDEF_STANDARD_C_1994
        #endif
        #if (__STDC_VERSION__ >= 199901L)
            #define PREDEF_STANDARD_C_1999
        #endif
        #if (__STDC_VERSION__ >= 201112L)
            #define PREDEF_STANDARD_C_2011
        #endif
    #endif

    #ifdef __cplusplus
        #if (__cplusplus >= 199711L)
            #define PREDEF_STANDARD_CPP_1998
        #endif
        #if (__cplusplus >= 201103L)
            #define PREDEF_STANDARD_CPP_2011
        #endif
        #if (__cplusplus >= 201402L)
            #define PREDEF_STANDARD_CPP_2014
        #endif
        #if (__cplusplus >= 201703L)
            #define PREDEF_STANDARD_CPP_2017
        #endif
    #endif
#endif

#if !defined(PREDEF_STANDARD_C_1989)
    #error "Your compiler is too old! Please update to a newer compiler compatible with ANSI X3.159-1989 and ISO/IEC 14882:1998"
#endif

#ifndef STRICT_MODE_OFF

#define STRICT_MODE_OK 0
#if __GNUC__ >= 4
    #if __GNUC_MINOR__ >= 4
        #if __GNUC_PATCHLEVEL__ > 7
            #undef STRICT_MODE_OK
            #define STRICT_MODE_OK 1
        #endif
    #endif
#endif

#if STRICT_MODE_OK == 1

    /* For turning ON/OFF GNU compiler strict warnings */
    #define STRICT_MODE_OFF                                             \
        _Pragma("GCC diagnostic push")                                  \
        _Pragma("GCC diagnostic ignored \"-Wreturn-type\"")             \
        _Pragma("GCC diagnostic ignored \"-Wunused-parameter\"")        \
        _Pragma("GCC diagnostic ignored \"-pedantic\"")                 \
        _Pragma("GCC diagnostic ignored \"-Wshadow\"")                  \
        _Pragma("GCC diagnostic ignored \"-Wold-style-cast\"")          \
        _Pragma("GCC diagnostic ignored \"-Wswitch-default\"")

        /* Addition options that can be enabled
        _Pragma("GCC diagnostic ignored \"-Wpedantic\"")                \
        _Pragma("GCC diagnostic ignored \"-Wformat=\"")                 \
        _Pragma("GCC diagnostic ignored \"-Werror\"")                   \
        _Pragma("GCC diagnostic ignored \"-Werror=\"")                  \
        _Pragma("GCC diagnostic ignored \"-Wunused-variable\"")         \
        _Pragma("GCC diagnostic ignored \"-Wdelete-non-virtual-dtor\"") \
        */

    #define STRICT_MODE_ON                                              \
        _Pragma("GCC diagnostic pop")

#else
    /* GNU C++ 4.4.7 or less (CentOS 6.8 etc) */
    #define STRICT_MODE_OFF
    #define STRICT_MODE_ON
#endif /* STRICT_MODE_OK */

#endif /* #ifndef STRICT_MODE_OFF */

#include <unistd.h>
#ifdef __cplusplus
    #include <string>
    #include <sstream>
    #include <stdexcept>
STRICT_MODE_OFF
    #include "tinythreadext.h"
STRICT_MODE_ON
#endif /* __cplusplus */

inline const char * GetDebugOrReleaseString()
{
#if defined(ENABLE_DEBUG) || defined(_DEBUG) || !defined(NDEBUG)
    return "DEBUG";
#else
    return "RELEASE";
#endif
}

#if !defined(nullptr)
    #define nullptr NULL
#endif

/* For disabling "unused variable warnings" */
#ifndef UNUSED_ALWAYS
    #define UNUSED_ALWAYS(x)    do { (void)(x); } while (0) /* compiler will optimize this code away to nothing but x gets used */
#endif

#define OUT /* Out parameter in C# sense */
#define REF /* Reference parameter in C# sense */

/**
 *  Return the stringified name of argument.
 *
 *  @param  x   Parameter x whose name is desired.
 *
 *  @return     Stringified name of parameter x
 */
#define nameof(x) #x

 /**
 *  Return the number of elements in an array.
 *
 *  @param  array   Array whose size is desired.
 *
 *  @return     Array size (number of elements in array).
 */
#define countof(array)    (sizeof(array) / sizeof((array)[0]))

/**
 *  Macro to define private copy constructor and assignment operator for a class
 *  thus preventing copy construction and assignment.
 */
#define NO_COPY_CONSTRUCTION_AND_ASSIGNMENT(C)  \
    private: \
        explicit C(const C &);      /* No copy construction of class C. */ \
        C & operator=(const C &)    /* No assignment of class C. */

 /**
 *  Macro to define private assignment operator for a class
 *  thus preventing assignment.
 */
#define NO_ASSIGNMENT(C)  \
    private: \
        C & operator=(const C &)    /* No assignment of class C. */

/**
 *  Various non ASCII characters in UTF-8 encoding:
 */

#define UTF_8_GREEK_MU    "\xCE\xBC"      /* UTF-8 Greek Mu, used for "microsecond" for example */
#define UTF_8_PLUS_MINUS  "\xC2\xB1"      /* UTF-8 +- */
#define UTF_8_MEMBERSHIP  "\xE2\x88\x88"  /* UTF -8 group membership character (https://en.wikipedia.org/wiki/Element_(mathematics)#Notation_and_terminology) */

/**
 *  Maximum integer that can be converted to a double without loss of precision.
 */
#define MAX_INT_THAT_FITS_INTO_DOUBLE   9007199254740992ULL

#ifndef MAC_ADDR_LEN
    #define MAC_ADDR_LEN     6  /* Size of a binary MAC address in bytes. */
#endif

typedef uint8_t MacAddress_t[MAC_ADDR_LEN];

/**
 *  Parse a MAC address string into a MAC address binary array.
 */
static inline bool ParseMacAddress(const char * macAddressString, MacAddress_t macAddress)
{
    unsigned int intMacAddress[MAC_ADDR_LEN];
    int i;

    int numberOfItemsFound = sscanf(macAddressString, "%02x:%02x:%02x:%02x:%02x:%02x",
        &intMacAddress[0], &intMacAddress[1], &intMacAddress[2],
        &intMacAddress[3], &intMacAddress[4], &intMacAddress[5]);
    for (i = 0; i < MAC_ADDR_LEN; ++i)
    {
        if (intMacAddress[i] > 255)
        {
            return false;
        }
#ifdef __cplusplus
        macAddress[i] = static_cast<uint8_t>(intMacAddress[i]);
#else
        macAddress[i] = (uint8_t)intMacAddress[i];
#endif
    }
    return numberOfItemsFound == MAC_ADDR_LEN;
}

static inline const char * PrintMacAddress(char * macAddressString, MacAddress_t macAddress)
{
    sprintf(macAddressString, "%02x:%02x:%02x:%02x:%02x:%02x",
        macAddress[0], macAddress[1], macAddress[2],
        macAddress[3], macAddress[4], macAddress[5]);
    return macAddressString;
}

/*================================================================== */

/* C++ definitions */

#ifdef __cplusplus

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

inline std::string Dump(const pthread_spinlock_t & lock)
{
    std::stringstream ss;
    ss << std::hex;
    for (std::size_t i = 0; i < sizeof(lock); ++i)
    {
        ss << std::setw(2) << std::setfill('0') << static_cast<uint16_t>(*(reinterpret_cast<const volatile uint8_t *>(&lock) + i));
    }
    return ss.str();
}

inline void SC_LockInit(pthread_spinlock_t & lock, const char * callContext, pthread_mutexattr_t * pMutexAttribute)
{
    int rc = pthread_spin_init(&lock, pMutexAttribute != nullptr ? PTHREAD_PROCESS_SHARED : PTHREAD_PROCESS_PRIVATE);
    if (rc != 0)
    {
        std::stringstream ss;
        ss << "SC_LockInit from " << callContext << ": pthread_spin_init failed with error code " << rc;
        throw std::runtime_error(ss.str());
    }
}

inline void SC_LockDestroy(pthread_spinlock_t & lock, pthread_mutexattr_t * /*pMutexAttribute*/)
{
    int rc = pthread_spin_destroy(&lock);
    if (rc != 0)
    {
        std::stringstream ss;
        ss << "SC_LockDestroy: pthread_spin_init pthread_spin_destroy with error code " << rc;
        throw std::runtime_error(ss.str());
    }
}

inline void SC_Lock(pthread_spinlock_t & lock)
{
    int rc = pthread_spin_lock(&lock);
    if (rc != 0)
    {
        std::stringstream ss;
        ss << "SC_Lock: pthread_spin_lock failed with error code " << rc;
        throw std::runtime_error(ss.str());
    }
}

inline void SC_Unlock(pthread_spinlock_t & lock)
{
    int rc = pthread_spin_unlock(&lock);
    if (rc != 0)
    {
        std::stringstream ss;
        ss << "SC_Unlock: pthread_spin_unlock failed with error code " << rc;
        throw std::runtime_error(ss.str());
    }
}

inline bool SC_IsLocked(pthread_spinlock_t & lock)
{
    int rc = pthread_spin_trylock(&lock);
    if (rc != 0)
    {
        std::stringstream ss;

        if (rc == EBUSY || rc == EDEADLK)
        {
            return true; /* Is already locked */
        }
        else
        {
            ss << "SC_IsLocked: pthread_spin_trylock failed with error code " << rc;
        }

        throw std::runtime_error(ss.str());
    }
    else
    {
        rc = pthread_spin_unlock(&lock);
        if (rc != 0)
        {
            std::stringstream ss;
            ss << "SC_IsLocked: pthread_spin_unlock failed with error code " << rc;
            throw std::runtime_error(ss.str());
        }

        return false;
    }
}

inline std::string Dump(const pthread_mutex_t & lock)
{
    std::stringstream ss;
    ss << std::hex;
    for (std::size_t i = 0; i < sizeof(lock); ++i)
    {
        ss << std::setw(2) << std::setfill('0') << static_cast<uint16_t>(*(reinterpret_cast<const uint8_t *>(&lock) + i));
    }
    return ss.str();
}

inline void SC_LockInit(pthread_mutex_t & lock, const char * callContext, pthread_mutexattr_t * pMutexAttribute)
{
    if (pMutexAttribute != nullptr)
    {
        int rc = pthread_mutexattr_init(pMutexAttribute);
        if (rc != 0)
        {
            std::stringstream ss;
            ss << "SC_LockInit from " << callContext << ": pthread_mutexattr_init failed with error code " << rc;
            throw std::runtime_error(ss.str());
        }
        rc = pthread_mutexattr_setpshared(pMutexAttribute, PTHREAD_PROCESS_SHARED);
        if (rc != 0)
        {
            std::stringstream ss;
            ss << "SC_LockInit from " << callContext << ": pthread_mutexattr_setpshared failed with error code " << rc;
            throw std::runtime_error(ss.str());
        }
    }

    int rc = pthread_mutex_init(&lock, pMutexAttribute);
    if (rc != 0)
    {
        std::stringstream ss;
        ss << "SC_LockInit from " << callContext << ": pthread_mutex_init failed with error code " << rc;
        throw std::runtime_error(ss.str());
    }
}

inline void SC_LockDestroy(pthread_mutex_t & lock, pthread_mutexattr_t * pMutexAttribute)
{
    if (pMutexAttribute != nullptr)
    {
        int rc = pthread_mutexattr_destroy(pMutexAttribute);
        if (rc != 0)
        {
            std::stringstream ss;
            ss << "SC_LockDestroy: pthread_mutexattr_destroy failed with error code " << rc;
            throw std::runtime_error(ss.str());
        }
    }

    int rc = pthread_mutex_destroy(&lock);
    if (rc != 0)
    {
        std::stringstream ss;
        ss << "SC_LockDestroy: pthread_mutex_destroy failed with error code " << rc;
        throw std::runtime_error(ss.str());
    }
}

inline void SC_Lock(pthread_mutex_t & lock)
{
    int rc = pthread_mutex_lock(&lock);
    if (rc != 0)
    {
        std::stringstream ss;
        ss << "SC_Lock: pthread_mutex_lock failed with error code " << rc;
        throw std::runtime_error(ss.str());
    }
}

inline void SC_Unlock(pthread_mutex_t & lock)
{
    int rc = pthread_mutex_unlock(&lock);
    if (rc != 0)
    {
        std::stringstream ss;
        ss << "SC_Unlock: pthread_mutex_unlock failed with error code " << rc;
        throw std::runtime_error(ss.str());
    }
}

inline bool SC_IsLocked(pthread_mutex_t & lock)
{
    int rc = pthread_mutex_trylock(&lock);
    if (rc != 0)
    {
        std::stringstream ss;

        if (rc == EBUSY || rc == EDEADLK)
        {
            return true; /* Is already locked */
        }
        else
        {
            ss << "SC_IsLocked: pthread_mutex_trylock failed with error code " << rc;
        }

        throw std::runtime_error(ss.str());
    }
    else
    {
        rc = pthread_mutex_unlock(&lock);
        if (rc != 0)
        {
            std::stringstream ss;
            ss << "SC_IsLocked: pthread_mutex_unlock failed with error code " << rc;
            throw std::runtime_error(ss.str());
        }

        return false;
    }
}

inline void SC_LockInit(tthread::mutex & lock, const char * callContext, pthread_mutexattr_t * pMutexAttribute)
{
    /* Nothing to do here, tthread::mutex constructor has already done everything. */

    UNUSED_ALWAYS(lock);
    UNUSED_ALWAYS(callContext);
    if (pMutexAttribute != nullptr)
    {
        throw std::runtime_error("SC_LockInit: tthread::mutex does not support shared memory.");
    }
}

inline void SC_LockDestroy(tthread::mutex & lock, pthread_mutexattr_t * pMutexAttribute)
{
    /* Nothing to do here, tthread::mutex destructor already does everything. */

    UNUSED_ALWAYS(lock);
    if (pMutexAttribute != nullptr)
    {
        throw std::runtime_error("SC_LockInit: tthread::mutex does not support shared memory.");
    }
}

inline bool SC_IsLocked(tthread::mutex & lock)
{
    if (lock.try_lock())
    {
        lock.unlock();
        return false;
    }
    return true;
}

/**
 *  A scoped lock that can use either pthread mutex or pthread spin lock.
 *  For tiny thread mutex see template specialization below.
 */
template <typename T>
class SC_ScopedLock
{
    T &         _lock;

public:

    inline explicit SC_ScopedLock(T & lock)
        : _lock(lock)
    {
        SC_Lock(_lock);
    }

    inline ~SC_ScopedLock()
    {
        SC_Unlock(_lock);
    }

    inline void Lock()
    {
        SC_Lock(_lock);
    }

    inline void Unlock()
    {
        SC_Unlock(_lock);
    }
};

/**
 *  Template specialization of SC_ScopedLock that uses tthread::lock_guard and tthread::mutex.
 */
template <>
class SC_ScopedLock<tthread::mutex> : private tthread::lock_guard<tthread::mutex>
{
public:

    inline explicit SC_ScopedLock(tthread::mutex & lock)
        : tthread::lock_guard<tthread::mutex>(lock)
    {
    }
};

/**
*  A scoped lock based on TinyThreads, pthread spinlock or pthread mutex that releases the lock when going out of scope.
*/
typedef SC_ScopedLock<tthread::mutex> ScopedTinyThreadLock;        /**< Scoped lock based on TinyThreads lock_guard. */
typedef SC_ScopedLock<pthread_spinlock_t> ScopedSpinLock;          /**< Scoped lock based on pthread spin lock. */
typedef SC_ScopedLock<pthread_mutex_t> ScopedMutexLock;            /**< Scoped lock based on pthread mutex. */

/**
 *  Test readability of a given memory segment. This function works
 *  even if normal read access would cause a segmentation fault so
 *  it can be used to test any address.
 *
 *  @param  pAddress    Memory start address to test.
 *  @param  length      Length of memory starting from address to test.
 *
 *  @return             True if memory address is readable, false otherwise.
 */
inline bool ReadableAddress(const void * pAddress, ssize_t length)
{
    int pipefd[2];
    ssize_t rc = pipe(pipefd);
    if (rc == -1)
    {
        std::stringstream ss;
        ss << "WritableAddress pipe failed with errno " << errno;
        ss << " at address " << pAddress << " length " << length;
        throw std::runtime_error(ss.str());
    }

    errno = 0;
    rc = write(pipefd[1], pAddress, size_t(length));
    int writeError = errno;

    close(pipefd[0]);
    close(pipefd[1]);

    if (rc == length && writeError == 0)
    {
        return true;
    }
    else if (rc == -1 && writeError == EFAULT)
    {
        return false;
    }

    std::stringstream ss;
    ss << "WritableAddress write failed with rc " << rc << " and errno " << writeError;
    ss << " at address " << pAddress << " length " << length;
    throw std::runtime_error(ss.str());
}

static inline void _RuntimeError(const char * fileName, int lineNumber, const char * format, va_list arguments)
{
    std::vector<char> buffer;
    buffer.reserve(200);

    while (true)
    {
        int numberOfCharsProduced = vsnprintf(&buffer[0], buffer.capacity(), format, arguments);
        if (numberOfCharsProduced < 0)
        {
            std::stringstream ss;
            ss << "Error in formatting \"" << format << "\" at file \"" << fileName << "\"#" << lineNumber;
            throw std::runtime_error(ss.str());
        }
        if (std::size_t(numberOfCharsProduced) < buffer.capacity())
        {
            break;
        }
        /* Double buffer capacity: */
        buffer.reserve(2 * buffer.capacity());
    }

    throw std::runtime_error(&buffer[0]);
}

struct RuntimeErrorFunctor
{
    const char *    _fileName;
    int             _lineNumber;

    inline explicit RuntimeErrorFunctor(const char * fileName, int lineNumber)
        : _fileName(fileName), _lineNumber(lineNumber)
    {
    }

    inline void operator()(const char * format, ...) __attribute__((format(printf, 2, 3)))
    {
        va_list arguments;
        va_start(arguments, format);
        _RuntimeError(_fileName, _lineNumber, format, arguments);
        va_end(arguments);
    }
};

#define RuntimeError         RuntimeErrorFunctor(__FILE__, __LINE__)

inline bool MacAddressEquals(const MacAddress_t macAddress1, const MacAddress_t macAddress2)
{
    for (int i = 0; i < MAC_ADDR_LEN; ++i)
    {
        if (macAddress1[i] != macAddress2[i])
        {
            return false; /* MAC addresses are not equal */
        }
    }
    return true; /* MAC addresses are equal */
}
#endif /* __cplusplus */

#endif /* __CommonUtilities_h__ */
