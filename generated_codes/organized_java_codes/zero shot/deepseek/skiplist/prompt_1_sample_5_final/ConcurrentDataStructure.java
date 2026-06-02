package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.ThreadLocalRandom;

public class ConcurrentDataStructure implements SetADT {
    private static final int MAX_LEVEL = 16;
    private static final double P = 0.5;
    private final Node header;
    private final AtomicMarkableReference<Integer> levelRef;

    private static class Node {
        final int key;
        final AtomicMarkableReference<Node>[] forward;

        @SuppressWarnings("unchecked")
        Node(int level) {
            this.key = 0;
            this.forward = new AtomicMarkableReference[level + 1];
            for (int i = 0; i <= level; i++) {
                forward[i] = new AtomicMarkableReference<>(null, false);
            }
        }

        @SuppressWarnings("unchecked")
        Node(int level, int key) {
            this.key = key;
            this.forward = new AtomicMarkableReference[level + 1];
            for (int i = 0; i <= level; i++) {
                forward[i] = new AtomicMarkableReference<>(null, false);
            }
        }
    }

    public ConcurrentDataStructure() {
        header = new Node(MAX_LEVEL);
        levelRef = new AtomicMarkableReference<>(1, false);
    }

    private int randomLevel() {
        int lvl = 1;
        while (ThreadLocalRandom.current().nextDouble() < P && lvl < MAX_LEVEL) {
            lvl++;
        }
        return lvl;
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node[] preds = new Node[MAX_LEVEL + 1];
            Node[] succs = new Node[MAX_LEVEL + 1];
            int lvlFound = find(key, preds, succs);
            
            if (lvlFound != -1) {
                Node nodeFound = succs[lvlFound];
                boolean[] marked = {false};
                nodeFound.forward[lvlFound].get(marked);
                if (!marked[0]) {
                    return false;
                }
                continue;
            }
            
            int topLevel = randomLevel();
            int currentLevel = levelRef.getReference();
            
            if (topLevel > currentLevel) {
                levelRef.compareAndSet(currentLevel, topLevel);
            }
            
            Node newNode = new Node(topLevel, key);
            
            for (int level = 1; level <= topLevel; level++) {
                newNode.forward[level].set(succs[level], false);
            }
            
            Node pred = preds[1];
            Node succ = succs[1];
            
            if (!pred.forward[1].compareAndSet(succ, newNode, false, false)) {
                continue;
            }
            
            for (int level = 2; level <= topLevel; level++) {
                while (true) {
                    pred = preds[level];
                    succ = succs[level];
                    if (pred.forward[level].compareAndSet(succ, newNode, false, false)) {
                        break;
                    }
                    find(key, preds, succs);
                }
            }
            
            return true;
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node[] preds = new Node[MAX_LEVEL + 1];
            Node[] succs = new Node[MAX_LEVEL + 1];
            int lvlFound = find(key, preds, succs);
            
            if (lvlFound == -1) {
                return false;
            }
            
            Node nodeToRemove = succs[lvlFound];
            boolean[] marked = {false};
            nodeToRemove.forward[lvlFound].get(marked);
            if (marked[0]) {
                return false;
            }
            
            for (int level = nodeToRemove.forward.length - 1; level >= 1; level--) {
                AtomicMarkableReference<Node> nextRef = nodeToRemove.forward[level];
                Node succ = nextRef.getReference();
                if (!nextRef.compareAndSet(succ, succ, false, true)) {
                    continue;
                }
                // Node has been marked
                break;
            }
            
            if (tryRemove(nodeToRemove, preds, succs)) {
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node[] preds = new Node[MAX_LEVEL + 1];
        Node[] succs = new Node[MAX_LEVEL + 1];
        int lvlFound = find(key, preds, succs);
        if (lvlFound == -1) {
            return false;
        }
        Node nodeFound = succs[lvlFound];
        boolean[] marked = {false};
        nodeFound.forward[lvlFound].get(marked);
        return !marked[0];
    }

    private int find(int key, Node[] preds, Node[] succs) {
        int currentLevel = levelRef.getReference();
        Node pred = header;
        
        for (int level = currentLevel; level >= 1; level--) {
            Node curr = pred.forward[level].getReference();
            while (curr != null && curr.key < key) {
                pred = curr;
                curr = pred.forward[level].getReference();
            }
            preds[level] = pred;
            succs[level] = curr;
        }
        
        if (succs[1] != null && succs[1].key == key) {
            return 1;
        }
        return -1;
    }

    private boolean tryRemove(Node nodeToRemove, Node[] preds, Node[] succs) {
        int highestLevel = nodeToRemove.forward.length - 1;
        
        for (int level = highestLevel; level >= 1; level--) {
            while (true) {
                boolean[] marked = {false};
                Node succ = nodeToRemove.forward[level].get(marked);
                if (!marked[0]) {
                    if (!nodeToRemove.forward[level].compareAndSet(succ, succ, false, true)) {
                        continue;
                    }
                    // Node has been marked
                }
                break;
            }
        }
        
        for (int level = 1; level <= highestLevel; level++) {
            while (true) {
                Node pred = preds[level];
                Node succ = succs[level];
                if (pred.forward[level].compareAndSet(nodeToRemove, succ, false, false)) {
                    break;
                }
                find(nodeToRemove.key, preds, succs);
            }
        }
        
        return true;
    }
}