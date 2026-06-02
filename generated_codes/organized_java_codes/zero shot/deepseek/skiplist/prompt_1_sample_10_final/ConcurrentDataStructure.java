package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.ThreadLocalRandom;

public class ConcurrentDataStructure implements SetADT {
    private static final int MAX_LEVEL = 16;
    private static final double P = 0.5;
    private final Node header;
    private final AtomicMarkableReference<Integer> level;

    public ConcurrentDataStructure() {
        header = new Node(MAX_LEVEL);
        level = new AtomicMarkableReference<>(1, false);
    }

    @Override
    public boolean add(int key) {
        int topLevel = randomLevel();
        Node[] preds = new Node[MAX_LEVEL + 1];
        Node[] succs = new Node[MAX_LEVEL + 1];

        while (true) {
            if (find(key, preds, succs)) {
                Node foundNode = succs[1];
                if (foundNode != null && !foundNode.marked.get()) {
                    return false;
                }
                continue;
            }

            Node newNode = new Node(topLevel);
            newNode.key = key;
            for (int i = 1; i <= topLevel; i++) {
                newNode.next[i].set(succs[i], false);
            }

            if (!preds[1].next[1].compareAndSet(succs[1], newNode, false, false)) {
                continue;
            }

            for (int i = 2; i <= topLevel; i++) {
                while (true) {
                    if (preds[i].next[i].compareAndSet(succs[i], newNode, false, false)) {
                        break;
                    }
                    find(key, preds, succs);
                }
            }

            while (true) {
                int currentLevel = level.getReference();
                if (currentLevel < topLevel && level.compareAndSet(currentLevel, topLevel, false, false)) {
                    continue;
                }
                break;
            }

            return true;
        }
    }

    @Override
    public boolean remove(int key) {
        Node[] preds = new Node[MAX_LEVEL + 1];
        Node[] succs = new Node[MAX_LEVEL + 1];
        Node victim;

        while (true) {
            if (!find(key, preds, succs)) {
                return false;
            }

            victim = succs[1];
            if (victim == null) {
                return false;
            }

            if (!victim.marked.compareAndSet(false, true)) {
                continue;
            }
            // Node has been marked

            for (int i = victim.next.length - 1; i >= 1; i--) {
                while (true) {
                    Node succ = victim.next[i].getReference();
                    if (!preds[i].next[i].compareAndSet(victim, succ, false, false)) {
                        find(key, preds, succs);
                        if (preds[i].next[i].getReference() != victim) {
                            break;
                        }
                    } else {
                        break;
                    }
                }
            }

            while (true) {
                int currentLevel = level.getReference();
                if (currentLevel > 1 && header.next[currentLevel].getReference() == null) {
                    level.compareAndSet(currentLevel, currentLevel - 1, false, false);
                } else {
                    break;
                }
            }

            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        Node[] preds = new Node[MAX_LEVEL + 1];
        Node[] succs = new Node[MAX_LEVEL + 1];
        if (find(key, preds, succs)) {
            Node node = succs[1];
            return node != null && !node.marked.get();
        }
        return false;
    }

    private boolean find(int key, Node[] preds, Node[] succs) {
        boolean[] marked = {false};
        Node pred, curr, succ;
        
        retry:
        while (true) {
            pred = header;
            int currentLevel = level.getReference();
            for (int i = currentLevel; i >= 1; i--) {
                curr = pred.next[i].getReference();
                while (true) {
                    if (curr == null) {
                        succ = null;
                        marked[0] = false;
                        break;
                    }
                    
                    succ = curr.next[i].get(marked);
                    while (marked[0]) {
                        if (!pred.next[i].compareAndSet(curr, succ, false, false)) {
                            continue retry;
                        }
                        curr = pred.next[i].getReference();
                        if (curr == null) {
                            succ = null;
                            marked[0] = false;
                            break;
                        }
                        succ = curr.next[i].get(marked);
                    }
                    
                    if (curr != null && curr.key < key) {
                        pred = curr;
                        curr = succ;
                    } else {
                        break;
                    }
                }
                preds[i] = pred;
                succs[i] = curr;
            }
            return curr != null && curr.key == key && !marked[0];
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
        AtomicMarkableReference<Node>[] next;
        AtomicMarkableReference<Boolean> marked;

        @SuppressWarnings("unchecked")
        Node(int level) {
            next = (AtomicMarkableReference<Node>[]) new AtomicMarkableReference[level + 1];
            for (int i = 0; i < next.length; i++) {
                next[i] = new AtomicMarkableReference<>(null, false);
            }
            marked = new AtomicMarkableReference<>(false, false);
        }
    }
}