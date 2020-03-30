#ifndef __RingBuffer_h__
#define __RingBuffer_h__

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

#include <cassert>
#include <stdint.h>
#include <sstream>
#include <stdexcept>
#include <typeinfo>

#include "CommonUtilities.h"


/**
 *  A ring buffer class. Elements in the ring buffer can be of any type T which supports
 *  assigment and copy construction. Default lock is pthread_spinlock_t, you can also
 *  use pthread_mutex_t or tthread::mutex.
 *
 *  @brief
 *
 *  There are (at least!) 2 ways to implement a ring buffer with head and tail positions
 *  for writing and reading:
 *  - Using a fill count
 *  - Using a flip marker
 *
 *  This implementation uses the flip marker method.
 *  The tail position is computed based on the head and flip state.
 *
 *  Two kinds of reads are supported:
 *  - rereading a sequence of elements again and again before releasing them.
 *  - reading one sequence of elements at a time and releasing the sequence space for writing,
 *    i.e. the sequence of elements cannot be read multiple times.
 */
template <typename T, typename LOCK = pthread_spinlock_t>
class RingBuffer
{
private:

    typedef SC_ScopedLock<LOCK> ScopedLock;

    mutable LOCK        _lock;              // Lock used for multithreaded safe access.
    T * const           _buffer;            // Data in the ring buffer.
    const std::size_t   _totalCapacity;     // Total capacity of the buffer in units of sizeof(T).
    std::size_t         _head;              // Head position in the buffer where new data can be written.
    std::size_t         _tail;              // Tail position in the buffer where data can be read from.
    bool                _flipped;           // True when _head and _tail positions are flipped.

    inline void SanityChecks(const T * buffer)
    {
#if defined(ENABLE_DEBUG) || defined(_DEBUG) || !defined(NDEBUG)
        assert(buffer == nullptr || buffer >= _buffer);
        assert(buffer == nullptr || buffer < _buffer + _totalCapacity);
        assert(_head < _totalCapacity);
        assert(_tail <= _totalCapacity);
        assert(GetUsedCapacity() <= _totalCapacity);
        assert(GetFreeCapacity() <= _totalCapacity);
        assert(GetUsedCapacity() + GetFreeCapacity() == _totalCapacity);
        assert(!_flipped ? _head >= _tail : _tail >= _head);
#else
        UNUSED_ALWAYS(buffer);
#endif
    }

    template <typename U> friend void RingBufferUnitTests();

public:

    typedef T ElementType;

    const std::size_t & TotalCapacity;  /**< Total capacity of the buffer. */

    /**
     *  Construct a ring buffer from a buffer pointer and total size.
     *
     *  @param  buffer          Pointer to buffer that contains the ring buffer data.
     *  @param  totalCapacity   Total size of the ring buffer.
     */
    explicit RingBuffer(T * buffer, std::size_t totalCapacity)
        : _lock()
        , _buffer(buffer)
        , _totalCapacity(totalCapacity)
        , _head(0)
        , _tail(0)
        , _flipped(false)
        , TotalCapacity(_totalCapacity)
    {
        SC_LockInit(_lock, "RingBuffer", nullptr);
    }

    /**
     *  Destruct a ring buffer.
     *
     */
    ~RingBuffer()
    {
        SC_LockDestroy(_lock, nullptr);
    }

    /**
     *  Get total capacity of the buffer.
     *
     *  @return The total buffer capacity in units of sizeof(T).
     */
    inline std::size_t GetCapacity() const
    {
        return _totalCapacity;
    }

    /**
     *  Get used capacity in the buffer.
     *
     *  @return The used buffer capacity in units of sizeof(T).
     */
    std::size_t GetUsedCapacity() const
    {
        if (!_flipped)
        {
            return _head - _tail;
        }
        return _totalCapacity - (_tail - _head);
    }

    /**
     *  Get free capacity in the buffer.
     *
     *  @return The free buffer capacity in units of sizeof(T).
     */
    std::size_t GetFreeCapacity() const
    {
        if (!_flipped)
        {
            return _totalCapacity - (_head - _tail);
        }
        return _tail - _head;
    }

