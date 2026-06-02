package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {
    private static final int DEFAULT_CAPACITY = 16;
    
    private static class Node {
        final int key;
        final AtomicReference<Node> next;
        final AtomicMarkableReference<Boolean> deleted;
        
        Node(int key) {
            this.key = key;
            this.next = new AtomicReference<>(null);
            this.deleted = new AtomicMarkableReference<>(false, false);
        }
    }
    
    private final int capacity;
    private final AtomicReference<Node>[] table;
    
    @SuppressWarnings("unchecked")
    public ConcurrentDataStructure() {
        this.capacity = DEFAULT_CAPACITY;
        this.table = new AtomicReference[capacity];
        for (int i = 0; i < capacity; i++) {
            table[i] = new AtomicReference<>(null);
        }
    }
    
    private int index(int key) {
        return Math.abs(key) % capacity;
    }
    
    private void cleanup(Node prev, Node curr) {
        Node next = curr.next.get();
        if (prev == null) {
            table[index(curr.key)].compareAndSet(curr, next);
        } else {
            prev.next.compareAndSet(curr, next);
        }
    }
    
    @Override
    public boolean add(int key) {
        int idx = index(key);
        
        while (true) {
            Node head = table[idx].get();
            Node curr = head;
            Node prev = null;
            
            while (curr != null) {
                if (curr.deleted.isMarked()) {
                    Node next = curr.next.get();
                    if (prev == null) {
                        table[idx].compareAndSet(curr, next);
                    } else {
                        prev.next.compareAndSet(curr, next);
                    }
                    curr = next;
                    continue;
                }
                
                if (curr.key == key) {
                    return false;
                }
                
                prev = curr;
                curr = curr.next.get();
            }
            
            Node newNode = new Node(key);
            newNode.next.set(head);
            
            if (prev == null) {
                if (table[idx].compareAndSet(head, newNode)) {
                    return true;
                }
            } else {
                if (prev.next.compareAndSet(null, newNode)) {
                    return true;
                }
            }
        }
    }
    
    @Override
    public boolean remove(int key) {
        int idx = index(key);
        
        while (true) {
            Node head = table[idx].get();
            Node curr = head;
            Node prev = null;
            
            while (curr != null) {
                if (curr.deleted.isMarked()) {
                    Node next = curr.next.get();
                    if (prev == null) {
                        table[idx].compareAndSet(curr, next);
                    } else {
                        prev.next.compareAndSet(curr, next);
                    }
                    curr = next;
                    continue;
                }
                
                if (curr.key == key) {
                    if (curr.deleted.compareAndSet(false, true, false, true)) {
                        // Node has been marked
                        cleanup(prev, curr);
                        return true;
                    }
                    continue;
                }
                
                prev = curr;
                curr = curr.next.get();
            }
            
            return false;
        }
    }
    
    @Override
    public boolean contains(int key) {
        int idx = index(key);
        Node curr = table[idx].get();
        
        while (curr != null) {
            if (!curr.deleted.isMarked() && curr.key == key) {
                return true;
            }
            curr = curr.next.get();
        }
        
        return false;
    }
}