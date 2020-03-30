#ifndef __SharedMemory_h__
#define __SharedMemory_h__

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
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>

// C++ headers
#include <cstddef>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>

#include "CommonUtilities.h"

#ifndef nullptr
    #define nullptr NULL
#endif

/**
 *  A shared memory class that allows multiple processes read/write access to shared memory.
 *  T is the type of the shared object. It can be a basic type or struct or class. Type T might
 *  need to support optional assignment and copy construction. The first instance creates
 *  the shared file at /dev/shm/NAME where NAME is the name given to the @ref Create call
 *  below and initializes the shared data inclusive calling the default constructor of user type T.
 *  The last instance to use this shared memory section will call the destructor of the user
 *  type T and delete the file.
 *
 *  @brief
 *
 *  Useful Linux commands for debugging shared memory:
 *
 *  ipcs -m
 *
 *  sudo grep p /proc/ * /maps | grep PROCESSNAME
 *
 *  rm /dev/shm/SharedMemoryTest
 *
 *  For further implementation details see the following web page: https://linux.die.net/man/3/pthread_mutexattr_destroy
 */

STRICT_MODE_OFF
static void * STRICT_MAP_FAILED = MAP_FAILED; // GNU strict mode does not like definition of MAP_FAILED (old style C cast)
STRICT_MODE_ON

template <typename T, typename LOCK = pthread_spinlock_t>
class SharedMemory
{
    NO_COPY_CONSTRUCTION_AND_ASSIGNMENT(SharedMemory);

private:

    /**
     *  This is the shared data in shared memory.
     */
    struct SharedData
    {
        pthread_mutexattr_t         MutexAttribute; ///< Mutex shared attribute used when LOCK is a pthread_mutex_t.
        LOCK                        Lock;           ///< Lock used for multithreaded/multiprocess safe access (pthread_spinlock_t is the default!)
        volatile uint32_t           UserCount;      ///< Count of users of this shared memory. When user count goes to zero the shared memory file is deleted.
        volatile T                  UserData;       ///< The user data to be shared

        /**
         *  Construct the shared data.
         */
        explicit SharedData(std::ostream * pLog)
            : MutexAttribute()
            , Lock()
            , UserCount(0)
            , UserData()
        {
            SC_LockInit(Lock, "SharedMemory::SharedData", &MutexAttribute);
#ifdef DEBUG
            printf("CREATED LOCK ADDRESS %p\n", reinterpret_cast<volatile void *>(&Lock));
#endif

            if (pLog != nullptr)
            {
                *pLog << "SharedMemory::SharedData: lock successfully initialized" << std::endl << std::flush;
            }
        }

        /**
         *  Deconstruct the shared data.
         */
        ~SharedData()
        {
        }

        /**
        *  Deconstruct the shared data.
        */
        void Destroy(std::ostream * pLog)
        {
            SC_LockDestroy(Lock, &MutexAttribute);

            if (pLog != nullptr)
            {
                *pLog << "SharedMemory::SharedData: lock successfully destroyed" << std::endl << std::flush;
            }
        }
    };

    int                     _fd;            // Shared memory file descriptor.
    void *                  _pSharedMemory; // Raw pointer to shared memory.
    SharedData *            _pSharedData;   // The shared data.
    std::size_t             _size;          // Shared memory size.
    std::ostream *          _pLog;          // Optional logging stream.
    std::string             _name;          // Name of the shared memory segment and name of the shared memory file.

    /**
     *  Round requested shared memory size upwards to nearest multiple of page size.
     */
    std::size_t MemorySizeInWholePages(std::size_t size)
    {
        const std::size_t PageSize = std::size_t(getpagesize());

        if ((size % PageSize) == 0)
        {
            return size;
        }

        size = size / PageSize + 1;
        return size * PageSize;
    }

    /**
     *  Stream a standard prologue to log lines.
     */
    std::ostream & Prologue(std::ostream & os)
    {
        os << "SharedMemory(" << getpid() << ":" << _name << ")::";
        return os;
    }

public:

