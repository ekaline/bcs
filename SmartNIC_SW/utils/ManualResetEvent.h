#ifndef __ManualResetEvent_h__
#define __ManualResetEvent_h__

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

#include <stdexcept>
#include <string>
#include <sstream>

#include "tinythreadext.h"

#ifdef WIN32

/**
 *  A manual reset event that can be signalled and waited on with ir without timeout.
 *  It is modelled after the the class with the same name in the .NET Framework.
 *
 *  This Windows specific implementation is temporary until the other implementation
 *  below using tinythreads is properly tested.
 */
class ManualResetEvent
{
private:

    HANDLE _eventHandle;    // Event handle

public:

    explicit ManualResetEvent(bool initialState)
        : _eventHandle(NULL)
    {
        _eventHandle = ::CreateEvent(NULL, TRUE, initialState, NULL);
        if (_eventHandle == NULL)
        {
            std::stringstream s("ManualResetEvent::ManualResetEvent(bool) failed with WIN32 error code ");
            s << ::GetLastError();
            throw std::runtime_error(s.str());
        }
    }

    ~ManualResetEvent()
    {
        if (_eventHandle != NULL)
        {
            ::CloseHandle(_eventHandle);
            _eventHandle = NULL;
        }
    }

    void Reset()
    {
        BOOL returnValue = ::ResetEvent(_eventHandle);
        if (returnValue == 0)
        {
            std::stringstream s("ManualResetEvent::Reset() failed with WIN32 error code ");
            s << ::GetLastError();
            throw std::runtime_error(s.str());
        }
    }

    void Set()
    {
        BOOL returnValue = ::SetEvent(_eventHandle);
        if (returnValue == 0)
        {
            std::stringstream s("ManualResetEvent::Set() failed with WIN32 error code ");
            s << ::GetLastError();
            throw std::runtime_error(s.str());
        }
    }

    bool WaitOne()
    {
        DWORD returnCode = ::WaitForSingleObject(_eventHandle, INFINITE);
        if (returnCode == WAIT_OBJECT_0)
        {
            return true;
        }
        else if (returnCode == WAIT_TIMEOUT)
        {
            return false;
        }
        else if (returnCode == WAIT_ABANDONED)
        {
            std::stringstream s("ManualResetEvent::WaitOne() abandoned mutex object with WIN32 error code ");
            s << ::GetLastError();
            throw std::runtime_error(s.str());
        }
        else
        {
            std::stringstream s("ManualResetEvent::WaitOne() failed with return code 0x");
            s << std::hex << returnCode << std::dec << " and WIN32 error code " << ::GetLastError();
            throw std::runtime_error(s.str());
        }
    }

    bool WaitOne(int milliSeconds)
    {
        DWORD returnCode = ::WaitForSingleObject(_eventHandle, milliSeconds);
        if (returnCode == WAIT_TIMEOUT)
        {
            return false;
        }
        else if (returnCode == WAIT_OBJECT_0)
        {
            return true;
        }
        else if (returnCode == WAIT_ABANDONED)
        {
            std::stringstream s("ManualResetEvent::WaitOne(int) abandoned mutex object with WIN32 error code ");
            s << ::GetLastError();
            throw std::runtime_error(s.str());
        }
        else
        {
            std::stringstream s("ManualResetEvent::WaitOne(int) failed with return code 0x");
            s << std::hex << returnCode << std::dec << " and WIN32 error code " << ::GetLastError();
            throw std::runtime_error(s.str());
        }
    }
};

#else

/**
 *  A manual reset event that can be signalled and waited on with ir without timeout.
 *  It is modelled after the the class with the same name in the .NET Framework.
 */
class ManualResetEvent
{
private:

    tthread::mutex                  _mutex;     // Mutex lock for multithread protection.
    tthread::condition_variable     _condition; // Condition that can notify waiting threads

public:

    /**
     *  Construct a manual reset event with given initial state.
     *
     *  @param initialState Initial signalled state of the event.
     */
    explicit ManualResetEvent(bool initialState)
    {
        if (initialState)
        {
            throw std::invalid_argument("ManualResetEvent(true) not implemented!");
        }
    }

    /**
     *  Reset the event to not signalled state.
     */
    void Reset()
    {
        throw std::runtime_error("ManualRestEvent.Reset() not implemented!");
    }

    /**
     *  Set the event to a signalled state. Waiting thread(s) are notified.
     */
    void Set()
    {
        _condition.notify_all();
    }

    /**
     *  Wait forever for the event to be signalled.
     *
     *  @return Always return true which means the event was signalled.
     */
    bool WaitOne()
    {
        _condition.wait(_mutex);
        return true;
    }

    /**
     *  Wait event for a timeout in milliseconds.
     *
     *  @return True if the event was signalled, false if the wait timed out.
     */
    bool WaitOne(int milliSeconds)
    {
        int result = tthread::wait(_condition, _mutex, milliSeconds);
        switch (result)
        {
            case 0: return true;
            case ETIMEDOUT: return false;
            default:
                std::stringstream message;
                message << "ManualResetEvent.WaitOne(int milliSeconds) failed with error code " << result << ", errno " << errno;
                throw std::runtime_error(message.str());
        }
    }
};

#endif

#endif

