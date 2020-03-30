#ifndef __stopwatch_h__
#define __stopwatch_h__

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

// C headers
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

    static const uint64_t ONE_BILLION_NS = 1000000000UL;

    /**
     *  Get a timestamp in nanoseconds elapsed from some arbitrary
     *  fixed point in the past. The resulting timestamp has no relation
     *  to any specific time of day or date.
     *
     *  @return Timestamp in nanoseconds.
     */
    static inline uint64_t SC_GetTimestamp()
    {
        struct timespec now;
        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        {
            fprintf(stderr, "SC_GetTimestamp clock_gettime failed with errno %d\n", errno);
            return 0;
        }
        return (uint64_t)now.tv_sec * ONE_BILLION_NS + (uint64_t)now.tv_nsec;
    }

    /**
     *  Get a raw timestamp in nanoseconds elapsed from some arbitrary
     *  fixed point in the past. The resulting timestamp has no relation
     *  to any specific time of day or date.
     *
     *  @return Raw timestamp in nanoseconds.
     */
    static inline uint64_t SC_GetRawTimestamp()
    {
#ifdef CLOCK_MONOTONIC_RAW
        struct timespec now;
        if (clock_gettime(CLOCK_MONOTONIC_RAW, &now) != 0)
        {
            fprintf(stderr, "SC_GetRawTimestamp clock_gettime failed with errno %d\n", errno);
            return 0;
        }
        return (uint64_t)now.tv_sec * ONE_BILLION_NS + (uint64_t)now.tv_nsec;
#else
        return SC_GetTimestamp();
#endif
    }

    /**
     *  Get current system time in nanoseconds since the Epoch (UTC 00:00:00 1 January 1970).
     *
     *  @return     Current system time in nanoseconds.
     */
    static inline uint64_t SC_GetTimeNow()
    {
        struct timespec now;
        if (clock_gettime(CLOCK_REALTIME, &now) != 0)
        {
            fprintf(stderr, "SC_GetRawTimestamp clock_gettime failed with errno %d\n", errno);
            return 0;
        }
        return (uint64_t)now.tv_sec * ONE_BILLION_NS + (uint64_t)now.tv_nsec;
    }

    /**
     *  Get resolution of timestamps in nanoseconds.
     *
     *  @return Timestamp resolution in nanoseconds.
     */
    static inline uint64_t SC_GetTimestampResolution()
    {
        uint64_t start = SC_GetTimestamp();
        uint64_t next;
        while ((next = SC_GetTimestamp()) == start);
        return next - start;
    }

    /**
     *  Get resolution of raw timestamps in nanoseconds.
     *
     *  @return Raw timestamp resolution in nanoseconds.
     */
    static inline uint64_t SC_GetRawTimestampResolution()
    {
        uint64_t start = SC_GetRawTimestamp();
        uint64_t next;
        while ((next = SC_GetRawTimestamp()) == start);
        return next - start;
    }

    /**
     *  Get resolution of system time now in nanoseconds.
     *
     *  @return System time resolution in nanoseconds.
     */
    static inline uint64_t SC_GetTimeNowResolution()
    {
        uint64_t start = SC_GetTimeNow();
        uint64_t next;
        while ((next = SC_GetTimeNow()) == start);
        return next - start;
    }

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

// C++ headers
#include <cerrno>
#include <cstring>
#include <ctime>
#include <sstream>
#include <stdexcept>

/**
 *  Stopwatch class for measuring elapsed time.
 */
class Stopwatch
{
private:

    struct
    {
        uint64_t    StartTime;          // Time interval start.
        uint64_t    TimeInterval;       // Cumulative time elapsed between calls to Start()/Restart() and Stop().
        bool        Running;            // True is stopwatch is started, false if it is stopped.

    }               _values;

public:

    /**
     *  Get a timestamp in nanoseconds elapsed from some arbitrary
     *  fixed point in the past. The resulting timestamp has no relation
     *  to any specific time of day or date.
     *
     *  @return Timestamp in nanoseconds.
     */
    static inline uint64_t GetTimestamp()
    {
        timespec now;
        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        {
            std::stringstream ss;
            ss << "Stopwatch::GetTimestamp clock_gettime failed with errno " << errno;
            throw std::runtime_error(ss.str());
        }
        return uint64_t(now.tv_sec) * ONE_BILLION_NS + uint64_t(now.tv_nsec);
    }

