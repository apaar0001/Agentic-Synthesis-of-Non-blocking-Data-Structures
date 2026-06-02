```java
package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.Random;

public class ConcurrentDataStructure implements SetADT {
    private static final int MAX_LEVEL = 32;
    private final Node head;
    private final Node tail;
    private final Random random = new Random();

    static final class Node {
        final int key;
        final int height;
        final AtomicMarkableReference<Node>[] next;

        @SuppressWarnings("unchecked")
        Node(int key, int height) {
            this.key = key;
            this.height = height;
            this.next = (AtomicMarkableReference<Node>[]) new AtomicMarkableReference[height + 1];
            for (int i = 0; i <= height; i++) {
                this.next[i] = new AtomicMarkableReference<>(null, false);
            }
        }
    }

    public ConcurrentDataStructure() {
        head = new Node(Integer.MIN_VALUE, MAX_LEVEL);
        tail = new Node(Integer.MAX_VALUE, MAX_LEVEL);
        for (int i = 0; i <= MAX_LEVEL; i++) {
            head.next[i].set(tail, false);
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
                    Node succ = curr.next[level].get(marked);
                    while (marked[0]) {
                        if (!pred.next[level].compareAndSet(curr, succ, false, false)) {
                            continue retry_loop;
                        }
                        curr = pred.next[level].getReference();
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
            Node succ = succs[0];
            for (int level = succ.height; level >= 1; level--) {
                boolean[] marked = {false};
                Node next = succ.next[level].get(marked);
                while (!marked[0]) {
                    succ.next[level].compareAndSet(next, next, false, true);
                    next = succ.next[level].get(marked);
                }
            }
            boolean[] marked = {false};
            Node next = succ.next[0].get(marked);
            while (!marked[0]) {
                if (succ.next[0].compareAndSet(next, next, false, true)) {
                    // Node has been marked
                    find(key, preds, succs);
                    return true;
                }
                next = succ.next[0].get(marked);
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
            while (true) {
                Node succ = curr.next[level].get(marked);
                while (marked[0]) {
                    curr = succ;
                    succ = curr.next[level].get(marked);
                }
                if (curr.key >= key) {
                    if (curr.key == key) {
                        return true;
                    }
                    break;
                }
                pred = curr;
                curr = succ;
            }
        }
        return false;
    }

    private int randomLevel() {
        int lvl = 0;
        while (lvl < MAX_LEVEL && random.nextBoolean()) {
            lvl++;
        }
        return lvl;
    }
}

```