    /**
     *  Returns a pointer into the buffer where a continuous section of given
     *  length can be written. Returns nullptr if such continuous memory is not
     *  available. Access is NOT multithread safe. If last continuous FreeReadableSection
     *  at the end of ring buffer is not long enough then pUnusableLength tells caller
     *  how much was allocated at the end of the ring buffer as unusable memory.
     *
     *  @param  sectionLength   Length of memory section required.
     *  @param  pUnusableLength Pointer to unusable length if a continuous section of memory could not be allocated.
     *
     *  @return Pointer to start of continuous writable memory section or nullptr if continuous memory could not be allocated.
     */
    T * GetWritableSection(std::size_t length, OUT std::size_t * pUnusableLength)
    {
        *pUnusableLength = 0;
        T * writableBuffer = nullptr;
        if (length <= GetFreeCapacity())
        {
            // There is enough free capacity
            if (!_flipped)
            {
                // _tail is lower than or equal to _head - free sections are:
                // 1. from _head to _totalCapacity
                // 2. from 0 to _tail

                if (_head + length <= _totalCapacity)
                {
                    // There is enough continuous space available above _head
                    writableBuffer = &_buffer[_head];
                    _head += length;
                }
                else
                {
                    // Not enough continuous space available.
                    writableBuffer = &_buffer[_head];
                    std::size_t unusableSectionLength = _totalCapacity - _head;
                    _head = 0; // Flip head around
                    _flipped = true;
                    // Signal split memory section to caller
                    *pUnusableLength = unusableSectionLength;
                }
            }
            else
            {
                // _head is below or equal to _tail - free sections are:
                // 1. from _head to _tail (and we already know it is long enough!)
                writableBuffer = &_buffer[_head];
                _head += length;
            }
        }

        SanityChecks(writableBuffer);

        return writableBuffer;
    }

    /**
     *  Returns a pointer into the buffer where a continuous section of given
     *  length can be written. Returns nullptr if such continuous memory is not
     *  available. Access is multithread safe.
     *
     *  @param  sectionLength   Length of memory section required.
     *  @param  pUnusableLength Pointer to unusable length if a continuous section of memory could not be allocated.
     *
     *  @return Pointer to start of continuous writable memory section or nullptr if continuous memory could not be allocated.
     */
    T * GetWritableSectionLocked(std::size_t sectionLength, OUT std::size_t * pUnusableLength)
    {
        ScopedLock lock(_lock);

        return GetWritableSection(sectionLength, OUT pUnusableLength);
    }

    /**
     *  Frees a section of ring buffer memory back into the ring buffer as
     *  writable memory that can be used to cache new pcap files.
     *  Returns a pointer to memory at tail which is same as start of free memory.
     *  Multithreaded access is NOT safe.
     *
     * @param   start Pointer to the start of memory section to free.
     * @param   length Length of memory section to free.
     * @return  The start address of the freed memory section or nullptr if the memory could not be freed.
     */
    const T * FreeReadableSection(const T * start, std::size_t length)
    {
         if (start < _buffer || start >= _buffer + _totalCapacity)
        {
            std::stringstream ss;
            ss << "RingBuffer<" << typeid(T).name() << ">::FreeReadableSection buffer start out of range: "
               << std::hex << "start = 0x" << static_cast<const void *>(start)
               << ", buffer = 0x" << static_cast<void * const>(_buffer) << ", total capacity = 0x" << _totalCapacity << std::endl;
            throw std::out_of_range(ss.str());
        }

       const T * freeBuffer = nullptr;
        if (length <= GetUsedCapacity())
        {
            // We can actually free this much, i.e. it is used
            if (!_flipped)
            {
                // _tail is below or equal to _head
                freeBuffer = &_buffer[_tail];
                _tail += length;
            }
            else
            {
                //  _head is below _tail
                if (_tail + length <= _totalCapacity)
                {
                    // Enough continuous space from _tail to end of ring buffer
                    freeBuffer = &_buffer[_tail];
                    _tail += length;
                }
                else
                {
                    // Split section
                    freeBuffer = &_buffer[_tail];
                    _tail += length - _totalCapacity;
                    _flipped = false;
                }
            }
        }

        SanityChecks(freeBuffer);

        return freeBuffer;
    }

    /**
     *  Frees a section of ring buffer memory back into the ring buffer as
     *  writable memory that can be used to cache new pcap files.
     *  Returns a pointer to memory at tail which is same as start of free memory.
     *  Multithreaded access is safe.
     *
     * @param   start Pointer to the start of memory section to free.
     * @param   length Length of memory section to free.
     * @return  The start address of the freed memory section.
     */
    const T * FreeReadableSectionLocked(const T * start, std::size_t length)
    {
        ScopedLock lock(_lock);

        return FreeReadableSection(start, length);
    }
};

typedef RingBuffer<uint8_t> ByteRingBuffer; ///< Convenience typedef for ring buffer of bytes

#endif // __RingBuffer_h__
