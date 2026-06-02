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
        Node[] update = new Node[MAX_LEVEL + 1];
        
        while (true) {
            Node current = header;
            boolean[] mark = new boolean[1];
            
            for (int i = MAX_LEVEL; i >= 1; i--) {
                Node n = findNext(current, i, key, mark);
                if (i <= topLevel) {
                    update[i] = current;
                }
                if (n != null && n.key == key && !mark[0]) {
                    return false;
                }
            }
            
            if (topLevel > level.getReference()) {
                level.compareAndSet(level.getReference(), topLevel, false, false);
            }
            
            Node newNode = new Node(topLevel);
            newNode.key = key;
            
            for (int i = 1; i <= topLevel; i++) {
                newNode.forward[i] = update[i].forward[i].getReference();
            }
            
            if (!update[1].forward[1].compareAndSet(update[1].forward[1].getReference(), newNode, false, false)) {
                continue;
            }
            
            for (int i = 2; i <= topLevel; i++) {
                while (true) {
                    if (update[i].forward[i].compareAndSet(update[i].forward[i].getReference(), newNode, false, false)) {
                        break;
                    }
                    findNext(update[i], i, key, mark);
                }
            }
            return true;
        }
    }
    
    @Override
    public boolean remove(int key) {
        Node[] update = new Node[MAX_LEVEL + 1];
        boolean[] mark = new boolean[1];
        
        while (true) {
            Node current = header;
            
            for (int i = MAX_LEVEL; i >= 1; i--) {
                Node n = findNext(current, i, key, mark);
                update[i] = current;
            }
            
            Node target = update[1].forward[1].getReference();
            if (target == null || target.key != key) {
                return false;
            }
            
            Node successor = target.forward[1].getReference();
            if (target.forward[1].compareAndSet(successor, successor, false, true)) {
                // Node has been marked
                findNext(header, 1, key, mark);
                return true;
            }
        }
    }
    
    @Override
    public boolean contains(int key) {
        boolean[] mark = new boolean[1];
        Node current = header;
        
        for (int i = MAX_LEVEL; i >= 1; i--) {
            Node n = findNext(current, i, key, mark);
            if (n != null && n.key == key && !mark[0]) {
                return true;
            }
        }
        return false;
    }
    
    private Node findNext(Node start, int level, int key, boolean[] mark) {
        Node current = start;
        Node next = current.forward[level].getReference();
        
        while (next != null) {
            boolean[] nextMark = new boolean[1];
            Node nextNext = next.forward[level].get(nextMark);
            
            if (nextMark[0]) {
                if (!current.forward[level].compareAndSet(next, nextNext, false, false)) {
                    return findNext(start, level, key, mark);
                }
                next = nextNext;
            } else if (next.key < key) {
                current = next;
                next = nextNext;
            } else {
                break;
            }
        }
        
        mark[0] = false;
        return next;
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