    /**
     *  A scoped lock to use when accessing (reading and/or writing) the shared memory data.
     *  In some consumer producer scenarios locking is not necessary if one process writes to
     *  the shared data and one or more processes just read. This condition has to be evaluated
     *  case by case. Constructor locks and destructor unlocks.
     */
    class ScopedLock
    {
        SharedMemory<T> *   _pSharedMemory;
        bool                _locked;
        std::ostream *      _pLog;

    public:

        /**
         *  Construct and optionally lock scoped lock.
         *
         *   @param
         */
        inline explicit ScopedLock(SharedMemory<T> * pSharedMemory, bool lock, std::ostream * pLog)
            : _pSharedMemory(pSharedMemory), _locked(false), _pLog(pLog)
        {
            if (lock)
            {
                Lock();
            }
        }

        /**
        *  Construct and optionally lock scoped lock.
        */
        inline explicit ScopedLock(SharedMemory<T> * pSharedMemory)
            : _pSharedMemory(pSharedMemory), _locked(false), _pLog(nullptr)
        {
            Lock();
        }

        /**
         *  Unlock and deconstruct the scoped lock.
         */
        inline ~ScopedLock()
        {
            if (_locked)
            {
                Unlock();
            }
        }

        /**
         *  Lock the scoped lock.
         */
        void Lock()
        {
            if (!_locked)
            {
                if (_pLog != nullptr)
                {
                    _pSharedMemory->Prologue(*_pLog) << "SharedMemory::ScopedLock::Lock: locking...";
                }

                SC_Lock(_pSharedMemory->_pSharedData->Lock);
                _locked = true;

                if (_pLog != nullptr)
                {
                    *_pLog << " locked\n";
                }
            }
            else
            {
                std::stringstream ss;
                _pSharedMemory->Prologue(ss) << "SharedMemory::ScopedLock::Lock(): spinlock is already locked!";
                throw std::runtime_error(ss.str());
            }
        }

        /**
         *  Unlock the scoped lock.
         */
        void Unlock()
        {
            if (_locked)
            {
                if (_pLog != nullptr)
                {
                    _pSharedMemory->Prologue(*_pLog) << "SharedMemory::ScopedLock::Unlock: unlocking...";
                }

                SC_Unlock(_pSharedMemory->_pSharedData->Lock);
                _locked = false;

                if (_pLog != nullptr)
                {
                    *_pLog << " unlocked\n";
                }
            }
            else
            {
                std::stringstream ss;
                _pSharedMemory->Prologue(ss) << "SharedMemory::ScopedLock::Unlock(): spinlock is not locked!";
                throw std::runtime_error(ss.str());
            }
        }
    };

    friend class ScopedLock;

    /**
     *  Construct a shared memory object.
     */
    explicit SharedMemory()
        : _fd(-1), _pSharedMemory(STRICT_MAP_FAILED), _pSharedData(nullptr), _size(MemorySizeInWholePages(sizeof(T))), _pLog(nullptr),  _name("")
    {
    }

    /**
     *  Destroy the shared memory object.
     */
    virtual ~SharedMemory()
    {
        Close();
    }

    /**
     *  Open share memory. First process calling create will actually create it
     *  and the corresponding shared memory file at /dev/shm/.
     *
     *  @param  name    Name of the shared object.
     *  @param  mode    Access rights of the shared object. For example 0666.
     *  @param  pLog    Pointer to a logging stream or NULL not to log.
     */
    void Open(const char * name, mode_t mode, std::ostream * pLog)
    {
        _name = name;
        if (_name.length() == 0)
        {
            std::stringstream ss;
            Prologue(ss) << "Open(): name should contain something!";
            throw std::runtime_error(ss.str());
        }
        if (_name[0] != '/')
        {
            _name = "/" + _name;
        }
        _pLog = pLog;

        bool firstUser = false;
        _fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, mode); // Atomic create?
        if (_fd == -1)
        {
            if (errno == EEXIST)
            {
                // Nope, open existing shared memory for read/write access:
                _fd = shm_open(name, O_RDWR, mode);
            }
            if (_fd == -1)
            {
                std::stringstream ss;
                Prologue(ss) << "Open(): shm_open failed with errno " << errno << ": " << strerror(errno);
                throw std::runtime_error(ss.str());
            }
        }
        else
        {
            firstUser = true;
        }

