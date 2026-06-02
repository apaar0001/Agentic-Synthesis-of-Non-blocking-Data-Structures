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
        
        Node(int key, Node nextNode) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(nextNode, false);
        }
    }
    
    private final int capacity;
    private final Node[] buckets;
    
    public ConcurrentDataStructure() {
        this.capacity = DEFAULT_CAPACITY;
        this.buckets = new Node[capacity];
        for (int i = 0; i < capacity; i++) {
            buckets[i] = new Node(Integer.MIN_VALUE);
        }
    }
    
    private int computeIndex(int key) {
        return Math.abs(key) % capacity;
    }
    
    @Override
    public boolean add(int key) {
        int index = computeIndex(key);
        
        while (true) {
            Node pred = buckets[index];
            Node curr = pred.next.getReference();
            
            while (curr != null) {
                boolean[] marked = {false};
                Node succ = curr.next.get(marked);
                
                if (marked[0]) {
                    pred.next.compareAndSet(curr, succ, false, false);
                    break;
                }
                
                if (curr.key == key) {
                    return false;
                }
                
                pred = curr;
                curr = succ;
            }
            
            if (curr == null) {
                Node newNode = new Node(key);
                if (pred.next.compareAndSet(null, newNode, false, false)) {
                    return true;
                }
            }
        }
    }
    
    @Override
    public boolean remove(int key) {
        int index = computeIndex(key);
        
        while (true) {
            Node pred = buckets[index];
            Node curr = pred.next.getReference();
            
            while (curr != null) {
                boolean[] marked = {false};
                Node succ = curr.next.get(marked);
                
                if (marked[0]) {
                    pred.next.compareAndSet(curr, succ, false, false);
                    break;
                }
                
                if (curr.key == key) {
                    if (curr.next.compareAndSet(succ, succ, false, true)) {
                        // Node has been marked
                        pred.next.compareAndSet(curr, succ, false, false);
                        return true;
                    }
                    continue;
                }
                
                pred = curr;
                curr = succ;
            }
            
            return false;
        }
    }
    
    @Override
    public boolean contains(int key) {
        int index = computeIndex(key);
        Node curr = buckets[index].next.getReference();
        
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