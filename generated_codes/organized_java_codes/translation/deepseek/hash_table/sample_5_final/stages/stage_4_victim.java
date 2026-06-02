package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReferenceArray;

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
    private static final int HASH_BITS = 0x7fffffff;

    private static class Node {
        final int key;
        final int hash;
        final AtomicMarkableReference<Node> next;

        Node(int key, int hash, Node next) {
            this.key = key;
            this.hash = hash;
            this.next = new AtomicMarkableReference<>(next, false);
        }
    }

    private final AtomicReferenceArray<Node> table;
    private final AtomicInteger size;

    public ConcurrentDataStructure() {
        this.table = new AtomicReferenceArray<>(INITIAL_CAPACITY);
        this.size = new AtomicInteger(0);
    }

    private static int spread(int h) {
        return (h ^ (h >>> 16)) & HASH_BITS;
    }

    private Node findBucket(int hash) {
        int index = (table.length() - 1) & hash;
        Node dummy = new Node(-1, hash, null);
        while (true) {
            Node current = table.get(index);
            if (current != null) {
                return current;
            }
            if (table.compareAndSet(index, null, dummy)) {
                return dummy;
            }
        }
    }

    private boolean addInternal(int key) {
        int hash = spread(key);
        while (true) {
            Node bucketHead = findBucket(hash);
            Node pred = bucketHead;
            Node curr = pred.next.getReference();
            while (curr != null) {
                boolean[] marked = {false};
                Node succ = curr.next.get(marked);
                if (marked[0]) {
                    Node nextSucc = succ;
                    if (!pred.next.compareAndSet(curr, nextSucc, false, false)) {
                        break;
                    }
                    curr = nextSucc;
                } else {
                    if (curr.key == key) {
                        return false;
                    }
                    pred = curr;
                    curr = succ;
                }
            }
            Node newNode = new Node(key, hash, null);
            if (pred.next.compareAndSet(null, newNode, false, false)) {
                size.incrementAndGet();
                return true;
            }
        }
    }

    private boolean removeInternal(int key) {
        int hash = spread(key);
        while (true) {
            Node bucketHead = findBucket(hash);
            Node pred = bucketHead;
            Node curr = pred.next.getReference();
            while (curr != null) {
                boolean[] marked = {false};
                Node succ = curr.next.get(marked);
                if (marked[0]) {
                    Node nextSucc = succ;
                    if (!pred.next.compareAndSet(curr, nextSucc, false, false)) {
                        break;
                    }
                    curr = nextSucc;
                } else {
                    if (curr.key != key) {
                        pred = curr;
                        curr = succ;
                        continue;
                    }
                    if (curr.next.compareAndSet(succ, succ, false, true)) {
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
                        size.decrementAndGet();
                        return true;
                    }
                }
            }
            return false;
        }
    }

    private boolean containsInternal(int key) {
        int hash = spread(key);
        Node bucketHead = findBucket(hash);
        Node curr = bucketHead.next.getReference();
        while (curr != null) {
            boolean[] marked = {false};
            curr.next.get(marked);
            if (!marked[0] && curr.key == key) {
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
        return addInternal(key);
    }

    @Override
    public boolean remove(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        return removeInternal(key);
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        return containsInternal(key);
    }
}