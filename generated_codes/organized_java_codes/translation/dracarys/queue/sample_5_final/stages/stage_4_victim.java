package com.example.Sets;
import com.example.utils.SetADT;

import com.example.utils.QueueADT;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements QueueADT {
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
        AtomicReference<Node> next;

        Node(int val) {
            this.val = val;
            this.next = new AtomicReference<>(null);
        }
    }

    private final AtomicReference<Node> head;
    private final AtomicReference<Node> tail;

    public ConcurrentDataStructure() {
        Node sentinel = new Node(-1);
        head = new AtomicReference<>(sentinel);
        tail = new AtomicReference<>(sentinel);
    }

    @Override
    public void enqueue(int val) {
        Node newNode = new Node(val);
        while (true) {
            Node currentTail = tail.get();
            Node currentTailNext = currentTail.next.get();
            if (currentTail == tail.get()) {
                if (currentTailNext == null) {
                    if (currentTail.next.compareAndSet(null, newNode)) {
                        tail.compareAndSet(currentTail, newNode);
                        return;
                    }
                } else {
                    tail.compareAndSet(currentTail, currentTailNext);
                }
            }
        }
    }

    @Override
    public int dequeue() {
        if (_lfRetired.get()) {
            return -1;
        }
        while (true) {
            Node currentHead = head.get();
            Node currentHeadNext = currentHead.next.get();
            if (currentHead == head.get()) {
                if (currentHeadNext == null) {
                    return -1;
                } else {
                    if (head.compareAndSet(currentHead, currentHeadNext)) {
                        // Dequeue victim point
            // Lock-freedom victim stall injection (auto-injected)
            if (_lfShouldStall()) {
                System.err.println("LOG: Victim thread stalling inside dequeue()");
                try {
                    Thread.sleep(10_000);
                } catch (InterruptedException ignored) {
                }
                System.err.println("LOG: Victim resumed and retiring");
                _lfRetired.set(true);
                return -1;
            }
                        return currentHeadNext.val;
                    }
                }
            }
        }
    }

    @Override
    public boolean isEmpty() {
        if (_lfRetired.get()) {
            return -1;
        }
        return head.get().next.get() == null;
    }
}