    /**
     *  Get a raw timestamp in nanoseconds elapsed from some arbitrary
     *  fixed point in the past. The resulting timestamp has no relation
     *  to any specific time of day or date.
     *
     *  @return Raw timestamp in nanoseconds.
     */
    static inline uint64_t GetRawTimestamp()
    {
#ifdef CLOCK_MONOTONIC_RAW
        timespec now;
        if (clock_gettime(CLOCK_MONOTONIC_RAW, &now) != 0)
        {
            std::stringstream ss;
            ss << "Stopwatch::GetRawTimestamp clock_gettime failed with errno " << errno;
            throw std::runtime_error(ss.str());
        }
        return uint64_t(now.tv_sec) * ONE_BILLION_NS + uint64_t(now.tv_nsec);
#else
        return GetTimestamp();
#endif
    }

    /**
     *  Get current system time in nanoseconds since the Epoch (UTC 00:00:00 1 January 1970).
     *
     *  @return     Current system time in nanoseconds.
     */
    static inline uint64_t GetTimeNow()
    {
        timespec now;
        if (clock_gettime(CLOCK_REALTIME, &now) != 0)
        {
            std::stringstream ss;
            ss << "Stopwatch::GetTimeNow clock_gettime failed with errno " << errno;
            throw std::runtime_error(ss.str());
        }
        return uint64_t(now.tv_sec) * ONE_BILLION_NS + uint64_t(now.tv_nsec);
    }

    /**
     *  Get resolution of timestamps in nanoseconds.
     *
     *  @return Timestamp resolution in nanoseconds.
     */
    static uint64_t GetTimestampResolution()
    {
        return SC_GetTimestampResolution();
    }

    /**
     *  Get resolution of raw timestamps in nanoseconds.
     *
     *  @return Raw timestamp resolution in nanoseconds.
     */
    static uint64_t GetRawTimestampResolution()
    {
        return SC_GetRawTimestampResolution();
    }

    /**
     *  Get resolution of system time now in nanoseconds.
     *
     *  @return System time resolution in nanoseconds.
     */
    static uint64_t GetTimeNowResolution()
    {
        return SC_GetTimeNowResolution();
    }

    /**
     *  Initializes a new instance of Stopwatch.
     */
    inline explicit Stopwatch()
        : _values()
    {
        Reset();
    }

    /**
     *  Initializes a new instance of Stopwatch and optionally starts it running.
     *
     *  @param  start       True if the stopwatch should be started, false otherwise.
     */
    inline explicit Stopwatch(bool start)
        : _values()
    {
        Reset();

        if (start)
        {
            Start();
        }
    }

    /**
     *  Tells whether the stopwatch is started or stopped.
     *
     *   @return     True if stopwatch has been started, false if it is stopped.
     */
    inline bool Running()
    {
        return _values.Running;
    }

    /**
     *  Starts or resumes measuring elapsed time for an interval.
     *  Interval stops when @ref Stop() method is called.
     */
    inline void Start()
    {
        _values.StartTime = Stopwatch::GetTimestamp();
        _values.Running = true;
    }

    /**
     *  Stops time interval measurement, sets the total elapsed time to zero
     *  and starts measuring elapsed time. Interval stops when @ref Stop() method is called.
     */
    inline void Restart()
    {
        Reset();
        Start();
    }

    /**
     *  Stops measuring elapsed time for an interval that was
     *  started by @ref Start() or @ref Restart(). The elapsed
     *  time interval is cumulated as total elapsed time.
     */
    inline void Stop()
    {
        uint64_t stopTime = Stopwatch::GetTimestamp();
        _values.TimeInterval += stopTime - _values.StartTime;
        _values.Running = false;
    }

    /**
     *  Stops time interval measurement and resets the elapsed time to zero.
     */
    inline void Reset()
    {
        memset(&_values, 0, sizeof(_values));
    }

    /**
     *  Get elapsed time in nanoseconds.
     *
     *  @return Elapsed time in nanoseconds.
     */
    inline uint64_t ElapsedNanoseconds()
    {
        if (_values.Running)
        {
            // Stopwatch is still running, return current cumulated time interval:
            uint64_t now = Stopwatch::GetTimestamp();
            return now - _values.StartTime + _values.TimeInterval;
        }
        return _values.TimeInterval;
    }

    /**
     *  Get elapsed time in microseconds.
     *
     *  @return Elapsed time in microseconds.
     */
    inline double ElapsedMicroseconds()
    {
        return double(ElapsedNanoseconds() / 1000);
    }

    /**
     *  Get elapsed time in milliseconds.
     *
     *  @return Elapsed time in milliseconds.
     */
    inline double ElapsedMilliseconds()
    {
        return double(ElapsedNanoseconds() / 1000000);
    }

    /**
     *  Get elapsed time in seconds.
     *
     *  @return Elapsed time in seconds.
     */
    inline double ElapsedSeconds()
    {
        return double(ElapsedNanoseconds() / 1000000000);
    }
};

#endif /* __cplusplus */

#endif /* __stopwatch_h__ */
