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
            int lvlFound = find(key, preds, succs);
            if (lvlFound != -1) {
                Node nodeFound = succs[lvlFound];
                if (!nodeFound.marked.get()) {
                    return false;
                }
                continue;
            }
            
            int highestLocked = -1;
            try {
                Node pred, succ;
                boolean valid = true;
                for (int level = 1; level <= topLevel && valid; level++) {
                    pred = preds[level];
                    succ = succs[level];
                    pred.lock.lock();
                    highestLocked = level;
                    valid = !pred.marked.get() && !succ.marked.get() && pred.next[level].getReference() == succ;
                }
                if (!valid) {
                    continue;
                }
                
                Node newNode = new Node(topLevel);
                newNode.key = key;
                for (int level = 1; level <= topLevel; level++) {
                    newNode.next[level].set(succs[level], false);
                }
                
                for (int level = 1; level <= topLevel; level++) {
                    preds[level].next[level].set(newNode, false);
                }
                
                int currentLevel;
                boolean[] markHolder = new boolean[1];
                do {
                    currentLevel = level.get(markHolder);
                } while (topLevel > currentLevel && !level.compareAndSet(currentLevel, topLevel, false, false));
                
                return true;
            } finally {
                for (int level = 1; level <= highestLocked; level++) {
                    preds[level].lock.unlock();
                }
            }
        }
    }
    
    @Override
    public boolean remove(int key) {
        Node[] preds = new Node[MAX_LEVEL + 1];
        Node[] succs = new Node[MAX_LEVEL + 1];
        Node victim = null;
        
        while (true) {
            int lvlFound = find(key, preds, succs);
            if (lvlFound == -1) {
                return false;
            }
            
            victim = succs[lvlFound];
            if (victim.marked.get()) {
                return false;
            }
            
            int highestLocked = -1;
            try {
                Node pred;
                boolean valid = true;
                for (int level = 1; level <= lvlFound && valid; level++) {
                    pred = preds[level];
                    pred.lock.lock();
                    highestLocked = level;
                    valid = !pred.marked.get() && pred.next[level].getReference() == victim;
                }
                if (!valid) {
                    continue;
                }
                
                if (victim.marked.compareAndSet(false, true)) {
                    // Node has been marked
                    for (int level = victim.topLevel; level >= 1; level--) {
                        preds[level].next[level].compareAndSet(victim, victim.next[level].getReference(), false, false);
                    }
                    return true;
                }
            } finally {
                for (int level = 1; level <= highestLocked; level++) {
                    preds[level].lock.unlock();
                }
            }
        }
    }
    
    @Override
    public boolean contains(int key) {
        Node[] preds = new Node[MAX_LEVEL + 1];
        Node[] succs = new Node[MAX_LEVEL + 1];
        int lvlFound = find(key, preds, succs);
        return lvlFound != -1 && !succs[lvlFound].marked.get();
    }
    
    private int find(int key, Node[] preds, Node[] succs) {
        int lvlFound = -1;
        Node pred = header;
        
        for (int level = level.getReference(); level >= 1; level--) {
            Node curr = pred.next[level].getReference();
            while (curr != null && curr.key < key) {
                pred = curr;
                curr = pred.next[level].getReference();
            }
            if (lvlFound == -1 && curr != null && curr.key == key) {
                lvlFound = level;
            }
            preds[level] = pred;
            succs[level] = curr;
        }
        return lvlFound;
    }
    
    private static int randomLevel() {
        int lvl = 1;
        while (ThreadLocalRandom.current().nextDouble() < P && lvl < MAX_LEVEL) {
            lvl++;
        }
        return lvl;
    }
    
    private static class Node {
        final int topLevel;
        int key;
        final AtomicMarkableReference<Node>[] next;
        final java.util.concurrent.locks.ReentrantLock lock = new java.util.concurrent.locks.ReentrantLock();
        final java.util.concurrent.atomic.AtomicBoolean marked = new java.util.concurrent.atomic.AtomicBoolean(false);
        
        @SuppressWarnings("unchecked")
        Node(int level) {
            this.topLevel = level;
            this.next = (AtomicMarkableReference<Node>[]) new AtomicMarkableReference[level + 1];
            for (int i = 1; i <= level; i++) {
                this.next[i] = new AtomicMarkableReference<>(null, false);
            }
        }
    }
}