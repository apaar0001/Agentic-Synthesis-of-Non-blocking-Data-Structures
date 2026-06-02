package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicInteger;
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

    private static final int INITIAL_CAPACITY = 16;
    private static final int MAXIMUM_CAPACITY = 1 << 30;
    private static final float LOAD_FACTOR = 0.75f;
    
    private static class Node {
        final int key;
        final AtomicMarkableReference<Node> next;
        
        Node(int key, Node next) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(next, false);
        }
    }
    
    private static class BucketList {
        final AtomicReference<Node> head;
        
        BucketList() {
            head = new AtomicReference<>(null);
        }
    }
    
    private volatile BucketList[] table;
    private final AtomicInteger size = new AtomicInteger(0);
    private final AtomicInteger threshold = new AtomicInteger((int)(INITIAL_CAPACITY * LOAD_FACTOR));
    
    public ConcurrentDataStructure() {
        table = new BucketList[INITIAL_CAPACITY];
        for (int i = 0; i < INITIAL_CAPACITY; i++) {
            table[i] = new BucketList();
        }
    }
    
    private int hash(int key) {
        return key & (table.length - 1);
    }
    
    private BucketList getBucket(int key) {
        return table[hash(key)];
    }
    
    private void resizeIfNeeded() {
        int currentSize = size.get();
        if (currentSize < threshold.get()) {
            return;
        }
        
        BucketList[] oldTable = table;
        int oldCapacity = oldTable.length;
        if (oldCapacity >= MAXIMUM_CAPACITY) {
            threshold.set(Integer.MAX_VALUE);
            return;
        }
        
        int newCapacity = oldCapacity << 1;
        BucketList[] newTable = new BucketList[newCapacity];
        for (int i = 0; i < newCapacity; i++) {
            newTable[i] = new BucketList();
        }
        
        for (int i = 0; i < oldCapacity; i++) {
            Node current = oldTable[i].head.get();
            while (current != null) {
                boolean[] marked = {false};
                Node next = current.next.get(marked);
                if (!marked[0]) {
                    int newIndex = current.key & (newCapacity - 1);
                    Node newNode = new Node(current.key, newTable[newIndex].head.get());
                    while (!newTable[newIndex].head.compareAndSet(newTable[newIndex].head.get(), newNode)) {
                        newNode = new Node(current.key, newTable[newIndex].head.get());
                    }
                }
                current = next;
            }
        }
        
        table = newTable;
        threshold.set((int)(newCapacity * LOAD_FACTOR));
    }
    
    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        while (true) {
            BucketList bucket = getBucket(key);
            AtomicReference<Node> headRef = bucket.head;
            Node current = headRef.get();
            Node prev = null;
            
            while (current != null) {
                boolean[] marked = {false};
                Node next = current.next.get(marked);
                
                if (marked[0]) {
                    if (prev == null) {
                        if (headRef.compareAndSet(current, next)) {
                            current = headRef.get();
                            continue;
                        } else {
                            break;
                        }
                    } else {
                        if (prev.next.compareAndSet(current, next, false, false)) {
                            current = next;
                            continue;
                        } else {
                            break;
                        }
                    }
                }
                
                if (current.key == key) {
                    return false;
                }
                
                prev = current;
                current = next;
            }
            
            Node newNode = new Node(key, headRef.get());
            if (headRef.compareAndSet(headRef.get(), newNode)) {
                int currentSize = size.incrementAndGet();
                if (currentSize >= threshold.get()) {
                    resizeIfNeeded();
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
            BucketList bucket = getBucket(key);
            AtomicReference<Node> headRef = bucket.head;
            Node current = headRef.get();
            Node prev = null;
            
            while (current != null) {
                boolean[] marked = {false};
                Node next = current.next.get(marked);
                
                if (marked[0]) {
                    if (prev == null) {
                        if (headRef.compareAndSet(current, next)) {
                            current = headRef.get();
                            continue;
                        } else {
                            break;
                        }
                    } else {
                        if (prev.next.compareAndSet(current, next, false, false)) {
                            current = next;
                            continue;
                        } else {
                            break;
                        }
                    }
                }
                
                if (current.key == key) {
                    if (current.next.compareAndSet(next, next, false, true)) {
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
                        if (prev == null) {
                            headRef.compareAndSet(current, next);
                        } else {
                            prev.next.compareAndSet(current, next, false, false);
                        }
                        size.decrementAndGet();
                        return true;
                    }
                    break;
                }
                
                prev = current;
                current = next;
            }
            
            return false;
        }
    }
    
    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        BucketList bucket = getBucket(key);
        Node current = bucket.head.get();
        
        while (current != null) {
            boolean[] marked = {false};
            Node next = current.next.get(marked);
            
            if (!marked[0] && current.key == key) {
                return true;
            }
            
            current = next;
        }
        
        return false;
    }
}