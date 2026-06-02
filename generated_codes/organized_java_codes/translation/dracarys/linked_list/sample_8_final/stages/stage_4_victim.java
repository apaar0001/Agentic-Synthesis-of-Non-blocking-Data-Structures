package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {
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
        final int key;
        final AtomicMarkableReference<Node> next;

        Node(int key, Node next) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(next, false);
        }
    }

    private final AtomicReference<Node> head;

    public ConcurrentDataStructure() {
        head = new AtomicReference<>(new Node(Integer.MIN_VALUE, null));
    }

    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        while (true) {
            Node prev = head.get();
            Node curr = prev.next.getReference();
            while (curr != null && curr.key < key) {
                prev = curr;
                curr = curr.next.getReference();
            }
            if (curr != null && curr.key == key) {
                return false;
            }
            Node newNode = new Node(key, curr);
            if (prev.next.compareAndSet(curr, newNode, false, false)) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        while (true) {
            Node prev = head.get();
            Node curr = prev.next.getReference();
            while (curr != null && curr.key < key) {
                prev = curr;
                curr = curr.next.getReference();
            }
            if (curr == null || curr.key != key) {
                return false;
            }
            Node next = curr.next.getReference();
            if (curr.next.compareAndSet(next, next, false, true)) {
                // Node has been marked
            // Lock-freedom victim stall injection (auto-injected)
            if (_lfShouldStall()) {
                System.err.println("LOG: Victim thread stalling inside remove()");
                try {
                    Thread.sleep(10_000);
                } catch (InterruptedException ignored) {
                }
                System.err.println("LOG: Victim resumed and retiring");
                _lfRetired.set(true);
                return false;
            }
            }
            if (prev.next.compareAndSet(curr, next, false, false)) {
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        Node curr = head.get();
        while (curr != null) {
            if (curr.key == key && !curr.next.isMarked()) {
                return true;
            }
            curr = curr.next.getReference();
        }
        return false;
    }
}