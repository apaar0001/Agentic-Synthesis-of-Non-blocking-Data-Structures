package com.example.utils;

/**
 * Interface for a concurrent lock-free integer Stack.
 * Semantics: LIFO — last-in, first-out.
 */
public interface StackADT {

    /**
     * Pushes a value onto the top of the stack.
     * Must never block; must be lock-free (CAS-based).
     *
     * @param val the integer value to push
     */
    void push(int val);

    /**
     * Pops and returns the top value.
     * Must never block; returns {@code -1} if the stack is empty.
     *
     * @return the top value, or -1 if empty
     */
    int pop();

    /**
     * Returns {@code true} if the stack contains no elements.
     * This is a best-effort snapshot; the stack may change immediately after.
     *
     * @return true if empty
     */
    boolean isEmpty();
}
