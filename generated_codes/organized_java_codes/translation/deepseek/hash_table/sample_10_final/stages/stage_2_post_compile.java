package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReferenceArray;

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
    
    private final AtomicReferenceArray<Node> table;
    private final AtomicInteger sizeCtl = new AtomicInteger(0);
    
    public ConcurrentDataStructure() {
        this.table = new AtomicReferenceArray<>(INITIAL_CAPACITY);
    }
    
    private static int spread(int h) {
        return (h ^ (h >>> 16)) & HASH_BITS;
    }
    
    private Node findBucket(int hash) {
        int n = table.length();
        int idx = (n - 1) & hash;
        Node bucket = table.get(idx);
        if (bucket == null) {
            Node newBucket = new Node(-1, hash, null);
            if (table.compareAndSet(idx, null, newBucket)) {
                return newBucket;
            }
        }
        return table.get(idx);
    }
    
    private boolean casBucket(int idx, Node expected, Node update) {
        return table.compareAndSet(idx, expected, update);
    }
    
    @Override
    public boolean add(int key) {
        int hash = spread(key);
        while (true) {
            Node bucket = findBucket(hash);
            Node pred = bucket;
            Node curr = pred.next.getReference();
            
            while (curr != null) {
                Node succ = curr.next.getReference();
                boolean[] marked = {false};
                curr.next.get(marked);
                
                if (marked[0]) {
                    if (!pred.next.compareAndSet(curr, succ, false, false)) {
                        break;
                    }
                    curr = succ;
                    continue;
                }
                
                if (curr.hash > hash || (curr.hash == hash && curr.key >= key)) {
                    break;
                }
                pred = curr;
                curr = succ;
            }
            
            if (curr != null && curr.hash == hash && curr.key == key) {
                boolean[] marked = {false};
                curr.next.get(marked);
                return marked[0];
            }
            
            Node newNode = new Node(key, hash, curr);
            if (pred.next.compareAndSet(curr, newNode, false, false)) {
                incrementSize();
                return true;
            }
        }
    }
    
    @Override
    public boolean remove(int key) {
        int hash = spread(key);
        while (true) {
            Node bucket = findBucket(hash);
            Node pred = bucket;
            Node curr = pred.next.getReference();
            
            while (curr != null) {
                Node succ = curr.next.getReference();
                boolean[] marked = {false};
                curr.next.get(marked);
                
                if (marked[0]) {
                    if (!pred.next.compareAndSet(curr, succ, false, false)) {
                        break;
                    }
                    curr = succ;
                    continue;
                }
                
                if (curr.hash > hash || (curr.hash == hash && curr.key > key)) {
                    return false;
                }
                
                if (curr.hash == hash && curr.key == key) {
                    if (curr.next.compareAndSet(succ, succ, false, true)) {
                        // Node has been marked
                        pred.next.compareAndSet(curr, succ, false, false);
                        decrementSize();
                        return true;
                    }
                    break;
                }
                pred = curr;
                curr = succ;
            }
            return false;
        }
    }
    
    @Override
    public boolean contains(int key) {
        int hash = spread(key);
        Node bucket = findBucket(hash);
        Node curr = bucket.next.getReference();
        
        while (curr != null) {
            boolean[] marked = {false};
            curr.next.get(marked);
            
            if (!marked[0]) {
                if (curr.hash > hash || (curr.hash == hash && curr.key > key)) {
                    break;
                }
                if (curr.hash == hash && curr.key == key) {
                    return true;
                }
            }
            curr = curr.next.getReference();
        }
        return false;
    }
    
    private void incrementSize() {
        int c;
        do {
            c = sizeCtl.get();
        } while (!sizeCtl.compareAndSet(c, c + 1));
        tryResize();
    }
    
    private void decrementSize() {
        int c;
        do {
            c = sizeCtl.get();
        } while (!sizeCtl.compareAndSet(c, c - 1));
    }
    
    private void tryResize() {
        int n = table.length();
        if (n >= MAX_CAPACITY) return;
        
        int sc = sizeCtl.get();
        if (sc <= n * 3 / 4) return;
        
        if (!sizeCtl.compareAndSet(sc, -1)) return;
        
        try {
            if (table.length() != n) return;
            
            int newCapacity = n << 1;
            if (newCapacity > MAX_CAPACITY) newCapacity = MAX_CAPACITY;
            
            AtomicReferenceArray<Node> newTable = new AtomicReferenceArray<>(newCapacity);
            
            for (int i = 0; i < n; i++) {
                Node bucket = table.get(i);
                if (bucket != null) {
                    Node newBucket = new Node(-1, i, null);
                    newTable.set(i, newBucket);
                    
                    Node curr = bucket.next.getReference();
                    while (curr != null) {
                        boolean[] marked = {false};
                        curr.next.get(marked);
                        if (!marked[0]) {
                            int newIdx = (newCapacity - 1) & curr.hash;
                            Node targetBucket = newTable.get(newIdx);
                            if (targetBucket == null) {
                                targetBucket = new Node(-1, curr.hash, null);
                                newTable.set(newIdx, targetBucket);
                            }
                            
                            Node newNode = new Node(curr.key, curr.hash, targetBucket.next.getReference());
                            while (!targetBucket.next.compareAndSet(targetBucket.next.getReference(), newNode, false, false)) {
                                newNode = new Node(curr.key, curr.hash, targetBucket.next.getReference());
                            }
                        }
                        curr = curr.next.getReference();
                    }
                }
            }
            
            for (int i = 0; i < newCapacity; i++) {
                if (newTable.get(i) == null) {
                    newTable.set(i, new Node(-1, i, null));
                }
            }
            
            for (int i = 0; i < n; i++) {
                table.set(i, newTable.get(i));
            }
            
            for (int i = n; i < newCapacity; i++) {
                table.set(i, newTable.get(i));
            }
            
        } finally {
            sizeCtl.set(newCapacity * 3 / 4);
        }
    }
}