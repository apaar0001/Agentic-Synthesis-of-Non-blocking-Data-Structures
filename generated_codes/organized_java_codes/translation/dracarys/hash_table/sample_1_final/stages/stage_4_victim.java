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

    private static final int INITIAL_CAPACITY = 16;
    private static final float LOAD_FACTOR = 0.75f;

    private AtomicReference<Node>[] table;

    public ConcurrentDataStructure() {
        table = new AtomicReference[INITIAL_CAPACITY];
        for (int i = 0; i < table.length; i++) {
            table[i] = new AtomicReference<>(null);
        }
    }

    private int hash(int key) {
        return key % table.length;
    }

    private Node newNode(int key, Node next) {
        return new Node(key, next);
    }

    private Node findNode(Node head, int key) {
        Node prev = null;
        Node curr = head;
        while (curr != null) {
            if (curr.key == key) {
                return curr;
            }
            prev = curr;
            curr = curr.next.getReference();
        }
        return null;
    }

    private Node findPrevNode(Node head, int key) {
        Node prev = null;
        Node curr = head;
        while (curr != null) {
            if (curr.key == key) {
                return prev;
            }
            prev = curr;
            curr = curr.next.getReference();
        }
        return null;
    }

    private boolean add(int key) {
        int index = hash(key);
        Node head = table[index].getReference();
        Node prev = findPrevNode(head, key);
        if (prev != null) {
            return false;
        }
        Node newNode = newNode(key, head);
        if (table[index].compareAndSet(head, newNode)) {
            return true;
        }
        return add(key);
    }

    private boolean remove(int key) {
        int index = hash(key);
        Node head = table[index].getReference();
        Node prev = findPrevNode(head, key);
        if (prev == null) {
            return false;
        }
        Node curr = prev.next.getReference();
        if (curr.key == key) {
            if (prev.next.compareAndSet(curr, curr.next.getReference())) {
                curr.mark.set(true); // Node has been marked
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
                return true;
            }
        }
        return remove(key);
    }

    private boolean contains(int key) {
        int index = hash(key);
        Node head = table[index].getReference();
        Node curr = head;
        while (curr != null) {
            if (curr.key == key && !curr.mark.get()) {
                return true;
            }
            curr = curr.next.getReference();
        }
        return false;
    }

    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        return add(key);
    }

    @Override
    public boolean remove(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        return remove(key);
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        return contains(key);
    }

    private static class Node {
        int key;
        AtomicMarkableReference<Node> next;
        AtomicBoolean mark;

        Node(int key, Node next) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(next, false);
            this.mark = new AtomicBoolean(false);
        }
    }
}