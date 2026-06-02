package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.ThreadLocalRandom;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static final int MAX_LEVEL = 16;
    private static final double P = 0.5;
    
    private static final class Node {
        final int key;
        final Node[] forward;
        final AtomicMarkableReference<Boolean> marked;
        
        Node(int lvl, int key) {
            this.key = key;
            this.forward = new Node[lvl + 1];
            this.marked = new AtomicMarkableReference<>(false, false);
        }
    }
    
    private final Node header;
    private final AtomicMarkableReference<Integer> currentLevel;
    
    public ConcurrentDataStructure() {
        this.header = new Node(MAX_LEVEL, Integer.MIN_VALUE);
        this.currentLevel = new AtomicMarkableReference<>(1, false);
    }
    
    private int randomLevel() {
        int lvl = 1;
        while (ThreadLocalRandom.current().nextDouble() < P && lvl < MAX_LEVEL) {
            lvl++;
        }
        return lvl;
    }
    
    private Node[] find(int key) {
        Node[] update = new Node[MAX_LEVEL + 1];
        Node current = header;
        int level = currentLevel.getReference();
        
        for (int i = level; i >= 1; i--) {
            Node next = current.forward[i];
            while (next != null && next.key < key) {
                if (next.marked.isMarked()) {
                    Node succ = next.forward[i];
                    current.forward[i] = succ;
                    next = succ;
                } else {
                    current = next;
                    next = current.forward[i];
                }
            }
            update[i] = current;
        }
        return update;
    }
    
    @Override
    public boolean add(int key) {
        int topLevel = randomLevel();
        
        while (true) {
            Node[] update = find(key);
            Node current = update[1].forward[1];
            
            if (current != null && current.key == key) {
                if (current.marked.isMarked()) {
                    continue;
                }
                return false;
            }
            
            Node newNode = new Node(topLevel, key);
            
            for (int i = 1; i <= topLevel; i++) {
                newNode.forward[i] = update[i].forward[i];
            }
            
            if (update[1].forward[1] != current) {
                continue;
            }
            update[1].forward[1] = newNode;
            
            for (int i = 2; i <= topLevel; i++) {
                while (update[i].forward[i] != newNode.forward[i]) {
                    update = find(key);
                }
                update[i].forward[i] = newNode;
            }
            
            int level;
            do {
                level = currentLevel.getReference();
                if (topLevel <= level) break;
            } while (!currentLevel.compareAndSet(level, topLevel, false, false));
            
            return true;
        }
    }
    
    @Override
    public boolean remove(int key) {
        while (true) {
            Node[] update = find(key);
            Node current = update[1].forward[1];
            
            if (current == null || current.key != key) {
                return false;
            }
            
            Node next = current.forward[1];
            if (current.marked.isMarked()) {
                return false;
            }
            
            if (current.forward[1] != next) {
                continue;
            }
            
            if (!current.marked.compareAndSet(false, false, false, true)) {
                continue;
            }
            
            // Node has been marked
            
            for (int i = 1; i < current.forward.length; i++) {
                while (true) {
                    if (update[i].forward[i] != current) {
                        update = find(key);
                        current = update[1].forward[1];
                        if (current == null || current.key != key || !current.marked.isMarked()) {
                            return true;
                        }
                    }
                    update[i].forward[i] = current.forward[i];
                    break;
                }
            }
            
            int level;
            do {
                level = currentLevel.getReference();
                if (level <= 1 || header.forward[level] != null) break;
            } while (!currentLevel.compareAndSet(level, level - 1, false, false));
            
            return true;
        }
    }
    
    @Override
    public boolean contains(int key) {
        Node current = header;
        int level = currentLevel.getReference();
        
        for (int i = level; i >= 1; i--) {
            Node next = current.forward[i];
            while (next != null && next.key < key) {
                if (next.marked.isMarked()) {
                    Node succ = next.forward[i];
                    current.forward[i] = succ;
                    next = succ;
                } else {
                    current = next;
                    next = current.forward[i];
                }
            }
        }
        
        current = current.forward[1];
        return current != null && current.key == key && !current.marked.isMarked();
    }
}