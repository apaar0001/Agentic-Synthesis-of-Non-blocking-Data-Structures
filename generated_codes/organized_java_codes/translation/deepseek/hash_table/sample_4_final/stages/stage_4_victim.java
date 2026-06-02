package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;
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
    private static final float LOAD_FACTOR = 0.75f;
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
    
    private static class BucketList {
        final AtomicReferenceArray<AtomicReference<Node>> buckets;
        final AtomicInteger size;
        final AtomicInteger threshold;
        
        BucketList(int capacity) {
            buckets = new AtomicReferenceArray<>(capacity);
            for (int i = 0; i < capacity; i++) {
                buckets.set(i, new AtomicReference<Node>(null));
            }
            size = new AtomicInteger(0);
            threshold = new AtomicInteger((int)(capacity * LOAD_FACTOR));
        }
        
        boolean tryResize(BucketList oldList, int newCapacity) {
            if (oldList != this) return false;
            
            for (int i = 0; i < oldList.buckets.length(); i++) {
                AtomicReference<Node> bucketRef = oldList.buckets.get(i);
                Node head = bucketRef.get();
                
                if (head != null) {
                    int newIndex = head.hash & (newCapacity - 1);
                    AtomicReference<Node> newBucket = buckets.get(newIndex);
                    
                    Node newHead = null;
                    Node curr = head;
                    while (curr != null) {
                        Node next = curr.next.getReference();
                        boolean marked = curr.next.isMarked();
                        
                        if (!marked) {
                            Node newNode = new Node(curr.key, curr.hash, newHead);
                            newHead = newNode;
                        }
                        
                        curr = next;
                    }
                    
                    if (newHead != null) {
                        newBucket.set(newHead);
                    }
                }
            }
            
            return true;
        }
    }
    
    private final AtomicReference<BucketList> table;
    private final AtomicInteger resizeInProgress;
    
    public ConcurrentDataStructure() {
        table = new AtomicReference<>(new BucketList(INITIAL_CAPACITY));
        resizeInProgress = new AtomicInteger(0);
    }
    
    private void helpResizeIfNeeded() {
        BucketList current = table.get();
        int currentSize = current.size.get();
        int currentThreshold = current.threshold.get();
        
        if (currentSize >= currentThreshold && 
            resizeInProgress.compareAndSet(0, 1)) {
            
            int newCapacity = current.buckets.length() << 1;
            if (newCapacity <= MAX_CAPACITY) {
                BucketList newList = new BucketList(newCapacity);
                
                if (newList.tryResize(current, newCapacity)) {
                    table.set(newList);
                }
            }
            
            resizeInProgress.set(0);
        }
    }
    
    private Node findNode(int key, int hash, AtomicReference<Node>[] predAndCurr) {
        BucketList currentTable = table.get();
        int index = hash & (currentTable.buckets.length() - 1);
        AtomicReference<Node> bucket = currentTable.buckets.get(index);
        
        Node pred = null;
        Node curr = bucket.get();
        
        while (curr != null) {
            Node succ = curr.next.getReference();
            boolean marked = curr.next.isMarked();
            
            if (marked) {
                if (!bucket.compareAndSet(curr, succ)) {
                    return findNode(key, hash, predAndCurr);
                }
                curr = succ;
                continue;
            }
            
            if (curr.hash == hash && curr.key == key) {
                predAndCurr[0] = bucket;
                predAndCurr[1] = new AtomicReference<>(pred);
                predAndCurr[2] = new AtomicReference<>(curr);
                return curr;
            }
            
            if (curr.hash > hash) {
                break;
            }
            
            pred = curr;
            curr = succ;
        }
        
        predAndCurr[0] = bucket;
        predAndCurr[1] = new AtomicReference<>(pred);
        predAndCurr[2] = new AtomicReference<>(curr);
        return null;
    }
    
    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int hash = key;
        
        while (true) {
            helpResizeIfNeeded();
            
            @SuppressWarnings("unchecked")
            AtomicReference<Node>[] predAndCurr = new AtomicReference[3];
            Node existing = findNode(key, hash, predAndCurr);
            
            if (existing != null) {
                return false;
            }
            
            AtomicReference<Node> bucket = predAndCurr[0];
            Node pred = predAndCurr[1].get();
            Node curr = predAndCurr[2].get();
            
            Node newNode = new Node(key, hash, curr);
            
            if (pred == null) {
                if (bucket.compareAndSet(curr, newNode)) {
                    BucketList currentTable = table.get();
                    currentTable.size.incrementAndGet();
                    return true;
                }
            } else {
                if (pred.next.compareAndSet(curr, newNode, false, false)) {
                    BucketList currentTable = table.get();
                    currentTable.size.incrementAndGet();
                    return true;
                }
            }
        }
    }
    
    @Override
    public boolean remove(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int hash = key;
        
        while (true) {
            helpResizeIfNeeded();
            
            @SuppressWarnings("unchecked")
            AtomicReference<Node>[] predAndCurr = new AtomicReference[3];
            Node target = findNode(key, hash, predAndCurr);
            
            if (target == null) {
                return false;
            }
            
            Node succ = target.next.getReference();
            
            if (target.next.compareAndSet(succ, succ, false, true)) {
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
                BucketList currentTable = table.get();
                currentTable.size.decrementAndGet();
                
                AtomicReference<Node> bucket = predAndCurr[0];
                Node pred = predAndCurr[1].get();
                
                if (pred == null) {
                    bucket.compareAndSet(target, succ);
                } else {
                    pred.next.compareAndSet(target, succ, false, false);
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
        int hash = key;
        BucketList currentTable = table.get();
        int index = hash & (currentTable.buckets.length() - 1);
        AtomicReference<Node> bucket = currentTable.buckets.get(index);
        
        Node curr = bucket.get();
        
        while (curr != null) {
            boolean marked = curr.next.isMarked();
            
            if (!marked && curr.hash == hash && curr.key == key) {
                return true;
            }
            
            if (curr.hash > hash) {
                break;
            }
            
            curr = curr.next.getReference();
        }
        
        return false;
    }
}