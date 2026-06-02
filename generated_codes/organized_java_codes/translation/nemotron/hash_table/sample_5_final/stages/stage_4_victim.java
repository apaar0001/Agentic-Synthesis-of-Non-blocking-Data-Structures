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


    private static final int DEFAULT_CAPACITY = 1 << 16;
    private final int capacity;
    private final int capacityMask;
    private final AtomicReference<Node>[] table;

    @SuppressWarnings("unchecked")
    public ConcurrentDataStructure() {
        this.capacity = DEFAULT_CAPACITY;
        this.capacityMask = capacity - 1;
        AtomicReference<Node>[] tmp = new AtomicReference[capacity];
        for (int i = 0; i < capacity; i++) {
            tmp[i] = new AtomicReference<>(new Node(Integer.MIN_VALUE, null));
        }
        this.table = tmp;
    }

    private int hash(int key) {
        int h = Integer.hashCode(key);
        h = (h ^ (h >>> 16)) & 0x7fffffff;
        return h & capacityMask;
    }

    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int h = hash(key);
        while (true) {
            AtomicReference<Node> headRef = table[h];
            Node pred = headRef.get();
            Node curr = pred.next.getReference();
            while (true) {
                Node succ = curr.next.getReference();
                boolean marked = curr.next.isMarked();
                if (marked) {
                    if (pred.next.compareAndSet(curr, succ, false, false)) {
                        curr = pred.next.getReference();
                        continue;
                    } else {
                        break;
                    }
                }
                if (curr != null && curr.key < key) {
                    pred = curr;
                    curr = succ;
                    continue;
                }
                if (curr != null && curr.key == key) {
                    return false;
                }
                Node node = new Node(key, curr);
                if (pred.next.compareAndSet(curr, node, false, false)) {
                    return true;
                } else {
                    break;
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int h = hash(key);
        while (true) {
            AtomicReference<Node> headRef = table[h];
            Node pred = headRef.get();
            Node curr = pred.next.getReference();
            while (true) {
                Node succ = curr.next.getReference();
                boolean marked = curr.next.isMarked();
                if (marked) {
                    if (pred.next.compareAndSet(curr, succ, false, false)) {
                        curr = pred.next.getReference();
                        continue;
                    } else {
                        break;
                    }
                }
                if (curr != null && curr.key < key) {
                    pred = curr;
                    curr = succ;
                    continue;
                }
                if (curr == null || curr.key != key) {
                    return false;
                }
                if (curr.next.attemptMark(succ, true)) {
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
                    pred.next.compareAndSet(curr, succ, false, false);
                    return true;
                } else {
                    break;
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int h = hash(key);
        while (true) {
            AtomicReference<Node> headRef = table[h];
            Node pred = headRef.get();
            Node curr = pred.next.getReference();
            while (true) {
                Node succ = curr.next.getReference();
                boolean marked = curr.next.isMarked();
                if (marked) {
                    if (pred.next.compareAndSet(curr, succ, false, false)) {
                        curr = pred.next.getReference();
                        continue;
                    } else {
                        break;
                    }
                }
                if (curr != null && curr.key < key) {
                    pred = curr;
                    curr = succ;
                    continue;
                }
                if (curr != null && curr.key == key) {
                    return true;
                }
                return false;
            }
        }
    }

    private static class Node {
        final int key;
        final AtomicMarkableReference<Node> next;

        Node(int key, Node next) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(next, false);
        }
    }
}