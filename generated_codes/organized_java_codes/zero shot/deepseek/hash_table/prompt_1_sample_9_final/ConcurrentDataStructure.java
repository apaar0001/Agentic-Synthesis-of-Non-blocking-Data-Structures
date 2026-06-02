package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;

class ConcurrentNode {
    final int key;
    final AtomicMarkableReference<ConcurrentNode> next;

    ConcurrentNode(int key, ConcurrentNode next) {
        this.key = key;
        this.next = new AtomicMarkableReference<>(next, false);
    }
}

public class ConcurrentDataStructure implements SetADT {
    private static final int DEFAULT_CAPACITY = 16;
    private final int capacity;
    private final ConcurrentNode[] buckets;

    public ConcurrentDataStructure() {
        this.capacity = DEFAULT_CAPACITY;
        this.buckets = new ConcurrentNode[capacity];
        for (int i = 0; i < capacity; i++) {
            buckets[i] = new ConcurrentNode(Integer.MIN_VALUE, null);
        }
    }

    private int computeIndex(int key) {
        return Math.abs(key) % capacity;
    }

    @Override
    public boolean add(int key) {
        int index = computeIndex(key);
        ConcurrentNode head = buckets[index];

        while (true) {
            ConcurrentNode pred = head;
            ConcurrentNode curr = pred.next.getReference();
            
            while (curr != null) {
                boolean[] marked = {false};
                ConcurrentNode succ = curr.next.get(marked);
                
                if (marked[0]) {
                    ConcurrentNode succ2 = curr.next.getReference();
                    if (!pred.next.compareAndSet(curr, succ2, false, false)) {
                        break;
                    }
                    curr = succ2;
                } else {
                    if (curr.key == key) {
                        return false;
                    }
                    pred = curr;
                    curr = succ;
                }
            }
            
            ConcurrentNode newNode = new ConcurrentNode(key, curr);
            if (pred.next.compareAndSet(curr, newNode, false, false)) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int index = computeIndex(key);
        ConcurrentNode head = buckets[index];

        while (true) {
            ConcurrentNode pred = head;
            ConcurrentNode curr = pred.next.getReference();
            
            while (curr != null) {
                boolean[] marked = {false};
                ConcurrentNode succ = curr.next.get(marked);
                
                if (marked[0]) {
                    ConcurrentNode succ2 = curr.next.getReference();
                    if (!pred.next.compareAndSet(curr, succ2, false, false)) {
                        break;
                    }
                    curr = succ2;
                } else {
                    if (curr.key == key) {
                        if (curr.next.attemptMark(succ, true)) {
                            // Node has been marked
                            pred.next.compareAndSet(curr, succ, false, false);
                            return true;
                        }
                        break;
                    }
                    pred = curr;
                    curr = succ;
                }
            }
            return false;
        }
    }

    @Override
    public boolean contains(int key) {
        int index = computeIndex(key);
        ConcurrentNode head = buckets[index];
        ConcurrentNode curr = head.next.getReference();
        
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