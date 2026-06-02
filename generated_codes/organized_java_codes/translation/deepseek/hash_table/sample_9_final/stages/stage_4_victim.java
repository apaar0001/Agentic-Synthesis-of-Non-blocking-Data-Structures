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
    private static final double LOAD_FACTOR = 0.75;
    private static final int MAX_CAPACITY = 1 << 30;
    
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
    
    private final AtomicReferenceArray<AtomicMarkableReference<Node>> buckets;
    private final AtomicInteger size = new AtomicInteger(0);
    private final AtomicInteger threshold = new AtomicInteger((int)(INITIAL_CAPACITY * LOAD_FACTOR));
    
    public ConcurrentDataStructure() {
        buckets = new AtomicReferenceArray<>(INITIAL_CAPACITY);
        for (int i = 0; i < INITIAL_CAPACITY; i++) {
            buckets.set(i, new AtomicMarkableReference<>(null, false));
        }
    }
    
    private int hash(int key) {
        return key ^ (key >>> 16);
    }
    
    private int bucketIndex(int hash, int capacity) {
        return hash & (capacity - 1);
    }
    
    private boolean casBucket(int index, AtomicMarkableReference<Node> expected, AtomicMarkableReference<Node> update) {
        return buckets.compareAndSet(index, expected, update);
    }
    
    private void maybeResize() {
        int currentSize = size.get();
        int currentCapacity = buckets.length();
        if (currentSize < threshold.get() || currentCapacity >= MAX_CAPACITY) {
            return;
        }
        
        int newCapacity = currentCapacity << 1;
        if (newCapacity > MAX_CAPACITY) {
            return;
        }
        
        AtomicReferenceArray<AtomicMarkableReference<Node>> oldBuckets = buckets;
        if (oldBuckets.length() != currentCapacity) {
            return;
        }
        
        for (int i = 0; i < currentCapacity; i++) {
            AtomicMarkableReference<Node> bucketRef = oldBuckets.get(i);
            Node head = bucketRef.getReference();
            if (head != null) {
                int newIndex = bucketIndex(head.hash, newCapacity);
                if (newIndex != i) {
                    AtomicMarkableReference<Node> newBucketRef = new AtomicMarkableReference<>(head, false);
                    if (!casBucket(newIndex, null, newBucketRef)) {
                        AtomicMarkableReference<Node> existing = buckets.get(newIndex);
                        Node existingHead = existing.getReference();
                        if (existingHead != null) {
                            Node last = head;
                            while (last.next.getReference() != null) {
                                last = last.next.getReference();
                            }
                            last.next.set(existingHead, false);
                        }
                    }
                    bucketRef.set(null, false);
                }
            }
        }
        
        threshold.set((int)(newCapacity * LOAD_FACTOR));
    }
    
    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int hash = hash(key);
        
        while (true) {
            int capacity = buckets.length();
            int index = bucketIndex(hash, capacity);
            AtomicMarkableReference<Node> bucketRef = buckets.get(index);
            Node head = bucketRef.getReference();
            
            if (head == null) {
                Node newNode = new Node(key, hash, null);
                AtomicMarkableReference<Node> newBucketRef = new AtomicMarkableReference<>(newNode, false);
                if (casBucket(index, bucketRef, newBucketRef)) {
                    size.incrementAndGet();
                    maybeResize();
                    return true;
                }
                continue;
            }
            
            Node pred = head;
            Node curr = head;
            while (curr != null) {
                Node succ = curr.next.getReference();
                boolean[] marked = {false};
                curr.next.get(marked);
                
                if (marked[0]) {
                    Node next = curr.next.getReference();
                    if (!pred.next.compareAndSet(curr, next, false, false)) {
                        break;
                    }
                    curr = next;
                    continue;
                }
                
                if (curr.key == key) {
                    return false;
                }
                
                pred = curr;
                curr = succ;
            }
            
            Node newNode = new Node(key, hash, head);
            if (bucketRef.compareAndSet(head, newNode, false, false)) {
                size.incrementAndGet();
                maybeResize();
                return true;
            }
        }
    }
    
    @Override
    public boolean remove(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int hash = hash(key);
        
        while (true) {
            int capacity = buckets.length();
            int index = bucketIndex(hash, capacity);
            AtomicMarkableReference<Node> bucketRef = buckets.get(index);
            Node head = bucketRef.getReference();
            
            if (head == null) {
                return false;
            }
            
            Node pred = head;
            Node curr = head;
            while (curr != null) {
                Node succ = curr.next.getReference();
                boolean[] marked = {false};
                curr.next.get(marked);
                
                if (marked[0]) {
                    Node next = curr.next.getReference();
                    if (!pred.next.compareAndSet(curr, next, false, false)) {
                        break;
                    }
                    curr = next;
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
                        Node next = curr.next.getReference();
                        pred.next.compareAndSet(curr, next, false, false);
                        size.decrementAndGet();
                        return true;
                    }
                    break;
                }
                
                pred = curr;
                curr = succ;
            }
            
            if (head.key == key) {
                Node succ = head.next.getReference();
                if (bucketRef.compareAndSet(head, succ, false, false)) {
                    size.decrementAndGet();
                    return true;
                }
                continue;
            }
            
            return false;
        }
    }
    
    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int hash = hash(key);
        int capacity = buckets.length();
        int index = bucketIndex(hash, capacity);
        AtomicMarkableReference<Node> bucketRef = buckets.get(index);
        Node head = bucketRef.getReference();
        
        if (head == null) {
            return false;
        }
        
        Node curr = head;
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
}