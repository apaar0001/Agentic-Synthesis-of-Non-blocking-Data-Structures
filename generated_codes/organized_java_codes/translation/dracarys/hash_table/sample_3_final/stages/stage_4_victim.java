package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicInteger;
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
    private static final int MAX_CAPACITY = 1 << 30;
    private static final int RESIZE_THRESHOLD = 16;

    private final AtomicInteger size;
    private final AtomicReference<Node>[] buckets;

    public ConcurrentDataStructure() {
        size = new AtomicInteger(0);
        buckets = new AtomicReference[INITIAL_CAPACITY];
        for (int i = 0; i < INITIAL_CAPACITY; i++) {
            buckets[i] = new AtomicReference<>();
        }
    }

    private int hash(int key) {
        return Math.abs(key) % buckets.length;
    }

    private Node newNode(int key) {
        return new Node(key, new AtomicMarkableReference<>(null, false));
    }

    private void resize() {
        if (size.get() < RESIZE_THRESHOLD) return;
        AtomicReference<Node>[] newBuckets = new AtomicReference[buckets.length << 1];
        for (int i = 0; i < newBuckets.length; i++) {
            newBuckets[i] = new AtomicReference<>();
        }
        for (int i = 0; i < buckets.length; i++) {
            Node node = buckets[i].get();
            while (node != null) {
                Node next = node.next.getReference();
                int newHash = Math.abs(node.key) % newBuckets.length;
                Node newNext = newBuckets[newHash].get();
                while (newNext != null && newNext.key < node.key) {
                    newNext = newNext.next.getReference();
                }
                if (newNext == null || newNext.key > node.key) {
                    node.next.set(new AtomicMarkableReference<>(newNext, false));
                    newBuckets[newHash].set(node);
                    break;
                } else {
                    node = next;
                }
            }
        }
        buckets = newBuckets;
    }

    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int hash = hash(key);
        Node node = buckets[hash].get();
        while (node != null) {
            if (node.key == key) {
                if (!node.next.isMarked()) {
                    return false;
                } else {
                    Node next = node.next.getReference();
                    if (next == null) {
                        return false;
                    } else if (next.key == key) {
                        return false;
                    } else {
                        Node newNode = newNode(key);
                        newNode.next.set(node.next);
                        if (buckets[hash].compareAndSet(node, newNode)) {
                            size.incrementAndGet();
                            return true;
                        }
                    }
                }
            }
            node = node.next.getReference();
        }
        Node newNode = newNode(key);
        if (buckets[hash].compareAndSet(null, newNode)) {
            size.incrementAndGet();
            return true;
        }
        return add(key);
    }

    @Override
    public boolean remove(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int hash = hash(key);
        Node node = buckets[hash].get();
        while (node != null) {
            if (node.key == key) {
                if (!node.next.isMarked()) {
                    Node next = node.next.getReference();
                    if (next == null) {
                        return false;
                    } else {
                        node.next.set(new AtomicMarkableReference<>(next, true));
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
                        return true;
                    }
                } else {
                    return false;
                }
            }
            node = node.next.getReference();
        }
        return false;
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int hash = hash(key);
        Node node = buckets[hash].get();
        while (node != null) {
            if (node.key == key && !node.next.isMarked()) {
                return true;
            }
            node = node.next.getReference();
        }
        return false;
    }

    private static class Node {
        final int key;
        final AtomicMarkableReference<Node> next;

        Node(int key, AtomicMarkableReference<Node> next) {
            this.key = key;
            this.next = next;
        }
    }
}