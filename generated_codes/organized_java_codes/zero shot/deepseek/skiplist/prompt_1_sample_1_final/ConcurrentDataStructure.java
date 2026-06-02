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
        header = new Node(MAX_HEIGHT);
        currentHeight = new AtomicMarkableReference<>(1, false);
    }
    
    @Override
    public boolean add(int key) {
        int topLevel = randomHeight();
        
        while (true) {
            Node[] updates = new Node[MAX_HEIGHT];
            Node[] successors = new Node[MAX_HEIGHT];
            
            Node found = find(key, updates, successors);
            if (found != null) {
                boolean[] mark = new boolean[1];
                found.next[0].get(mark);
                if (!mark[0]) {
                    return false;
                }
                continue;
            }
            
            Node newNode = new Node(topLevel);
            newNode.key = key;
            
            for (int i = 0; i < topLevel; i++) {
                newNode.next[i].set(successors[i], false);
            }
            
            if (!updates[0].next[0].compareAndSet(successors[0], newNode, false, false)) {
                continue;
            }
            
            for (int i = 1; i < topLevel; i++) {
                while (true) {
                    if (updates[i].next[i].compareAndSet(successors[i], newNode, false, false)) {
                        break;
                    }
                    find(key, updates, successors);
                }
            }
            
            int oldHeight;
            boolean[] mark = new boolean[1];
            do {
                oldHeight = currentHeight.get(mark);
                if (topLevel <= oldHeight) {
                    break;
                }
            } while (!currentHeight.compareAndSet(oldHeight, topLevel, false, false));
            
            return true;
        }
    }
    
    @Override
    public boolean remove(int key) {
        while (true) {
            Node[] updates = new Node[MAX_HEIGHT];
            Node[] successors = new Node[MAX_HEIGHT];
            
            Node target = find(key, updates, successors);
            if (target == null) {
                return false;
            }
            
            Node next = target.next[0].getReference();
            if (!target.next[0].attemptMark(next, true)) {
                continue;
            }
            
            // Node has been marked
            
            if (updates[0].next[0].compareAndSet(target, next, false, false)) {
                for (int i = 1; i < target.height; i++) {
                    while (true) {
                        boolean[] mark = new boolean[1];
                        Node succ = target.next[i].get(mark);
                        if (mark[0]) {
                            break;
                        }
                        if (target.next[i].compareAndSet(succ, succ, false, true)) {
                            break;
                        }
                    }
                    updates[i].next[i].compareAndSet(target, next, false, false);
                }
            }
            
            return true;
        }
    }
    
    @Override
    public boolean contains(int key) {
        Node current = header;
        int height = currentHeight.getReference();
        
        for (int i = height - 1; i >= 0; i--) {
            while (true) {
                boolean[] mark = new boolean[1];
                Node next = current.next[i].get(mark);
                if (next == null) {
                    break;
                }
                if (mark[0]) {
                    Node succ = next.next[i].getReference();
                    current.next[i].compareAndSet(next, succ, false, false);
                    continue;
                }
                if (next.key < key) {
                    current = next;
                    continue;
                }
                break;
            }
        }
        
        boolean[] mark = new boolean[1];
        Node candidate = current.next[0].get(mark);
        return candidate != null && candidate.key == key && !mark[0];
    }
    
    private Node find(int key, Node[] updates, Node[] successors) {
        Node current = header;
        int height = currentHeight.getReference();
        boolean retry = false;
        
        for (int i = height - 1; i >= 0; i--) {
            while (true) {
                boolean[] mark = new boolean[1];
                Node next = current.next[i].get(mark);
                successors[i] = next;
                
                if (retry || mark[0]) {
                    Node succ = next != null ? next.next[i].getReference() : null;
                    if (!current.next[i].compareAndSet(next, succ, false, false)) {
                        retry = true;
                        current = header;
                        i = height - 1;
                        break;
                    }
                    next = succ;
                    successors[i] = next;
                }
                
                if (next == null || next.key >= key) {
                    updates[i] = current;
                    break;
                }
                
                current = next;
            }
        }
        
        Node candidate = successors[0];
        if (candidate != null && candidate.key == key) {
            boolean[] mark = new boolean[1];
            candidate.next[0].get(mark);
            if (!mark[0]) {
                return candidate;
            }
        }
        return null;
    }
    
    private int randomHeight() {
        int height = 1;
        while (ThreadLocalRandom.current().nextDouble() < PROBABILITY && height < MAX_HEIGHT) {
            height++;
        }
        return height;
    }
    
    private static class Node {
        final int height;
        int key;
        final AtomicMarkableReference<Node>[] next;
        
        @SuppressWarnings("unchecked")
        Node(int height) {
            this.height = height;
            next = new AtomicMarkableReference[height];
            for (int i = 0; i < height; i++) {
                next[i] = new AtomicMarkableReference<>(null, false);
            }
        }
    }
}