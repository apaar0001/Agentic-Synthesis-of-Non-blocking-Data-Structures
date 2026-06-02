package com.example.Sets;

import com.example.utils.SetADT;
import java.util.Random;
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


    private static final int MAX_LEVEL = 16;
    private static final Random random = new Random();

    private static class Node {
        final int key;
        final AtomicMarkableReference<Node>[] next;
        final int level;

        Node(int key, int level) {
            this.key = key;
            this.next = new AtomicMarkableReference[level];
            for (int i = 0; i < level; i++) {
                this.next[i] = new AtomicMarkableReference<>(null, false);
            }
            this.level = level;
        }
    }

    private final AtomicReference<Node> head;

    public ConcurrentDataStructure() {
        Node sentinel = new Node(Integer.MAX_VALUE, MAX_LEVEL);
        head = new AtomicReference<>(new Node(Integer.MIN_VALUE, MAX_LEVEL));
        for (int i = 0; i < MAX_LEVEL; i++) {
            head.get().next[i].set(sentinel);
        }
    }

    private int randomLevel() {
        int level = 1;
        while (random.nextBoolean() && level < MAX_LEVEL) {
            level++;
        }
        return level;
    }

    private Node find(int key) {
        Node current = head.get();
        Node[] update = new Node[MAX_LEVEL];
        int level = current.level - 1;
        while (level >= 0) {
            Node next = current.next[level].getReference();
            while (next != null && next.key < key) {
                current = next;
                next = current.next[level].getReference();
            }
            update[level] = current;
            level--;
        }
        return current;
    }

    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int level = randomLevel();
        Node newNode = new Node(key, level);
        while (true) {
            Node[] update = new Node[MAX_LEVEL];
            Node current = find(key);
            Node next = current.next[0].getReference();
            if (next != null && next.key == key) {
                return false;
            }
            for (int i = 0; i < level; i++) {
                next = current.next[i].getReference();
                while (next != null && next.key < key) {
                    current = next;
                    next = current.next[i].getReference();
                }
                update[i] = current;
            }
            for (int i = 0; i < level; i++) {
                newNode.next[i].set(update[i].next[i].getReference());
            }
            if (current.next[0].compareAndSet(next, newNode)) {
                for (int i = 1; i < level; i++) {
                    while (!update[i].next[i].compareAndSet(next, newNode)) {
                        current = find(key);
                        next = current.next[i].getReference();
                        while (next != null && next.key < key) {
                            current = next;
                            next = current.next[i].getReference();
                        }
                        update[i] = current;
                    }
                }
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
            Node[] update = new Node[MAX_LEVEL];
            Node current = find(key);
            Node next = current.next[0].getReference();
            if (next == null || next.key != key) {
                return false;
            }
            if (next.next[0].attemptMark(null, true)) {
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
                for (int i = 0; i < current.level; i++) {
                    while (!current.next[i].compareAndSet(next, next.next[i].getReference())) {
                        current = find(key);
                        next = current.next[i].getReference();
                        while (next != null && next.key < key) {
                            current = next;
                            next = current.next[i].getReference();
                        }
                        update[i] = current;
                    }
                }
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        Node current = head.get();
        for (int i = current.level - 1; i >= 0; i--) {
            Node next = current.next[i].getReference();
            while (next != null && next.key < key) {
                current = next;
                next = current.next[i].getReference();
            }
        }
        Node next = current.next[0].getReference();
        return next != null && next.key == key && !next.next[0].isMarked();
    }
}