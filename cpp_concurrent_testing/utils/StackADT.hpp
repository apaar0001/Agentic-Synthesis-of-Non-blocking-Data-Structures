#pragma once

class StackADT {
public:
    virtual ~StackADT() = default;

    /**
     * Pushes a value onto the top of the stack.
     * @param val the integer value to push
     */
    virtual void push(int val) = 0;

    /**
     * Pops and returns the top value.
     * @return the top value, or -1 if empty
     */
    virtual int pop() = 0;

    /**
     * Returns true if the stack contains no elements.
     * @return true if empty
     */
    virtual bool isEmpty() = 0;
};
