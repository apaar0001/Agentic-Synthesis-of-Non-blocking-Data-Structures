package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.ThreadLocalRandom;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    // Lock-freedom test helpers (auto-injected)
    private static final java.util.concurrent.atomic.AtomicBoolean _lfVictimChosen = new java.util.concurrent.atomic.AtomicBoolean(false);
    private static final ThreadLocal<Integer> _lfOpCount = ThreadLocal.withInitial(() -> 0);
    private static final ThreadLocal<Boolean> _lfRetired =
            ThreadLocal.withInitial(() -> false);

    /**
     * Decide if the *current* thread should become the victim.
     *
     * Each call increments a per-thread operation counter. Once a thread
     * has executed more than 100 operations and no victim has been chosen,
     * it atomically claims the victim role and will then stall.
     */
    private static boolean _lfShouldStall() {
        int c = _lfOpCount.get() + 1;
        _lfOpCount.set(c);
        if (c > 100 && !_lfVictimChosen.get() && _lfVictimChosen.compareAndSet(false, true)) {
            return true;
        }
        return false;
    }

    private static final int MAX_LEVEL = 32;
    private final Node head;
    private final AtomicInteger size;
    
    private static class Node {
        final int key;
        final AtomicMarkableReference<Node>[] next;
        final int topLevel;
        
        @SuppressWarnings("unchecked")
        Node(int key, int height) {
            this.key = key;
            this.topLevel = height;
            this.next = (AtomicMarkableReference<Node>[]) new AtomicMarkableReference[MAX_LEVEL + 1];
            for (int i = 0; i <= height; i++) {
                next[i] = new AtomicMarkableReference<>(null, false);
            }
        }
    }
    
    public ConcurrentDataStructure() {
        head = new Node(Integer.MIN_VALUE, MAX_LEVEL);
        Node tail = new Node(Integer.MAX_VALUE, MAX_LEVEL);
        for (int i = 0; i <= MAX_LEVEL; i++) {
            head.next[i].set(tail, false);
        }
        size = new AtomicInteger(0);
    }
    
    private int randomLevel() {
        int level = 0;
        while (ThreadLocalRandom.current().nextDouble() < 0.5 && level < MAX_LEVEL) {
            level++;
        }
        return level;
    }
    
    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int topLevel = randomLevel();
        Node[] preds = new Node[MAX_LEVEL + 1];
        Node[] succs = new Node[MAX_LEVEL + 1];
        
        while (true) {
            if (find(key, preds, succs)) {
                return false;
            }
            
            Node newNode = new Node(key, topLevel);
            for (int level = 0; level <= topLevel; level++) {
                Node succ = succs[level];
                newNode.next[level].set(succ, false);
            }
            
            Node pred = preds[0];
            Node succ = succs[0];
            if (!pred.next[0].compareAndSet(succ, newNode, false, false)) {
                continue;
            }
            
            for (int level = 1; level <= topLevel; level++) {
                while (true) {
                    pred = preds[level];
                    succ = succs[level];
                    if (pred.next[level].compareAndSet(succ, newNode, false, false)) {
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
        if (_lfRetired.get()) {
            return false;
        }
        Node[] preds = new Node[MAX_LEVEL + 1];
        Node[] succs = new Node[MAX_LEVEL + 1];
        Node victim;
        
        while (true) {
            if (!find(key, preds, succs)) {
                return false;
            }
            
            victim = succs[0];
            for (int level = victim.topLevel; level >= 1; level--) {
                boolean[] marked = {false};
                Node next = victim.next[level].get(marked);
                while (!marked[0]) {
                    victim.next[level].compareAndSet(next, next, false, true);
                    next = victim.next[level].get(marked);
                }
            }
            
            boolean[] marked = {false};
            Node next = victim.next[0].get(marked);
            while (true) {
                boolean iMarkedIt = victim.next[0].compareAndSet(next, next, false, true);
                next = victim.next[0].get(marked);
                if (iMarkedIt) {
                    // Node has been marked
            // Lock-freedom victim stall injection (auto-injected)
            if (_lfShouldStall()) {
                System.err.println("LOG: Victim thread stalling inside remove()");
                try {
                    Thread.sleep(10_000);
                } catch (InterruptedException ignored) {
                }
                System.err.println("LOG: Victim resumed and retiring");
                _lfRetired.set(true);
                return false;
            }
                    find(key, preds, succs);
                    size.decrementAndGet();
                    return true;
                } else if (marked[0]) {
                    return false;
                }
            }
        }
    }
    
    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        Node[] preds = new Node[MAX_LEVEL + 1];
        Node[] succs = new Node[MAX_LEVEL + 1];
        return find(key, preds, succs);
    }
    
    private boolean find(int key, Node[] preds, Node[] succs) {
        boolean[] marked = {false};
        boolean snip;
        Node pred = null;
        Node curr = null;
        Node succ;
        
        retry: while (true) {
            pred = head;
            for (int level = MAX_LEVEL; level >= 0; level--) {
                curr = pred.next[level].getReference();
                while (true) {
                    succ = curr.next[level].get(marked);
                    while (marked[0]) {
                        snip = pred.next[level].compareAndSet(curr, succ, false, false);
                        if (!snip) {
                            continue retry;
                        }
                        curr = pred.next[level].getReference();
                        succ = curr.next[level].get(marked);
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