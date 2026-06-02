package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.ThreadLocalRandom;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
private static final int MAX_LEVEL = 32;
private final Node head;
private final Node tail;


private static final class Node {
    final int key;
    final int topLevel;
    final AtomicMarkableReference<Node>[] next;

    @SuppressWarnings("unchecked")
    Node(int key, int height) {
        this.key = key;
        this.topLevel = height;
        this.next = (AtomicMarkableReference<Node>[]) new AtomicMarkableReference[height + 1];
        for (int i = 0; i <= height; i++) {
            this.next[i] = new AtomicMarkableReference<>(null, false);
        }
    }
}

public ConcurrentDataStructure() {
    tail = new Node(Integer.MAX_VALUE, MAX_LEVEL);
    head = new Node(Integer.MIN_VALUE, MAX_LEVEL);
    for (int i = 0; i <= MAX_LEVEL; i++) {
        head.next[i].set(tail, false);
    }
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
            newNode.next[level].set(succs[level], false);
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

        return true;
    }
}

@Override
public boolean remove(int key) {
    Node victim;
    boolean isMarked;
    boolean[] marked = {false};
    Node[] preds = new Node[MAX_LEVEL + 1];
    Node[] succs = new Node[MAX_LEVEL + 1];

    while (true) {
        boolean found = find(key, preds, succs);
        if (!found) {
            return false;
        }

        victim = succs[0];

        for (int level = victim.topLevel; level >= 1; level--) {
            Node succ = victim.next[level].get(marked);
            while (!marked[0]) {
                victim.next[level].compareAndSet(succ, succ, false, true);
                succ = victim.next[level].get(marked);
            }
        }

        Node succ = victim.next[0].get(marked);
        while (true) {
            isMarked = victim.next[0].compareAndSet(succ, succ, false, true);
            if (isMarked) {
                // Node has been marked
                find(key, preds, succs);
                return true;
            }

            succ = victim.next[0].get(marked);
            if (marked[0]) {
                return false;
            }
        }
    }
}

@Override
public boolean contains(int key) {
    boolean[] marked = {false};
    Node curr = head;

    for (int level = MAX_LEVEL; level >= 0; level--) {
        curr = curr.next[level].getReference();

        while (curr.key < key) {
            curr = curr.next[level].getReference();
        }

        if (curr.key == key) {
            curr.next[0].get(marked);
            return !marked[0];
        }
    }

    return false;
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

private int randomLevel() {
    int level = 0;
    int rnd = ThreadLocalRandom.current().nextInt();

    while ((rnd & 1) == 1 && level < MAX_LEVEL) {
        level++;
        rnd >>>= 1;
    }

    return level;
}


}
