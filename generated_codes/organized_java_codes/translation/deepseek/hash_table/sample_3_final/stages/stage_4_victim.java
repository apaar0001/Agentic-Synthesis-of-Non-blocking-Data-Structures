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
    private final AtomicInteger size = new AtomicInteger(0);
    private final AtomicInteger threshold = new AtomicInteger(INITIAL_CAPACITY);
    
    public ConcurrentDataStructure() {
        table = new AtomicReferenceArray<>(INITIAL_CAPACITY);
        for (int i = 0; i < INITIAL_CAPACITY; i++) {
            table.lazySet(i, new Node(-1, i, null));
        }
    }
    
    private static int spread(int h) {
        return (h ^ (h >>> 16)) & HASH_BITS;
    }
    
    private Node findBucket(int hash) {
        int idx = hash & (table.length() - 1);
        Node bucket = table.get(idx);
        if (bucket == null) {
            Node newBucket = new Node(-1, hash, null);
            if (table.compareAndSet(idx, null, newBucket)) {
                return newBucket;
            } else {
                return table.get(idx);
            }
        }
        return bucket;
    }
    
    private boolean helpDelete(Node pred, Node curr) {
        Node succ = curr.next.getReference();
        boolean[] marked = {false};
        Node succ2 = curr.next.get(marked);
        if (marked[0]) {
            pred.next.compareAndSet(curr, succ, false, false);
            return true;
        }
        return false;
    }
    
    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int hash = spread(key);
        while (true) {
            Node bucket = findBucket(hash);
            Node pred = bucket;
            Node curr = bucket.next.getReference();
            
            while (curr != null) {
                boolean[] marked = {false};
                Node succ = curr.next.get(marked);
                
                if (marked[0]) {
                    if (!pred.next.compareAndSet(curr, succ, false, false)) {
                        break;
                    }
                    curr = succ;
                    continue;
                }
                
                if (curr.key == key) {
                    return false;
                }
                
                if (curr.hash > hash) {
                    break;
                }
                
                pred = curr;
                curr = succ;
            }
            
            Node newNode = new Node(key, hash, curr);
            if (pred.next.compareAndSet(curr, newNode, false, false)) {
                int s = size.get();
                if (s + 1 > threshold.get() && table.length() < MAX_CAPACITY) {
                    resize();
                }
                size.incrementAndGet();
                return true;
            }
        }
    }
    
    @Override
    public boolean remove(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int hash = spread(key);
        while (true) {
            Node bucket = findBucket(hash);
            Node pred = bucket;
            Node curr = bucket.next.getReference();
            
            while (curr != null) {
                boolean[] marked = {false};
                Node succ = curr.next.get(marked);
                
                if (marked[0]) {
                    if (!pred.next.compareAndSet(curr, succ, false, false)) {
                        break;
                    }
                    curr = succ;
                    continue;
                }
                
                if (curr.key == key) {
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
                    break;
                }
                
                if (curr.hash > hash) {
                    return false;
                }
                
                pred = curr;
                curr = succ;
            }
            return false;
        }
    }
    
    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int hash = spread(key);
        Node bucket = findBucket(hash);
        Node curr = bucket.next.getReference();
        
        while (curr != null) {
            boolean[] marked = {false};
            curr.next.get(marked);
            
            if (!marked[0] && curr.key == key) {
                return true;
            }
            
            if (curr.hash > hash) {
                return false;
            }
            
            curr = curr.next.getReference();
        }
        return false;
    }
    
    private void resize() {
        int oldCap = table.length();
        if (oldCap >= MAX_CAPACITY) {
            threshold.set(Integer.MAX_VALUE);
            return;
        }
        
        int newCap = oldCap << 1;
        if (newCap > MAX_CAPACITY) {
            newCap = MAX_CAPACITY;
        }
        
        AtomicReferenceArray<Node> newTable = new AtomicReferenceArray<>(newCap);
        for (int i = 0; i < newCap; i++) {
            newTable.lazySet(i, new Node(-1, i, null));
        }
        
        for (int i = 0; i < oldCap; i++) {
            Node bucket = table.get(i);
            if (bucket != null) {
                Node curr = bucket.next.getReference();
                while (curr != null) {
                    boolean[] marked = {false};
                    Node succ = curr.next.get(marked);
                    if (!marked[0]) {
                        int newIdx = curr.hash & (newCap - 1);
                        Node newBucket = newTable.get(newIdx);
                        Node newPred = newBucket;
                        Node newCurr = newBucket.next.getReference();
                        
                        while (newCurr != null && newCurr.hash < curr.hash) {
                            newPred = newCurr;
                            newCurr = newCurr.next.getReference();
                        }
                        
                        Node newNode = new Node(curr.key, curr.hash, newCurr);
                        newPred.next.set(newNode, false);
                    }
                    curr = succ;
                }
            }
        }
        
        for (int i = 0; i < newCap; i++) {
            if (newTable.get(i) == null) {
                newTable.lazySet(i, new Node(-1, i, null));
            }
        }
        
        for (int i = 0; i < oldCap; i++) {
            table.lazySet(i, null);
        }
        
        for (int i = 0; i < newCap; i++) {
            if (table.get(i) == null) {
                table.lazySet(i, newTable.get(i));
            }
        }
        
        threshold.set(newCap);
    }
}