package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.ThreadLocalRandom;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static final int MAX_LEVEL = 16;
    private static final double P = 0.5;
    
    private final Node header;
    private int level;
    
    public ConcurrentDataStructure() {
        header = new Node(MAX_LEVEL);
        level = 1;
    }
    
    @Override
    public boolean add(int key) {
        int topLevel = randomLevel();
        Node[] preds = new Node[MAX_LEVEL + 1];
        Node[] succs = new Node[MAX_LEVEL + 1];
        
        while (true) {
            Node pred = header;
            for (int i = level; i >= 1; i--) {
                Node curr = pred.forward[i].getReference();
                while (curr != null && curr.key < key) {
                    pred = curr;
                    curr = pred.forward[i].getReference();
                }
                preds[i] = pred;
                succs[i] = curr;
            }
            
            Node curr = succs[1];
            if (curr != null && curr.key == key) {
                boolean[] marked = {false};
                curr.forward[1].get(marked);
                if (marked[0]) {
                    continue;
                }
                return false;
            }
            
            Node newNode = new Node(topLevel);
            newNode.key = key;
            
            for (int i = 1; i <= topLevel; i++) {
                newNode.forward[i].set(succs[i], false);
            }
            
            if (!preds[1].forward[1].compareAndSet(succs[1], newNode, false, false)) {
                continue;
            }
            
            for (int i = 2; i <= topLevel; i++) {
                while (true) {
                    pred = preds[i];
                    curr = succs[i];
                    if (pred.forward[i].compareAndSet(curr, newNode, false, false)) {
                        break;
                    }
                    
                    pred = header;
                    for (int j = level; j >= 1; j--) {
                        curr = pred.forward[j].getReference();
                        while (curr != null && curr.key < key) {
                            pred = curr;
                            curr = pred.forward[j].getReference();
                        }
                        preds[j] = pred;
                        succs[j] = curr;
                    }
                    if (succs[i] != curr) {
                        break;
                    }
                }
            }
            
            while (topLevel > level) {
                level = topLevel;
            }
            
            return true;
        }
    }
    
    @Override
    public boolean remove(int key) {
        Node[] preds = new Node[MAX_LEVEL + 1];
        Node[] succs = new Node[MAX_LEVEL + 1];
        
        while (true) {
            Node pred = header;
            for (int i = level; i >= 1; i--) {
                Node curr = pred.forward[i].getReference();
                while (curr != null && curr.key < key) {
                    pred = curr;
                    curr = pred.forward[i].getReference();
                }
                preds[i] = pred;
                succs[i] = curr;
            }
            
            Node curr = succs[1];
            if (curr == null || curr.key != key) {
                return false;
            }
            
            boolean[] marked = {false};
            Node next = curr.forward[1].get(marked);
            if (marked[0]) {
                return false;
            }
            
            if (!curr.forward[1].compareAndSet(next, next, false, true)) {
                continue;
            }
            
            // Node has been marked
            return true;
        }
    }
    
    @Override
    public boolean contains(int key) {
        Node curr = header;
        for (int i = level; i >= 1; i--) {
            Node next = curr.forward[i].getReference();
            while (next != null && next.key < key) {
                curr = next;
                next = curr.forward[i].getReference();
            }
        }
        
        curr = curr.forward[1].getReference();
        if (curr == null || curr.key != key) {
            return false;
        }
        
        boolean[] marked = {false};
        curr.forward[1].get(marked);
        return !marked[0];
    }
    
    private int randomLevel() {
        int lvl = 1;
        while (ThreadLocalRandom.current().nextDouble() < P && lvl < MAX_LEVEL) {
            lvl++;
        }
        return lvl;
    }
    
    private static class Node {
        int key;
        AtomicMarkableReference<Node>[] forward;
        
        @SuppressWarnings("unchecked")
        Node(int level) {
            forward = (AtomicMarkableReference<Node>[]) new AtomicMarkableReference[level + 1];
            for (int i = 0; i <= level; i++) {
                forward[i] = new AtomicMarkableReference<>(null, false);
            }
        }
    }
}