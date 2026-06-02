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
    private final AtomicInteger size = new AtomicInteger(0);
    private final AtomicInteger threshold = new AtomicInteger(INITIAL_CAPACITY);
    
    public ConcurrentDataStructure() {
        table = new AtomicReferenceArray<>(INITIAL_CAPACITY);
        for (int i = 0; i < INITIAL_CAPACITY; i++) {
            table.set(i, new Node(-1, i, null));
        }
    }
    
    private static int spread(int h) {
        return (h ^ (h >>> 16)) & HASH_BITS;
    }
    
    private Node getBucket(int hash) {
        int idx = (table.length() - 1) & hash;
        return table.get(idx);
    }
    
    private boolean casBucket(int hash, Node oldBucket, Node newBucket) {
        int idx = (table.length() - 1) & hash;
        return table.compareAndSet(idx, oldBucket, newBucket);
    }
    
    private void helpDelete(Node pred, Node curr) {
        boolean[] marked = {false};
        Node succ = curr.next.get(marked);
        while (marked[0]) {
            Node nextSucc = succ != null ? succ.next.getReference() : null;
            if (!pred.next.compareAndSet(curr, succ, false, false)) {
                break;
            }
            curr = succ;
            succ = nextSucc;
            if (succ != null) {
                succ.next.get(marked);
            }
        }
    }
    
    private boolean findAndRemove(Node bucket, int key, int hash) {
        Node pred = bucket;
        Node curr = pred.next.getReference();
        while (curr != null) {
            boolean[] marked = {false};
            Node succ = curr.next.get(marked);
            
            while (marked[0]) {
                if (!pred.next.compareAndSet(curr, succ, false, false)) {
                    return findAndRemove(bucket, key, hash);
                }
                curr = succ;
                if (curr == null) {
                    succ = null;
                    break;
                }
                succ = curr.next.get(marked);
            }
            
            if (curr != null && curr.hash == hash && curr.key == key) {
                if (curr.next.compareAndSet(succ, succ, false, true)) {
                    // Node has been marked
                    helpDelete(pred, curr);
                    size.decrementAndGet();
                    return true;
                }
                return findAndRemove(bucket, key, hash);
            }
            
            pred = curr;
            curr = succ;
        }
        return false;
    }
    
    private boolean findAndAdd(Node bucket, int key, int hash) {
        Node pred = bucket;
        Node curr = pred.next.getReference();
        while (curr != null) {
            boolean[] marked = {false};
            Node succ = curr.next.get(marked);
            
            while (marked[0]) {
                if (!pred.next.compareAndSet(curr, succ, false, false)) {
                    return findAndAdd(bucket, key, hash);
                }
                curr = succ;
                if (curr == null) {
                    succ = null;
                    break;
                }
                succ = curr.next.get(marked);
            }
            
            if (curr != null && curr.hash == hash && curr.key == key) {
                return false;
            }
            
            if (curr != null && curr.hash > hash) {
                break;
            }
            
            pred = curr;
            curr = succ;
        }
        
        Node newNode = new Node(key, hash, curr);
        if (pred.next.compareAndSet(curr, newNode, false, false)) {
            size.incrementAndGet();
            maybeResize();
            return true;
        }
        return findAndAdd(bucket, key, hash);
    }
    
    private void maybeResize() {
        int sz = size.get();
        int th = threshold.get();
        if (sz < th || table.length() >= MAX_CAPACITY) {
            return;
        }
        
        int newCap = table.length() << 1;
        if (newCap > MAX_CAPACITY) {
            newCap = MAX_CAPACITY;
        }
        
        if (threshold.compareAndSet(th, newCap)) {
            AtomicReferenceArray<Node> newTable = new AtomicReferenceArray<>(newCap);
            for (int i = 0; i < newCap; i++) {
                newTable.set(i, new Node(-1, i, null));
            }
            
            for (int i = 0; i < table.length(); i++) {
                Node oldBucket = table.get(i);
                Node newBucket = newTable.get(i & (newCap - 1));
                
                Node curr = oldBucket.next.getReference();
                while (curr != null) {
                    boolean[] marked = {false};
                    Node succ = curr.next.get(marked);
                    if (!marked[0]) {
                        int newIdx = curr.hash & (newCap - 1);
                        Node targetBucket = newTable.get(newIdx);
                        
                        Node pred = targetBucket;
                        Node targetCurr = pred.next.getReference();
                        while (targetCurr != null && targetCurr.hash < curr.hash) {
                            pred = targetCurr;
                            targetCurr = targetCurr.next.getReference();
                        }
                        
                        Node newNode = new Node(curr.key, curr.hash, targetCurr);
                        pred.next.compareAndSet(targetCurr, newNode, false, false);
                    }
                    curr = succ;
                }
            }
            
            for (int i = 0; i < newCap; i++) {
                table.set(i, newTable.get(i));
            }
        }
    }
    
    @Override
    public boolean add(int key) {
        int hash = spread(key);
        while (true) {
            Node bucket = getBucket(hash);
            if (findAndAdd(bucket, key, hash)) {
                return true;
            }
        }
    }
    
    @Override
    public boolean remove(int key) {
        int hash = spread(key);
        while (true) {
            Node bucket = getBucket(hash);
            if (findAndRemove(bucket, key, hash)) {
                return true;
            }
            if (!contains(key)) {
                return false;
            }
        }
    }
    
    @Override
    public boolean contains(int key) {
        int hash = spread(key);
        Node bucket = getBucket(hash);
        Node curr = bucket.next.getReference();
        
        while (curr != null) {
            boolean[] marked = {false};
            Node succ = curr.next.get(marked);
            
            if (marked[0]) {
                curr = succ;
                continue;
            }
            
            if (curr.hash == hash && curr.key == key) {
                return true;
            }
            
            if (curr.hash > hash) {
                break;
            }
            
            curr = succ;
        }
        return false;
    }
}