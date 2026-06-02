package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {

    private AtomicReference<Node> root;

    public ConcurrentDataStructure() {
        root = new AtomicReference<>(null);
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node curr = root.get();
            if (curr == null) {
                Node newNode = new Node(key);
                if (root.compareAndSet(null, newNode)) {
                    return true;
                }
            } else {
                Node[] predAndSucc = findPredAndSucc(curr, key);
                Node pred = predAndSucc[0];
                Node succ = predAndSucc[1];
                if (succ == null || succ.key != key) {
                    Node newNode = new Node(key);
                    newNode.left.set(pred);
                    newNode.right.set(succ);
                    if (pred.right.compareAndSet(succ, newNode)) {
                        return true;
                    }
                } else {
                    return false;
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node curr = root.get();
            if (curr == null) {
                return false;
            } else {
                Node[] predAndSucc = findPredAndSucc(curr, key);
                Node pred = predAndSucc[0];
                Node succ = predAndSucc[1];
                if (succ == null || succ.key != key) {
                    return false;
                } else {
                    Node succSucc = succ.right.getReference();
                    if (succSucc == null) {
                        if (succ.marked.compareAndSet(false, true)) {
                            // Node has been marked
                            if (pred.right.compareAndSet(succ, null)) {
                                return true;
                            }
                        }
                    } else {
                        if (succSucc.marked.compareAndSet(false, true)) {
                            // Node has been marked
                            if (succ.right.compareAndSet(succSucc, succSucc.right.getReference())) {
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        while (true) {
            Node curr = root.get();
            if (curr == null) {
                return false;
            } else {
                Node[] predAndSucc = findPredAndSucc(curr, key);
                Node pred = predAndSucc[0];
                Node succ = predAndSucc[1];
                if (succ == null || succ.key != key) {
                    return false;
                } else {
                    return !succ.marked.get();
                }
            }
        }
    }

    private Node[] findPredAndSucc(Node curr, int key) {
        Node pred = null;
        Node succ = curr;
        while (succ != null) {
            if (succ.key == key) {
                return new Node[]{pred, succ};
            } else if (succ.key < key) {
                pred = succ;
                succ = succ.right.getReference();
            } else {
                succ = succ.left.getReference();
            }
        }
        return new Node[]{pred, null};
    }

    private class Node {
        int key;
        AtomicReference<Node> left;
        AtomicReference<Node> right;
        AtomicMarkableReference<Boolean> marked;

        Node(int key) {
            this.key = key;
            left = new AtomicReference<>(null);
            right = new AtomicReference<>(null);
            marked = new AtomicMarkableReference<>(false);
        }
    }
}