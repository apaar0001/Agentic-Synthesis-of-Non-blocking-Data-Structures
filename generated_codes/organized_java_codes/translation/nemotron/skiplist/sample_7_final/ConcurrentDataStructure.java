package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.ThreadLocalRandom;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {

    private static final int MAX_LEVEL = 16;
    private final Node head;
    private final Node tail;
    private final ThreadLocalRandom rand = ThreadLocalRandom.current();

    private static class Node {
        int key;
        int height;
        AtomicMarkableReference<Node>[] next;

        Node(int key, int height) {
            this.key = key;
            this.height = height;
            @SuppressWarnings("unchecked")
            AtomicMarkableReference<Node>[] n = new AtomicMarkableReference[height];
            for (int i = 0; i < height; i++) {
                n[i] = new AtomicMarkableReference<>(null, false);
            }
            this.next = n;
        }
    }

    public ConcurrentDataStructure() {
        head = new Node(Integer.MIN_VALUE, MAX_LEVEL);
        tail = new Node(Integer.MAX_VALUE, MAX_LEVEL);
        for (int i = 0; i < MAX_LEVEL; i++) {
            head.next[i].set(tail, false);
            tail.next[i].set(null, false);
        }
    }

    private int randomHeight() {
        int h = 1;
        while (rand.nextDouble() < 0.5 && h < MAX_LEVEL) {
            h++;
        }
        return h;
    }

    private void helpRemove(Node pred, int level) {
        Node succ = pred.next[level].getReference();
        if (succ != null && pred.next[level].isMarked()) {
            Node next = succ.next[level].getReference();
            pred.next[level].compareAndSet(succ, next, false, false);
        }
    }

    private void find(int key, Node[] pred, Node[] succ) {
        int level = MAX_LEVEL - 1;
        Node curr = head;
        while (true) {
            Node next = curr.next[level].getReference();
            while (next != tail && next.key < key) {
                if (curr.next[level].isMarked()) {
                    Node nextNext = next.next[level].getReference();
                    curr.next[level].compareAndSet(next, nextNext, false, false);
                    next = curr.next[level].getReference();
                } else {
                    curr = next;
                    next = curr.next[level].getReference();
                }
            }
            pred[level] = curr;
            succ[level] = next;
            if (level == 0) break;
            level--;
            curr = pred[level];
        }
    }

    @Override
    public boolean add(int key) {
        int maxLevel = MAX_LEVEL;
        Node[] pred = new Node[maxLevel];
        Node[] succ = new Node[maxLevel];
        outer:
        while (true) {
            find(key, pred, succ);
            Node node = succ[0];
            if (node != tail && node.key == key) {
                if (!pred[0].next[0].isMarked()) {
                    return false;
                }
                helpRemove(pred[0], 0);
                continue;
            }
            int nodeLevel = randomHeight();
            Node newNode = new Node(key, nodeLevel);
            for (int level = 0; level < nodeLevel; level++) {
                newNode.next[level].set(succ[level], false);
                while (!pred[level].next[level].compareAndSet(succ[level], newNode, false, false)) {
                    if (pred[level].next[level].isMarked()) {
                        helpRemove(pred[level], level);
                    }
                    find(key, pred, succ);
                    node = succ[0];
                    if (node != tail && node.key == key) {
                        if (!pred[0].next[0].isMarked()) {
                            return false;
                        }
                        helpRemove(pred[0], 0);
                        continue outer;
                    }
                    newNode.next[level].set(succ[level], false);
                }
            }
            return true;
        }
    }

    @Override
    public boolean remove(int key) {
        int maxLevel = MAX_LEVEL;
        Node[] pred = new Node[maxLevel];
        Node[] succ = new Node[maxLevel];
        while (true) {
            find(key, pred, succ);
            Node node = succ[0];
            if (node == tail || node.key != key) {
                return false;
            }
            boolean marked = pred[0].next[0].attemptMark(node, true);
            if (!marked) {
                if (pred[0].next[0].isMarked()) {
                    helpRemove(pred[0], 0);
                }
                find(key, pred, succ);
                continue;
            }
            // Node has been marked
            // Linearization point of remove
            // Node has been marked
            for (int level = 1; level < node.height; level++) {
                while (true) {
                    Node succNode = succ[level];
                    Node next = node.next[level].getReference();
                    if (pred[level].next[level].compareAndSet(node, next, false, false)) {
                        break;
                    }
                    if (pred[level].next[level].isMarked()) {
                        helpRemove(pred[level], level);
                    }
                    find(key, pred, succ);
                    node = succ[0];
                    if (node == tail || node.key != key) {
                        return true;
                    }
                }
            }
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        int level = MAX_LEVEL - 1;
        Node curr = head;
        while (true) {
            Node next = curr.next[level].getReference();
            while (next != tail && next.key < key) {
                if (curr.next[level].isMarked()) {
                    Node nextNext = next.next[level].getReference();
                    curr.next[level].compareAndSet(next, nextNext, false, false);
                    next = curr.next[level].getReference();
                } else {
                    curr = next;
                    next = curr.next[level].getReference();
                }
            }
            if (level == 0) {
                return (next != tail && next.key == key && !curr.next[0].isMarked());
            }
            level--;
            curr = pred[level] = curr;
        }
    }
}