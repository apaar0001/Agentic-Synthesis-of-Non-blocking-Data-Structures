package com.example.utils;

/**
 * Interface for a concurrent lock-free integer Queue.
 * Semantics: FIFO — first-in, first-out.
 */
public interface QueueADT {

    /**
     * Enqueues a value at the tail of the queue.
     * Must never block; must be lock-free (CAS-based).
     *
     * @param val the integer value to enqueue
     */
    void enqueue(int val);

    /**
     * Dequeues and returns the head value.
     * Must never block; returns {@code -1} if the queue is empty.
     *
     * @return the head value, or -1 if empty
     */
    int dequeue();

    /**
     * Returns {@code true} if the queue contains no elements.
     * This is a best-effort snapshot; the queue may change immediately after.
     *
     * @return true if empty
     */
    boolean isEmpty();
}
