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
        
        retry: while (true) {
            Node head = buckets[index].getReference();
            Node curr = head;
            Node prev = null;
            
            while (curr != null) {
                boolean[] marked = {false};
                Node next = curr.next.get(marked);
                
                if (marked[0]) {
                    Node succ = next;
                    if (prev == null) {
                        if (buckets[index].compareAndSet(curr, succ, false, false)) {
                            curr = succ;
                            continue;
                        } else {
                            continue retry;
                        }
                    } else {
                        if (prev.next.compareAndSet(curr, succ, false, false)) {
                            curr = succ;
                            continue;
                        } else {
                            continue retry;
                        }
                    }
                }
                
                if (curr.key == key) {
                    return false;
                }
                
                prev = curr;
                curr = next;
            }
            
            Node newNode = new Node(key);
            newNode.next.set(head, false);
            
            if (buckets[index].compareAndSet(head, newNode, false, false)) {
                return true;
            }
        }
    }
    
    @Override
    public boolean remove(int key) {
        int index = computeIndex(key);
        
        retry: while (true) {
            Node head = buckets[index].getReference();
            Node curr = head;
            Node prev = null;
            
            while (curr != null) {
                boolean[] marked = {false};
                Node next = curr.next.get(marked);
                
                if (marked[0]) {
                    Node succ = next;
                    if (prev == null) {
                        if (buckets[index].compareAndSet(curr, succ, false, false)) {
                            curr = succ;
                            continue;
                        } else {
                            continue retry;
                        }
                    } else {
                        if (prev.next.compareAndSet(curr, succ, false, false)) {
                            curr = succ;
                            continue;
                        } else {
                            continue retry;
                        }
                    }
                }
                
                if (curr.key == key) {
                    if (curr.next.compareAndSet(next, next, false, true)) {
                        // Node has been marked
                        Node succ = next;
                        if (prev == null) {
                            buckets[index].compareAndSet(curr, succ, false, false);
                        } else {
                            prev.next.compareAndSet(curr, succ, false, false);
                        }
                        return true;
                    } else {
                        continue retry;
                    }
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
                Node succ = next;
                buckets[index].compareAndSet(curr, succ, false, false);
            }
            
            curr = next;
        }
        
        return false;
    }
}