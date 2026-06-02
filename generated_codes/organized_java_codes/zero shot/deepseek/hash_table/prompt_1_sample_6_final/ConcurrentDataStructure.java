package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static final int DEFAULT_CAPACITY = 16;
    
    private static class Node {
        final int key;
        final AtomicMarkableReference<Node> next;
        
        Node(int key) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, false);
        }
    }
    
    private final int capacity;
    private final AtomicMarkableReference<Node>[] buckets;
    
    @SuppressWarnings("unchecked")
    public ConcurrentDataStructure() {
        this.capacity = DEFAULT_CAPACITY;
        this.buckets = new AtomicMarkableReference[capacity];
        for (int i = 0; i < capacity; i++) {
            buckets[i] = new AtomicMarkableReference<>(null, false);
        }
    }
    
    private int computeIndex(int key) {
        return Math.abs(key) % capacity;
    }
    
    private boolean attemptUnlink(Node prev, Node curr, Node next) {
        if (prev == null) {
            return buckets[computeIndex(curr.key)].compareAndSet(curr, next, false, false);
        } else {
            return prev.next.compareAndSet(curr, next, false, false);
        }
    }
    
    @Override
    public boolean add(int key) {
        int index = computeIndex(key);
        
        while (true) {
            Node prev = null;
            Node curr = buckets[index].getReference();
            
            while (curr != null) {
                boolean[] marked = {false};
                Node next = curr.next.get(marked);
                
                if (marked[0]) {
                    if (!attemptUnlink(prev, curr, next)) {
                        break;
                    }
                    curr = next;
                    continue;
                }
                
                if (curr.key == key) {
                    return false;
                }
                
                prev = curr;
                curr = next;
            }
            
            Node newNode = new Node(key);
            newNode.next.set(curr, false);
            
            if (prev == null) {
                if (buckets[index].compareAndSet(curr, newNode, false, false)) {
                    return true;
                }
            } else {
                if (prev.next.compareAndSet(curr, newNode, false, false)) {
                    return true;
                }
            }
        }
    }
    
    @Override
    public boolean remove(int key) {
        int index = computeIndex(key);
        
        while (true) {
            Node prev = null;
            Node curr = buckets[index].getReference();
            
            while (curr != null) {
                boolean[] marked = {false};
                Node next = curr.next.get(marked);
                
                if (marked[0]) {
                    if (!attemptUnlink(prev, curr, next)) {
                        break;
                    }
                    curr = next;
                    continue;
                }
                
                if (curr.key == key) {
                    if (curr.next.compareAndSet(next, next, false, true)) {
                        // Node has been marked
                        attemptUnlink(prev, curr, next);
                        return true;
                    }
                    break;
                }
                
                prev = curr;
                curr = next;
            }
            
            return false;
        }
    }
    
    @Override
    public boolean contains(int key) {
        int index = computeIndex(key);
        
        Node curr = buckets[index].getReference();
        while (curr != null) {
            boolean[] marked = {false};
            Node next = curr.next.get(marked);
            
            if (!marked[0] && curr.key == key) {
                return true;
            }
            
            if (marked[0]) {
                attemptUnlink(null, curr, next);
            }
            
            curr = next;
        }
        
        return false;
    }
}