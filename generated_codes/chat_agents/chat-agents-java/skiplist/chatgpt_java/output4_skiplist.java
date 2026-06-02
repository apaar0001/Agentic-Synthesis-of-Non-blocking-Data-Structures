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
    Node(int key, int topLevel) {
        this.key = key;
        this.topLevel = topLevel;
        this.next = (AtomicMarkableReference<Node>[]) new AtomicMarkableReference[topLevel + 1];
        for (int i = 0; i <= topLevel; i++) {
            this.next[i] = new AtomicMarkableReference<>(null, false);
        }
    }
}

public ConcurrentDataStructure() {
    this.head = new Node(Integer.MIN_VALUE, MAX_LEVEL);
    this.tail = new Node(Integer.MAX_VALUE, MAX_LEVEL);
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
        if (find(key, preds, succs)) {
            return false;
        }

        Node node = new Node(key, topLevel);
        for (int level = 0; level <= topLevel; level++) {
            node.next[level].set(succs[level], false);
        }

        if (!preds[0].next[0].compareAndSet(succs[0], node, false, false)) {
            continue;
        }

        for (int level = 1; level <= topLevel; level++) {
            while (true) {
                if (preds[level].next[level].compareAndSet(succs[level], node, false, false)) {
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
    boolean[] marked = {false};

    while (true) {
        if (!find(key, preds, succs)) {
            return false;
        }

        Node node = succs[0];

        for (int level = node.topLevel; level >= 1; level--) {
            Node succ = node.next[level].get(marked);
            while (!marked[0]) {
                node.next[level].compareAndSet(succ, succ, false, true);
                succ = node.next[level].get(marked);
            }
        }

        Node succ = node.next[0].get(marked);
        while (true) {
            if (marked[0]) {
                return false;
            }

            if (node.next[0].compareAndSet(succ, succ, false, true)) {
                // Node has been marked
                find(key, preds, succs);
                return true;
            }

            succ = node.next[0].get(marked);
        }
    }
}

@Override
public boolean contains(int key) {
    boolean[] marked = {false};
    Node pred = head;
    Node curr = null;
    Node succ;

    for (int level = MAX_LEVEL; level >= 0; level--) {
        curr = pred.next[level].getReference();

        while (true) {
            succ = curr.next[level].get(marked);

            while (marked[0]) {
                curr = succ;
                succ = curr.next[level].get(marked);
            }

            if (curr.key < key) {
                pred = curr;
                curr = succ;
            } else {
                break;
            }
        }
    }

    return curr.key == key && !curr.next[0].isMarked();
}

private boolean find(int key, Node[] preds, Node[] succs) {
    boolean[] marked = {false};

    retry:
    while (true) {
        Node pred = head;

        for (int level = MAX_LEVEL; level >= 0; level--) {
            Node curr = pred.next[level].getReference();

            while (true) {
                Node succ = curr.next[level].get(marked);

                while (marked[0]) {
                    if (!pred.next[level].compareAndSet(curr, succ, false, false)) {
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

        return succs[0].key == key;
    }
}

private int randomLevel() {
    int level = 0;
    while (level < MAX_LEVEL && ThreadLocalRandom.current().nextInt(2) == 0) {
        level++;
    }
    return level;
}


}
