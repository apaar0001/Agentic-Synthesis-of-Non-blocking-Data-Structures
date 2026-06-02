package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.ThreadLocalRandom;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static final int MAX_LEVEL = 16;
    private final Node head;
    private final Node tail;

    public static final class Node {
        public final int key;
        public final int height;
        public final AtomicMarkableReference<Node>[] next;

        @SuppressWarnings("unchecked")
        public Node(int key, int height) {
            this.key = key;
            this.height = height;
            this.next = (AtomicMarkableReference<Node>[]) new AtomicMarkableReference[height + 1];
            for (int i = 0; i <= height; i++) {
                this.next[i] = new AtomicMarkableReference<>(null, false);
            }
        }
    }

    public ConcurrentDataStructure() {
        this.head = new Node(Integer.MIN_VALUE, MAX_LEVEL);
        this.tail = new Node(Integer.MAX_VALUE, MAX_LEVEL);
        for (int i = 0; i <= MAX_LEVEL; i++) {
            this.head.next[i].set(this.tail, false);
        }
    }

    private boolean find(int key, Node[] preds, Node[] succs) {
        boolean[] marked = {false};
        boolean retry;
        
        retry_loop:
        while (true) {
            retry = false;
            Node pred = head;
            for (int level = MAX_LEVEL; level >= 0; level--) {
                Node curr = pred.next[level].getReference();
                while (true) {
                    if (curr == null) {
                        retry = true;
                        continue retry_loop;
                    }
                    Node succ = curr.next[level].get(marked);
                    while (marked[0]) {
                        boolean snip = pred.next[level].compareAndSet(curr, succ, false, false);
                        if (!snip) {
                            continue retry_loop;
                        }
                        curr = pred.next[level].getReference();
                        if (curr == null) {
                            retry = true;
                            continue retry_loop;
                        }
                        succ = curr.next[level].get(marked);
                    }
                    if (curr.key >= key) {
                        preds[level] = pred;
                        succs[level] = curr;
                        break;
                    }
                    pred = curr;
                    curr = succ;
                }
            }
            return succs[0].key == key;
        }
    }

    @Override
    public boolean add(int key) {
        int topLevel = randomLevel();
        Node[] preds = new Node[MAX_LEVEL + 1];
        Node[] succs = new Node[MAX_LEVEL + 1];

        while (true) {
            if (find(key, preds, succs)) {
                return false;
            }
            Node newNode = new Node(key, topLevel);
            for (int i = 0; i <= topLevel; i++) {
                newNode.next[i].set(succs[i], false);
            }
            Node pred = preds[0];
            Node succ = succs[0];
            if (!pred.next[0].compareAndSet(succ, newNode, false, false)) {
                continue;
            }
            for (int i = 1; i <= topLevel; i++) {
                while (true) {
                    pred = preds[i];
                    succ = succs[i];
                    if (pred.next[i].compareAndSet(succ, newNode, false, false)) {
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
        Node[] preds = new Node[MAX_LEVEL + 1];
        Node[] succs = new Node[MAX_LEVEL + 1];

        while (true) {
            if (!find(key, preds, succs)) {
                return false;
            }
            Node nodeToRemove = succs[0];
            for (int level = nodeToRemove.height; level >= 1; level--) {
                boolean[] marked = {false};
                Node succ = nodeToRemove.next[level].get(marked);
                while (!marked[0]) {
                    nodeToRemove.next[level].compareAndSet(succ, succ, false, true);
                    succ = nodeToRemove.next[level].get(marked);
                }
            }
            boolean[] marked = {false};
            Node succ = nodeToRemove.next[0].get(marked);
            while (!marked[0]) {
                boolean iMarkedIt = nodeToRemove.next[0].compareAndSet(succ, succ, false, true);
                succ = nodeToRemove.next[0].get(marked);
                if (iMarkedIt) {
                    // Node has been marked
                    find(key, preds, succs);
                    return true;
                }
            }
            return false;
        }
    }

    @Override
    public boolean contains(int key) {
        boolean[] marked = {false};
        Node pred = head;
        for (int level = MAX_LEVEL; level >= 0; level--) {
            Node curr = pred.next[level].getReference();
            while (curr != null && curr.key < key) {
                pred = curr;
                curr = curr.next[level].getReference();
            }
        }
        if (pred == null) {
            return false;
        }
        Node curr = pred.next[0].getReference();
        if (curr == null) {
            return false;
        }
        curr.next[0].get(marked);
        return curr.key == key && !marked[0];
    }

    private int randomLevel() {
        int lvl = 0;
        while (lvl < MAX_LEVEL && ThreadLocalRandom.current().nextInt(2) == 0) {
            lvl++;
        }
        return lvl;
    }
}