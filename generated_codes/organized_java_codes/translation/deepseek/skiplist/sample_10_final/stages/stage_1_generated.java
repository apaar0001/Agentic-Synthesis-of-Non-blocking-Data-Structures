package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.ThreadLocalRandom;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReferenceArray;

public class ConcurrentDataStructure implements SetADT {
    private static final int MAX_LEVEL = 32;
    private final Node head;
    private final AtomicInteger size;
    
    private static class Node {
        final int key;
        final AtomicReferenceArray<AtomicMarkableReference<Node>> next;
        final int topLevel;
        
        Node(int key, int height) {
            this.key = key;
            this.topLevel = height;
            this.next = new AtomicReferenceArray<>(height + 1);
            for (int i = 0; i <= height; i++) {
                next.set(i, new AtomicMarkableReference<>(null, false));
            }
        }
    }
    
    public ConcurrentDataStructure() {
        this.head = new Node(Integer.MIN_VALUE, MAX_LEVEL);
        Node tail = new Node(Integer.MAX_VALUE, MAX_LEVEL);
        for (int i = 0; i <= MAX_LEVEL; i++) {
            head.next.get(i).set(tail, false);
        }
        this.size = new AtomicInteger(0);
    }
    
    private int randomLevel() {
        int level = 0;
        while (ThreadLocalRandom.current().nextDouble() < 0.5 && level < MAX_LEVEL - 1) {
            level++;
        }
        return level;
    }
    
    @Override
    public boolean add(int key) {
        int topLevel = randomLevel();
        Node[] preds = new Node[MAX_LEVEL + 1];
        Node[] succs = new Node[MAX_LEVEL + 1];
        
        while (true) {
            boolean found = find(key, preds, succs);
            if (found) {
                return false;
            }
            
            Node newNode = new Node(key, topLevel);
            for (int level = 0; level <= topLevel; level++) {
                Node succ = succs[level];
                newNode.next.get(level).set(succ, false);
            }
            
            Node pred = preds[0];
            Node succ = succs[0];
            if (!pred.next.get(0).compareAndSet(succ, newNode, false, false)) {
                continue;
            }
            
            for (int level = 1; level <= topLevel; level++) {
                while (true) {
                    pred = preds[level];
                    succ = succs[level];
                    if (pred.next.get(level).compareAndSet(succ, newNode, false, false)) {
                        break;
                    }
                    find(key, preds, succs);
                }
            }
            size.incrementAndGet();
            return true;
        }
    }
    
    @Override
    public boolean remove(int key) {
        Node[] preds = new Node[MAX_LEVEL + 1];
        Node[] succs = new Node[MAX_LEVEL + 1];
        Node victim;
        
        while (true) {
            boolean found = find(key, preds, succs);
            if (!found) {
                return false;
            }
            
            victim = succs[0];
            for (int level = victim.topLevel; level >= 1; level--) {
                AtomicMarkableReference<Node> ref = victim.next.get(level);
                Node succ = ref.getReference();
                boolean[] marked = {false};
                ref.get(marked);
                while (!marked[0]) {
                    if (ref.compareAndSet(succ, succ, false, true)) {
                        break;
                    }
                    succ = ref.getReference();
                    ref.get(marked);
                }
            }
            
            AtomicMarkableReference<Node> ref = victim.next.get(0);
            Node succ = ref.getReference();
            boolean[] marked = {false};
            ref.get(marked);
            if (ref.compareAndSet(succ, succ, false, true)) {
                // Node has been marked
                find(key, preds, succs);
                size.decrementAndGet();
                return true;
            }
        }
    }
    
    @Override
    public boolean contains(int key) {
        Node pred = head;
        Node curr = null;
        
        for (int level = MAX_LEVEL; level >= 0; level--) {
            curr = pred.next.get(level).getReference();
            while (true) {
                boolean[] marked = {false};
                Node succ = curr.next.get(level).get(marked);
                while (marked[0]) {
                    curr = succ;
                    succ = curr.next.get(level).get(marked);
                }
                if (curr.key < key) {
                    pred = curr;
                    curr = succ;
                } else {
                    break;
                }
            }
        }
        return curr.key == key;
    }
    
    private boolean find(int key, Node[] preds, Node[] succs) {
        boolean[] marked = {false};
        boolean snip;
        Node pred;
        Node curr;
        Node succ;
        
        retry:
        while (true) {
            pred = head;
            for (int level = MAX_LEVEL; level >= 0; level--) {
                curr = pred.next.get(level).getReference();
                while (true) {
                    succ = curr.next.get(level).get(marked);
                    while (marked[0]) {
                        snip = pred.next.get(level).compareAndSet(curr, succ, false, false);
                        if (!snip) {
                            continue retry;
                        }
                        curr = pred.next.get(level).getReference();
                        succ = curr.next.get(level).get(marked);
                    }
                    if (curr.key < key) {
                        pred = curr;
                        curr = succ;
                    } else {
                        break;
                    }
                }
                preds[level] = pred;
                succs[level] = curr;
            }
            return curr.key == key;
        }
    }
}