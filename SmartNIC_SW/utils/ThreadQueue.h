#ifndef __ThreadQueue_h__
#define __ThreadQueue_h__

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

#include <algorithm>
#include <cstddef>
#include <memory>
#include <queue>
#include "CommonUtilities.h"
#include "ManualResetEvent.h"

#ifndef nullptr
    #define nullptr NULL
#endif

/**
 *  Thread safe queue that signals an event when there is something available in the queue.
 *  A thread can wait in a queue for the event with or without a timeout. Typical usage
 *  scenario is one or more producer threads writing (enqueueing) into a queue and a single consumer
 *  thread reading (dequeueing) from it. Multiple consumers are supported but there are no guarantees
 *  how dequeueing is distributed among them. Default lock is pthread_spinlock_t, you can also
 *  use pthread_mutex_t or tthread::mutex.
 */
template <typename T, typename LOCK = pthread_spinlock_t>
class ThreadQueue
{
private:

    typedef SC_ScopedLock<LOCK> ScopedLock;

    mutable LOCK            _lock;      // Mutex lock used for multithreaded safe access
    ManualResetEvent        _event;     // Event to signal when there is something in the queue
    std::queue<T> *         _queue;     // Queue of items

    /**
     *  Unlocked dequeue for internal use. Replaces the internal queue with an empty
     *  queue and returns the internal queue. Rationale for having empty queue as
     *  parameter and not creating it here is that in some cases it can be created
     *  outside a lock thus shortening the scope of locked code.
     *
     *  @param  emptyQueue  An empty queue that replaces the internal queue.
     *
     *  @return Return the internal queue.
     */
    inline std::queue<T> * UnlockedDequeue(std::queue<T> * emptyQueue)
    {
        std::queue<T> * queue = _queue;
        _queue = emptyQueue;
        return queue;
    }

public:

    /**
     *  Construct a thread queue that signals an event when
     *  there is something available in the queue.
     */
    explicit ThreadQueue()
        : _lock()
        , _event(false)
        , _queue(new std::queue<T>())
    {
        SC_LockInit(_lock, "ThreadQueue", nullptr);
    }

    /**
     *  Destroy the thread queue.
     */
    virtual ~ThreadQueue()
    {
        delete _queue;
        _queue = nullptr;
        SC_LockDestroy(_lock, nullptr);
    }

    /**
     *  Checks whether the queue is empty.
     *
     *  @return True if queue is empty, false otherwise.
     */
    bool Empty() const
    {
        ScopedLock lock(_lock);

        return _queue->empty();
    }

    /**
     *  Count of available queue items.
     *
     *  @return The number of items available in the queue.
     */
    std::size_t Count() const
    {
        ScopedLock lock(_lock);

        return _queue->size();
    }

    /**
     *  Enqueue one item into the queue and signal the event
     *  unless a signal has already been signalled in a previous
     *  call.
     *
     *  @param item - The item that will be enqueued.
     */
    void Enqueue(const T & item)
    {
        ScopedLock lock(_lock);

        bool queueWasEmpty = _queue->size() == 0; // Is queue empty before we add an item?
        _queue->push(item);
        if (queueWasEmpty)
        {
             // Signal there is something in the queue.
             // If the queue was not empty to begin with then
             // we have already signalled the event and the consumer
             // who is waiting to read the queue is going to receive
             // or has already received the event.
            _event.Set();
        }
    }

    /**
     *  Enqueue sequence of items into the queue and signal the event
     *  unless a signal has already been signalled in a previous
     *  call.
     *
     *  @param first - Iterator pointing to the first item to enqueue.
     *  @param last - Iterator pointing past the last item to enqueue.
     */
    void Enqueue(const T * first, const T * last)
    {
        ScopedLock lock(_lock);

        bool queueWasEmpty = _queue->size() == 0; // Is queue empty before we add more items?
        while (first != last)
        {
            _queue->push(*first++);
        }
        if (queueWasEmpty)
        {
             // Signal there is something in the queue.
             // If the queue was not empty to begin with then
             // we have already signalled the event and the consumer
             // who is waiting to read the queue is going to receive
             // or has already received the event.
            _event.Set();
        }
    }

    /**
     *  Dequeue all items from the queue whether there is anything
     *  available or not in which case an empty queue is returned.
     *
     *  @return Queue of items available or an empty queue if nothing
     *          is available.
     */
    std::queue<T> * Dequeue()
    {
        // Empty queue can be created outside the lock:
        std::queue<T> * emptyQueue = new std::queue<T>();

        ScopedLock lock(_lock);

        return UnlockedDequeue(emptyQueue);
    }

    /**
     *  Wait forever until there are items in the queue.
     *
     *  @return Return queue of available items.
     */
    std::queue<T> * WaitAndDequeue()
    {
        // Empty queue can be created outside the lock:
        std::queue<T> * emptyQueue = new std::queue<T>();

        ScopedLock lock(_lock);

        if (_queue->size() == 0)
        {
            _event.WaitOne();
            //_event.Reset();
        }

        return UnlockedDequeue(emptyQueue);
    }

    /**
     *  Wait until there are items in the queue or a milliseconds
     *  timeout has elapsed.
     *
     *  @param milliSeconds - Wait timeout in milliseconds.
     *
     *  @return Return queue of available items or nullptr if the wait
     *          timed out.
     */
    std::queue<T> * WaitAndDequeue(int milliSeconds)
    {
        ScopedLock lock(_lock);

        if (_queue->size() == 0)
        {
            bool signalled = _event.WaitOne(milliSeconds);
            //_event.Reset();
            if (!signalled)
            {
                return nullptr; // Timeout
            }
        }

        return UnlockedDequeue(new std::queue<T>());
    }
};

#endif // __ThreadQueue_h__
