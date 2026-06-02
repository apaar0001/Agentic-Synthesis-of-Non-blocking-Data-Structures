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
    
    @Override
    public boolean add(int key) {
        int index = computeIndex(key);
        
        while (true) {
            Node head = buckets[index].getReference();
            Node curr = head;
            Node prev = null;
            
            while (curr != null) {
                boolean[] marked = {false};
                Node succ = curr.next.get(marked);
                
                if (marked[0]) {
                    if (prev != null) {
                        if (!prev.next.compareAndSet(curr, succ, false, false)) {
                            break;
                        }
                    } else {
                        if (!buckets[index].compareAndSet(curr, succ, false, false)) {
                            break;
                        }
                    }
                    curr = succ;
                    continue;
                }
                
                if (curr.key == key) {
                    return false;
                }
                
                prev = curr;
                curr = succ;
            }
            
            Node newNode = new Node(key);
            newNode.next.set(head, false);
            
            if (prev == null) {
                if (buckets[index].compareAndSet(head, newNode, false, false)) {
                    return true;
                }
            } else {
                if (prev.next.compareAndSet(null, newNode, false, false)) {
                    return true;
                }
            }
        }
    }
    
    @Override
    public boolean remove(int key) {
        int index = computeIndex(key);
        
        while (true) {
            Node head = buckets[index].getReference();
            Node curr = head;
            Node prev = null;
            
            while (curr != null) {
                boolean[] marked = {false};
                Node succ = curr.next.get(marked);
                
                if (marked[0]) {
                    if (prev != null) {
                        if (!prev.next.compareAndSet(curr, succ, false, false)) {
                            break;
                        }
                    } else {
                        if (!buckets[index].compareAndSet(curr, succ, false, false)) {
                            break;
                        }
                    }
                    curr = succ;
                    continue;
                }
                
                if (curr.key == key) {
                    if (curr.next.attemptMark(succ, true)) {
                        // Node has been marked
                        return true;
                    }
                    break;
                }
                
                prev = curr;
                curr = succ;
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
            curr.next.get(marked);
            
            if (!marked[0] && curr.key == key) {
                return true;
            }
            
            curr = curr.next.getReference();
        }
        
        return false;
    }
}