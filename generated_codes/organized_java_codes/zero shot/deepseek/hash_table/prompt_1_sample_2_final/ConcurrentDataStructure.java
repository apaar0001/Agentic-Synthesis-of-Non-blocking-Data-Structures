package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;

class Node {
    int key;
    AtomicMarkableReference<Node> next;
    
    Node(int key) {
        this.key = key;
        this.next = new AtomicMarkableReference<>(null, false);
    }
}

public class ConcurrentDataStructure implements SetADT {
    private static final int DEFAULT_CAPACITY = 16;
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
        Node head = buckets[index];
        
        while (true) {
            Node pred = head;
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
                
                if (curr.key == key) {
                    return false;
                }
                
                if (curr.key > key) {
                    break;
                }
                
                pred = curr;
                curr = succ;
            }
            
            Node newNode = new Node(key);
            newNode.next.set(curr, false);
            
            if (pred.next.compareAndSet(curr, newNode, false, false)) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int index = computeIndex(key);
        Node head = buckets[index];
        
        while (true) {
            Node pred = head;
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
                
                if (curr.key == key) {
                    if (curr.next.compareAndSet(succ, succ, false, true)) {
                        // Node has been marked
                        pred.next.compareAndSet(curr, succ, false, false);
                        return true;
                    }
                    continue;
                }
                
                if (curr.key > key) {
                    return false;
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
            
            if (curr.key > key) {
                return false;
            }
            
            curr = curr.next.getReference();
        }
        return false;
    }
}