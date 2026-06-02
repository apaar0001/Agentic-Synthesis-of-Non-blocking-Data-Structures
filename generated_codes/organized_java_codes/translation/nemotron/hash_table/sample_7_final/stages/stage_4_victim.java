package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicMarkableReference;

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
        int key;
        AtomicMarkableReference<Node> next;

        Node(int key, Node next) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(next, false);
        }
    }

    private static final int DEFAULT_CAPACITY = 16;
    private final AtomicReference<Node>[] table;

    @SuppressWarnings("unchecked")
    public ConcurrentDataStructure() {
        table = new AtomicReferenceReference[DEFAULT_CAPACITY];
        for (int i = 0; i < DEFAULT_CAPACITY; i++) {
            table[i] = new AtomicReference<>(null);
        }
    }

    private static int hash(int key) {
        return Integer.hashCode(key);
    }

    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int idx = hash(key) & (table.length - 1);
        AtomicReference<Node> bucket = table[idx];
        while (true) {
            Node pred = null;
            Node curr = bucket.get();
            while (true) {
                if (curr == null) {
                    Node newNode = new Node(key, null);
                    if (pred == null) {
                        if (bucket.compareAndSet(null, newNode)) {
                            return true;
                        } else {
                            break;
                        }
                    } else {
                        if (pred.next.compareAndSet(curr, newNode, false, false)) {
                            return true;
                        } else {
                            break;
                        }
                    }
                }
                if (curr.next.isMarked()) {
                    curr = curr.next.getReference();
                    continue;
                }
                int k = curr.key;
                if (k == key) {
                    return false;
                } else if (k > key) {
                    Node newNode = new Node(key, curr);
                    if (pred == null) {
                        if (bucket.compareAndSet(curr, newNode)) {
                            return true;
                        } else {
                            break;
                        }
                    } else {
                        if (pred.next.compareAndSet(curr, newNode, false, false)) {
                            return true;
                        } else {
                            break;
                        }
                    }
                } else {
                    pred = curr;
                    curr = curr.next.getReference();
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int idx = hash(key) & (table.length - 1);
        AtomicReference<Node> bucket = table[idx];
        while (true) {
            Node pred = null;
            Node curr = bucket.get();
            while (true) {
                if (curr == null) {
                    return false;
                }
                if (curr.next.isMarked()) {
                    curr = curr.next.getReference();
                    continue;
                }
                int k = curr.key;
                if (k == key) {
                    if (curr.next.attemptMark(curr.next.getReference(), true)) {
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
                    } else {
                        break;
                    }
                } else if (k > key) {
                    return false;
                } else {
                    pred = curr;
                    curr = curr.next.getReference();
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int idx = hash(key) & (table.length - 1);
        AtomicReference<Node> bucket = table[idx];
        Node curr = bucket.get();
        while (curr != null) {
            if (curr.next.isMarked()) {
                curr = curr.next.getReference();
                continue;
            }
            int k = curr.key;
            if (k == key) {
                return true;
            } else if (k > key) {
                return false;
            } else {
                curr = curr.next.getReference();
            }
        }
        return false;
    }
}