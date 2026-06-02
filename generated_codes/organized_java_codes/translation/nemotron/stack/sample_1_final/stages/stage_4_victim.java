package com.example.Sets;
import com.example.utils.SetADT;

import com.example.utils.StackADT;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements StackADT {
    // Lock-freedom test helpers (auto-injected)
    private static final java.util.concurrent.atomic.AtomicBoolean _lfVictimChosen = new java.util.concurrent.atomic.AtomicBoolean(false);
    private static final ThreadLocal<Integer> _lfOpCount = ThreadLocal.withInitial(() -> 0);
    private static final ThreadLocal<Boolean> _lfRetired =
            ThreadLocal.withInitial(() -> false);

    /**
     * Decide if the *current* thread should become the victim.
     *
     * Each call increments a per-thread operation counter. Once a thread
     * has executed more than 100 operations and no victim has been chosen,
     * it atomically claims the victim role and will then stall.
     */
    private static boolean _lfShouldStall() {
        int c = _lfOpCount.get() + 1;
        _lfOpCount.set(c);
        if (c > 100 && !_lfVictimChosen.get() && _lfVictimChosen.compareAndSet(false, true)) {
            return true;
        }
        return false;
    }

    private static class Node {
        int val;
        Node next;
        Node(int v, Node n) {
            val = v;
            next = n;
        }
    }

    private final AtomicReference<Node> top = new AtomicReference<>(null);

    public ConcurrentDataStructure() {
    }

    @Override
    public void push(int val) {
        Node newNode = new Node(val, null);
        while (true) {
            Node current = top.get();
            newNode.next = current;
            if (top.compareAndSet(current, newNode)) {
                return;
            }
        }
    }

    @Override
    public int pop() {
        if (_lfRetired.get()) {
            return -1;
        }
        while (true) {
            Node current = top.get();
            if (current == null) {
                return -1;
            }
            Node next = current.next;
            if (top.compareAndSet(current, next)) {
                // Pop victim point
            // Lock-freedom victim stall injection (auto-injected)
            if (_lfShouldStall()) {
                System.err.println("LOG: Victim thread stalling inside pop()");
                try {
                    Thread.sleep(10_000);
                } catch (InterruptedException ignored) {
                }
                System.err.println("LOG: Victim resumed and retiring");
                _lfRetired.set(true);
                return -1;
            }
                return current.val;
            }
        }
    }

    @Override
    public boolean isEmpty() {
        if (_lfRetired.get()) {
            return -1;
        }
        return top.get() == null;
    }
}