package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.ThreadLocalRandom;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static final int MAX_LEVEL = 16;
    private static final double P = 0.5;
    
    private final Node header;
    private final AtomicMarkableReference<Integer> level;
    
    public ConcurrentDataStructure() {
        this.header = new Node(MAX_LEVEL);
        this.level = new AtomicMarkableReference<>(1, false);
    }
    
    @Override
    public boolean add(int key) {
        int topLevel = randomLevel();
        Node[] preds = new Node[MAX_LEVEL + 1];
        Node[] succs = new Node[MAX_LEVEL + 1];
        
        while (true) {
            int foundLevel = find(key, preds, succs);
            if (foundLevel != -1) {
                Node nodeFound = succs[foundLevel];
                if (!nodeFound.forward[1].isMarked()) {
                    return false;
                }
                continue;
            }
            
            Node newNode = new Node(topLevel);
            newNode.key = key;
            
            for (int i = 1; i <= topLevel; i++) {
                newNode.forward[i].set(succs[i], false);
            }
            
            Node pred = preds[1];
            Node succ = succs[1];
            
            if (!pred.forward[1].compareAndSet(succ, newNode, false, false)) {
                continue;
            }
            
            for (int i = 2; i <= topLevel; i++) {
                while (true) {
                    pred = preds[i];
                    succ = succs[i];
                    if (pred.forward[i].compareAndSet(succ, newNode, false, false)) {
                        break;
                    }
                    find(key, preds, succs);
                }
            }
            
            int currentLevel;
            boolean[] markHolder = new boolean[1];
            do {
                currentLevel = level.get(markHolder);
                if (topLevel <= currentLevel) {
                    break;
                }
            } while (!level.compareAndSet(currentLevel, topLevel, false, false));
            
            return true;
        }
    }
    
    @Override
    public boolean remove(int key) {
        Node[] preds = new Node[MAX_LEVEL + 1];
        Node[] succs = new Node[MAX_LEVEL + 1];
        
        while (true) {
            int foundLevel = find(key, preds, succs);
            if (foundLevel == -1) {
                return false;
            }
            
            Node victim = succs[foundLevel];
            int victimLevel = victim.forward.length - 1;
            
            for (int i = victimLevel; i > 1; i--) {
                Node succ = victim.forward[i].getReference();
                victim.forward[i].set(succ, true);
            }
            
            Node succ = victim.forward[1].getReference();
            if (victim.forward[1].compareAndSet(succ, succ, false, true)) {
                // Node has been marked
                find(key, preds, succs);
                return true;
            }
        }
    }
    
    @Override
    public boolean contains(int key) {
        Node[] preds = new Node[MAX_LEVEL + 1];
        Node[] succs = new Node[MAX_LEVEL + 1];
        int foundLevel = find(key, preds, succs);
        if (foundLevel == -1) return false;
        return !succs[foundLevel].forward[1].isMarked();
    }
    
    private int find(int key, Node[] preds, Node[] succs) {
        int currentLevel;
        boolean[] markHolder = new boolean[1];
        
        retry: while (true) {
            Node pred = header;
            currentLevel = level.get(markHolder);
            
            for (int i = currentLevel; i >= 1; i--) {
                Node curr = pred.forward[i].getReference();
                while (true) {
                    boolean[] marked = {false};
                    Node succ = curr != null ? curr.forward[i].get(marked) : null;
                    
                    while (curr != null && marked[0]) {
                        if (!pred.forward[i].compareAndSet(curr, succ, false, false)) {
                            continue retry;
                        }
                        curr = succ;
                        if (curr != null) {
                            succ = curr.forward[i].get(marked);
                        }
                    }
                    
                    if (curr == null || curr.key >= key) {
                        preds[i] = pred;
                        succs[i] = curr;
                        break;
                    }
                    pred = curr;
                    curr = succ;
                }
            }
            
            return succs[1] != null && succs[1].key == key ? 1 : -1;
        }
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
            this.forward = (AtomicMarkableReference<Node>[]) new AtomicMarkableReference[level + 1];
            for (int i = 1; i <= level; i++) {
                this.forward[i] = new AtomicMarkableReference<>(null, false);
            }
        }
    }
}