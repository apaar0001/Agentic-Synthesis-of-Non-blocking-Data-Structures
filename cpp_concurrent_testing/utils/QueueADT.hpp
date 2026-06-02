#pragma once

class QueueADT {
public:
    virtual ~QueueADT() = default;

    /**
     * Enqueues a value at the tail of the queue.
     * @param val the integer value to enqueue
     */
    virtual void enqueue(int val) = 0;

    /**
     * Dequeues and returns the head value.
     * @return the head value, or -1 if empty
     */
    virtual int dequeue() = 0;

    /**
     * Returns true if the queue contains no elements.
     * @return true if empty
     */
    virtual bool isEmpty() = 0;
};
