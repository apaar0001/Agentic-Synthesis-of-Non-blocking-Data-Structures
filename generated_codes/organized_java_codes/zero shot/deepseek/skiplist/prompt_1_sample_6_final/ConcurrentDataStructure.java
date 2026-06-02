package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.ThreadLocalRandom;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static final int MAX_HEIGHT = 16;
    private static final double PROBABILITY = 0.5;
    
    private final Node header;
    private final AtomicMarkableReference<Integer> currentHeight;
    
    public ConcurrentDataStructure() {
        this.header = new Node(MAX_HEIGHT);
        this.currentHeight = new AtomicMarkableReference<>(1, false);
    }
    
    @Override
    public boolean add(int key) {
        while (true) {
            Node[] update = new Node[MAX_HEIGHT];
            Node current = header;
            Node next;
            
            int height = currentHeight.getReference();
            
            for (int i = height - 1; i >= 0; i--) {
                while (true) {
                    next = current.forward[i].getReference();
                    if (next == null) break;
                    
                    while (next.marked.get()) {
                        Node succ = next.forward[i].getReference();
                        if (current.forward[i].compareAndSet(next, succ, false, false)) {
                            next = succ;
                            if (next == null) break;
                        } else {
                            break;
                        }
                    }
                    
                    if (next == null) break;
                    
                    if (next.key < key) {
                        current = next;
                    } else {
                        break;
                    }
                }
                update[i] = current;
            }
            
            next = current.forward[0].getReference();
            
            if (next != null && next.key == key) {
                if (!next.marked.get()) {
                    return false;
                }
                continue;
            }
            
            int newHeight = randomHeight();
            
            if (newHeight > height) {
                for (int i = height; i < newHeight; i++) {
                    update[i] = header;
                }
                while (true) {
                    int curHeight = currentHeight.getReference();
                    if (curHeight >= newHeight) break;
                    if (currentHeight.compareAndSet(curHeight, newHeight, false, false)) {
                        height = newHeight;
                        break;
                    }
                }
            }
            
            Node newNode = new Node(newHeight);
            newNode.key = key;
            
            for (int i = 0; i < newHeight; i++) {
                newNode.forward[i].set(update[i].forward[i].getReference(), false);
            }
            
            for (int i = 0; i < newHeight; i++) {
                while (true) {
                    Node expected = update[i].forward[i].getReference();
                    if (update[i].forward[i].compareAndSet(expected, newNode, false, false)) {
                        break;
                    }
                    
                    current = update[i];
                    for (int j = i; j >= 0; j--) {
                        while (true) {
                            next = current.forward[j].getReference();
                            if (next == null) break;
                            
                            while (next.marked.get()) {
                                Node succ = next.forward[j].getReference();
                                if (current.forward[j].compareAndSet(next, succ, false, false)) {
                                    next = succ;
                                    if (next == null) break;
                                } else {
                                    break;
                                }
                            }
                            
                            if (next == null) break;
                            
                            if (next.key < key) {
                                current = next;
                            } else {
                                break;
                            }
                        }
                        update[j] = current;
                    }
                    
                    if (i == 0) {
                        next = current.forward[0].getReference();
                        if (next != null && next.key == key && !next.marked.get()) {
                            return false;
                        }
                    }
                }
            }
            
            return true;
        }
    }
    
    @Override
    public boolean remove(int key) {
        while (true) {
            Node[] update = new Node[MAX_HEIGHT];
            Node current = header;
            Node next;
            
            int height = currentHeight.getReference();
            
            for (int i = height - 1; i >= 0; i--) {
                while (true) {
                    next = current.forward[i].getReference();
                    if (next == null) break;
                    
                    while (next.marked.get()) {
                        Node succ = next.forward[i].getReference();
                        if (current.forward[i].compareAndSet(next, succ, false, false)) {
                            next = succ;
                            if (next == null) break;
                        } else {
                            break;
                        }
                    }
                    
                    if (next == null) break;
                    
                    if (next.key < key) {
                        current = next;
                    } else {
                        break;
                    }
                }
                update[i] = current;
            }
            
            next = current.forward[0].getReference();
            
            if (next == null || next.key != key) {
                return false;
            }
            
            if (next.marked.get()) {
                return false;
            }
            
            if (next.marked.compareAndSet(false, true)) {
                // Node has been marked
                
                for (int i = 0; i < next.forward.length; i++) {
                    while (true) {
                        Node expected = update[i].forward[i].getReference();
                        if (update[i].forward[i].compareAndSet(next, next.forward[i].getReference(), false, false)) {
                            break;
                        }
                        
                        current = update[i];
                        for (int j = i; j >= 0; j--) {
                            while (true) {
                                Node n = current.forward[j].getReference();
                                if (n == null) break;
                                
                                while (n.marked.get()) {
                                    Node succ = n.forward[j].getReference();
                                    if (current.forward[j].compareAndSet(n, succ, false, false)) {
                                        n = succ;
                                        if (n == null) break;
                                    } else {
                                        break;
                                    }
                                }
                                
                                if (n == null) break;
                                
                                if (n.key < key) {
                                    current = n;
                                } else {
                                    break;
                                }
                            }
                            update[j] = current;
                        }
                        
                        if (update[i].forward[i].getReference() != next) {
                            break;
                        }
                    }
                }
                
                while (true) {
                    int curHeight = currentHeight.getReference();
                    if (curHeight <= 1 || header.forward[curHeight - 1].getReference() != null) {
                        break;
                    }
                    if (currentHeight.compareAndSet(curHeight, curHeight - 1, false, false)) {
                        continue;
                    }
                    break;
                }
                
                return true;
            }
        }
    }
    
    @Override
    public boolean contains(int key) {
        Node current = header;
        Node next;
        
        int height = currentHeight.getReference();
        
        for (int i = height - 1; i >= 0; i--) {
            while (true) {
                next = current.forward[i].getReference();
                if (next == null) break;
                
                while (next.marked.get()) {
                    Node succ = next.forward[i].getReference();
                    if (current.forward[i].compareAndSet(next, succ, false, false)) {
                        next = succ;
                        if (next == null) break;
                    } else {
                        break;
                    }
                }
                
                if (next == null) break;
                
                if (next.key < key) {
                    current = next;
                } else {
                    break;
                }
            }
        }
        
        next = current.forward[0].getReference();
        
        return next != null && next.key == key && !next.marked.get();
    }
    
    private int randomHeight() {
        int height = 1;
        while (ThreadLocalRandom.current().nextDouble() < PROBABILITY && height < MAX_HEIGHT) {
            height++;
        }
        return height;
    }
    
    private static class Node {
        int key;
        AtomicMarkableReference<Node>[] forward;
        AtomicMarkableReference<Boolean> marked;
        
        @SuppressWarnings("unchecked")
        Node(int height) {
            this.forward = (AtomicMarkableReference<Node>[]) new AtomicMarkableReference[height];
            for (int i = 0; i < height; i++) {
                this.forward[i] = new AtomicMarkableReference<>(null, false);
            }
            this.marked = new AtomicMarkableReference<>(false, false);
        }
    }
}