package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReferenceArray;

public class ConcurrentDataStructure implements SetADT {

    private static final int MAX_LEVEL = 16;
    private static final int MIN_LEVEL = 1;

    private final AtomicReference<Node> head;

    public ConcurrentDataStructure() {
        head = new AtomicReference<>(new Node(Integer.MIN_VALUE, MAX_LEVEL));
        Node tail = new Node(Integer.MAX_VALUE, MAX_LEVEL);
        for (int i = 0; i < MAX_LEVEL; i++) {
            head.get().forward[i].set(tail);
        }
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node[] preds = new Node[MAX_LEVEL];
            Node[] succs = new Node[MAX_LEVEL];
            findPredecessors(key, preds, succs);
            Node succ = succs[0];
            if (succ.key == key) {
                return false;
            }
            Node newNode = new Node(key, randomLevel());
            for (int i = 0; i < newNode.level; i++) {
                newNode.forward[i].set(succs[i]);
            }
            if (tryInsert(preds, succ, newNode)) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node[] preds = new Node[MAX_LEVEL];
            Node[] succs = new Node[MAX_LEVEL];
            findPredecessors(key, preds, succs);
            Node succ = succs[0];
            if (succ.key != key) {
                return false;
            }
            if (succ.marked.get()) {
                return false;
            }
            if (tryMark(succ)) {
                if (tryRemove(preds, succ)) {
                    return true;
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        while (true) {
            Node[] preds = new Node[MAX_LEVEL];
            Node[] succs = new Node[MAX_LEVEL];
            findPredecessors(key, preds, succs);
            Node succ = succs[0];
            if (succ.key == key && !succ.marked.get()) {
                return true;
            }
            return false;
        }
    }

    private void findPredecessors(int key, Node[] preds, Node[] succs) {
        Node pred = head.get();
        for (int i = pred.level - 1; i >= 0; i--) {
            Node curr = pred.forward[i].getReference();
            while (curr != null && curr.key < key) {
                pred = curr;
                curr = pred.forward[i].getReference();
            }
            preds[i] = pred;
            succs[i] = curr;
        }
    }

    private boolean tryInsert(Node[] preds, Node succ, Node newNode) {
        for (int i = 0; i < newNode.level; i++) {
            if (!preds[i].forward[i].compareAndSet(succ, newNode)) {
                return false;
            }
        }
        return true;
    }

    private boolean tryMark(Node node) {
        return node.marked.compareAndSet(false, true);
    }

    private boolean tryRemove(Node[] preds, Node succ) {
        for (int i = succ.level - 1; i >= 0; i--) {
            if (!preds[i].forward[i].compareAndSet(succ, succ.forward[i].getReference())) {
                return false;
            }
        }
        // Node has been marked
        return true;
    }

    private int randomLevel() {
        int level = MIN_LEVEL;
        while (Math.random() < 0.5 && level < MAX_LEVEL) {
            level++;
        }
        return level;
    }

    private static class Node {
        final int key;
        final int level;
        final AtomicMarkableReference<Boolean> marked = new AtomicMarkableReference<>(false);
        final AtomicReference<Node>[] forward;

        Node(int key, int level) {
            this.key = key;
            this.level = level;
            forward = new AtomicReference[level];
            for (int i = 0; i < level; i++) {
                forward[i] = new AtomicReference<>();
            }
        }
    }
}