        if (pLog != nullptr)
        {
            Prologue(*pLog) << "Open(): " << (firstUser ? "created" : "opened") << " with file descriptor " << _fd << std::endl;
        }

        if (firstUser)
        {
            if (ftruncate(_fd, __off_t(_size)) == -1) // Size the memory
            {
                std::stringstream ss;
                Prologue(ss) << "Open(): ftruncate failed with errno " << errno << ": " << strerror(errno);
                throw std::runtime_error(ss.str());
            }

            if (pLog != nullptr)
            {
                Prologue(*pLog)  << "Open(): shared memory file size truncated to " << _size << " bytes" << std::endl;
            }
        }
        else
        {
            // Second or later user, check that shared memory file size is as expected:
            off_t size = lseek(_fd, 0, SEEK_END);
            if (size == -1)
            {
                std::stringstream ss;
                Prologue(ss) << "Open(): lseek failed with errno " << errno << ": " << strerror(errno);
                throw std::runtime_error(ss.str());
            }

            if (pLog != nullptr)
            {
                Prologue(*pLog) << "Open(): shared memory file size found to be " << size << ", expected size is " << _size << std::endl;
            }

            if (static_cast<size_t>(size) != _size)
            {
                std::stringstream ss;
                Prologue(ss) << "shared memory file size found to be " << size << " but expected size is " << _size;
                throw std::runtime_error(ss.str());
            }
        }

        _pSharedMemory = mmap(/*reinterpret_cast<void *>(0x7ffff7ff7000)*/nullptr, _size, PROT_READ | PROT_WRITE, MAP_SHARED, _fd, 0);
        if (_pSharedMemory == STRICT_MAP_FAILED)
        {
            std::stringstream ss;
            Prologue(ss) << "Open(): mmap failed with errno " << errno << ": " << strerror(errno);
            throw std::runtime_error(ss.str());
        }

        if (pLog != nullptr)
        {
            Prologue(*pLog) << "Open(): mapped to virtual address " << _pSharedMemory << std::endl;
        }

