package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
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
    
    private static class Bucket {
        final AtomicReference<Node> head;
        
        Bucket(Node node) {
            this.head = new AtomicReference<>(node);
        }
    }
    
    private final AtomicReference<Bucket[]> buckets;
    private final AtomicInteger size;
    
    public ConcurrentDataStructure() {
        Bucket[] initialBuckets = new Bucket[INITIAL_CAPACITY];
        for (int i = 0; i < INITIAL_CAPACITY; i++) {
            initialBuckets[i] = new Bucket(new Node(-1, i, null));
        }
        this.buckets = new AtomicReference<>(initialBuckets);
        this.size = new AtomicInteger(0);
    }
    
    private static int spread(int h) {
        return (h ^ (h >>> 16)) & HASH_BITS;
    }
    
    private Bucket[] getBuckets() {
        return buckets.get();
    }
    
    private int bucketIndex(int hash, int capacity) {
        return hash & (capacity - 1);
    }
    
    private boolean casBuckets(Bucket[] expected, Bucket[] update) {
        return buckets.compareAndSet(expected, update);
    }
    
    @Override
    public boolean add(int key) {
        int hash = spread(key);
        while (true) {
            Bucket[] currentBuckets = getBuckets();
            int capacity = currentBuckets.length;
            int index = bucketIndex(hash, capacity);
            Bucket bucket = currentBuckets[index];
            Node head = bucket.head.get();
            
            Node pred = head;
            Node curr = head.next.getReference();
            boolean[] marked = {false};
            
            while (curr != null) {
                Node succ = curr.next.get(marked);
                if (marked[0]) {
                    Node next = curr.next.getReference();
                    if (!pred.next.compareAndSet(curr, next, false, false)) {
                        break;
                    }
                    curr = next;
                } else if (curr.key == key) {
                    return false;
                } else if (curr.hash > hash) {
                    break;
                } else {
                    pred = curr;
                    curr = succ;
                }
            }
            
            Node newNode = new Node(key, hash, curr);
            if (pred.next.compareAndSet(curr, newNode, false, false)) {
                size.incrementAndGet();
                tryResize();
                return true;
            }
        }
    }
    
    @Override
    public boolean remove(int key) {
        int hash = spread(key);
        while (true) {
            Bucket[] currentBuckets = getBuckets();
            int capacity = currentBuckets.length;
            int index = bucketIndex(hash, capacity);
            Bucket bucket = currentBuckets[index];
            Node head = bucket.head.get();
            
            Node pred = head;
            Node curr = head.next.getReference();
            boolean[] marked = {false};
            
            while (curr != null) {
                Node succ = curr.next.get(marked);
                if (marked[0]) {
                    Node next = curr.next.getReference();
                    if (!pred.next.compareAndSet(curr, next, false, false)) {
                        break;
                    }
                    curr = next;
                } else if (curr.key == key) {
                    if (curr.next.compareAndSet(succ, succ, false, true)) {
                        // Node has been marked
                        pred.next.compareAndSet(curr, succ, false, false);
                        size.decrementAndGet();
                        return true;
                    }
                    break;
                } else if (curr.hash > hash) {
                    return false;
                } else {
                    pred = curr;
                    curr = succ;
                }
            }
            return false;
        }
    }
    
    @Override
    public boolean contains(int key) {
        int hash = spread(key);
        Bucket[] currentBuckets = getBuckets();
        int capacity = currentBuckets.length;
        int index = bucketIndex(hash, capacity);
        Bucket bucket = currentBuckets[index];
        Node head = bucket.head.get();
        
        Node curr = head.next.getReference();
        boolean[] marked = {false};
        
        while (curr != null) {
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
    
    private void tryResize() {
        int currentSize = size.get();
        Bucket[] currentBuckets = getBuckets();
        int capacity = currentBuckets.length;
        
        if (currentSize < capacity * 2 || capacity >= MAX_CAPACITY) {
            return;
        }
        
        Bucket[] newBuckets = new Bucket[capacity * 2];
        for (int i = 0; i < newBuckets.length; i++) {
            newBuckets[i] = new Bucket(new Node(-1, i, null));
        }
        
        if (casBuckets(currentBuckets, newBuckets)) {
            for (int i = 0; i < capacity; i++) {
                migrateBucket(currentBuckets[i], newBuckets);
            }
        }
    }
    
    private void migrateBucket(Bucket oldBucket, Bucket[] newBuckets) {
        Node head = oldBucket.head.get();
        Node curr = head.next.getReference();
        
        while (curr != null) {
            boolean[] marked = {false};
            Node succ = curr.next.get(marked);
            if (!marked[0]) {
                int newIndex = bucketIndex(curr.hash, newBuckets.length);
                Bucket newBucket = newBuckets[newIndex];
                Node newHead = newBucket.head.get();
                
                Node newPred = newHead;
                Node newCurr = newHead.next.getReference();
                
                while (newCurr != null && newCurr.hash < curr.hash) {
                    newPred = newCurr;
                    newCurr = newCurr.next.getReference();
                }
                
                Node newNode = new Node(curr.key, curr.hash, newCurr);
                if (newPred.next.compareAndSet(newCurr, newNode, false, false)) {
                    curr.next.compareAndSet(succ, succ, false, true);
                }
            }
            curr = succ;
        }
    }
}