        if (firstUser)
        {
            // Only first user initializes the shared memory.
            memset(_pSharedMemory, 0, _size);   // Zero the memory

            // Now construct the memory by calling the in-place constructor.
            // Default constructor of user data type T will also be called.
            _pSharedData = new(_pSharedMemory) SharedData(pLog);

            if (pLog != nullptr)
            {
                Prologue(*pLog) << "Open(): shared memory zeroed and then constructed" << std::endl;
            }
        }
        else
        {
            _pSharedData = static_cast<SharedData *>(_pSharedMemory);
        }
        uint32_t userCount = __sync_add_and_fetch(&_pSharedData->UserCount, 1);
        if (_pLog != nullptr)
        {
            Prologue(*_pLog) << "Open(): user count is " << userCount << std::endl;
        }
    }

    /**
     *  Close shared memory.
     */
    void Close()
    {
        if (_pSharedData != nullptr && _pSharedData->UserCount > 0)
        {
            uint32_t userCount = __sync_sub_and_fetch(&_pSharedData->UserCount, 1);
            if (_pLog != nullptr)
            {
                Prologue(*_pLog) << "Close(): user count is " << userCount << std::endl;
            }

            std::exception * pException = nullptr;

            if (userCount == 0) // Last user?
            {
                // Last user cleans up and deconstructs the shared memory.
                // Destructor of user shared data type T is also called.

                _pSharedData->Destroy(_pLog);
                _pSharedData->~SharedData();

                if (_pLog != nullptr)
                {
                    Prologue(*_pLog) << "Close(): shared memory deconstructed" << std::endl;
                }
            }

            if (_pSharedMemory != STRICT_MAP_FAILED)
            {
                // Unmap the memory:

                if (munmap(_pSharedMemory, _size) != 0)
                {
                    std::stringstream ss;
                    Prologue(ss) << "Close(): munmap(0x" << std::hex << _pSharedMemory << ", " << _size << ") failed with errno "
                        << std::dec << errno << ": " << strerror(errno);
                    pException = new std::runtime_error(ss.str());
                }

                if (_pLog != nullptr)
                {
                    Prologue(*_pLog) << "Close(): shared memory munmapped from raw address " << _pSharedMemory << std::endl;
                }

                _pSharedMemory = STRICT_MAP_FAILED;
            }

            _pSharedData = nullptr;

            if (_fd != -1)
            {
                // Close the file descriptor:

                if (close(_fd) == -1)
                {
                    std::stringstream ss;
                    if (pException != nullptr)
                    {
                        ss << pException->what() << std::endl;
                        delete pException;
                    }
                    Prologue(ss) << "Close(): close(" << _fd << ") failed with errno " << errno << ": " << strerror(errno);
                    pException = new std::runtime_error(ss.str());
                }

                if (_pLog != nullptr)
                {
                    Prologue(*_pLog) << "Close(): file descriptor " << _fd << " closed" << std::endl;
                }

                _fd = -1;
            }

            if (_name.length() > 0 && userCount == 0)
            {
                // Last user deletes the share memory file from the file system:
                if (shm_unlink(_name.c_str()) == -1)
                {
                    std::stringstream ss;
                    if (pException != nullptr)
                    {
                        ss << pException->what() << std::endl;
                        delete pException;
                    }
                    Prologue(ss) << "Close(): shm_unlink failed with errno " << errno << ": " << strerror(errno);
                    pException = new std::runtime_error(ss.str());
                }

                if (_pLog != nullptr)
                {
                    Prologue(*_pLog) << "Close(): shared memory file /dev/shm" << _name << " deleted from file system" << std::endl;
                }

                _name = "";
            }

            _pLog = nullptr;

            if (pException != nullptr)
            {
                throw *pException;
            }
        }
    }

    /**
     *  Return shared memory open status.
     *
     *  @return True if shared memory is open, false if it is closed.
     */
    inline bool IsOpen() const
    {
        return _pSharedData != nullptr;
    }

    /**
     *  Get a volatile reference to the shared data.
     *  Besides simple producer consumer scenarios the user is responsible
     *  for protecting read/write access to this data with a @ref ScopedLock.
     *
     *  @return Volatile reference to shared data of type T.
     */
    inline volatile T & GetSharedData()
    {
        return _pSharedData->UserData;
    }

    /**
     *  Get a const volatile  reference to the shared data.
     *  Besides simple producer consumer scenarios the user is responsible
     *  for protecting read/write access to this data with a @ref ScopedLock.
     *
     *  @return  Constant volatile reference to shared data of type T.
     */
    inline const volatile T & GetSharedData() const
    {
        return _pSharedData->UserData;
    }

    /**
     *  Set the value shared data to a copy of the parameter.
     *
     *  @param  userData  The shared user data which is copied from.
     */
    void SetSharedData(const T & userData)
    {
        SharedMemory<T, LOCK>::ScopedLock lock(*this);

        *_pSharedData->UserData = userData;
    }

    /**
     *  Get a volatile  pointer to the spinlock that protects shared
     *  data for multithreaded access.
     *
     *  @return  Volatile reference to shared data lock.
     */

    inline volatile LOCK * GetLock()
    {
#ifdef DEBUG
        printf("GET LOCK ADDRESS %p\n", reinterpret_cast<volatile void *>(&_pSharedData->Lock));
#endif
        return &_pSharedData->Lock;
    }

    /**
     *  Get the name of the shared memory (file).
     *
     *  @return     The name of the shared memory.
     */
    inline const char * Name() const
    {
        return _name.c_str();
    }

    /**
     *  Get pointer to logging stream or NULL if not logging.
     *
     *  @return     Pointer to logging stream or NULL.
     */
    inline std::ostream * LogStream() const
    {
        return _pLog;
    }
};

#endif // __SharedMemory_h__
