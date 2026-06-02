package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        final int key;
        final AtomicMarkableReference<Node> left;
        final AtomicMarkableReference<Node> right;

        Node(int key) {
            this.key = key;
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
        }
    }

    private static class Window {
        Node pred;
        AtomicMarkableReference<Node> predRef;
        Node curr;

        Window(Node pred, AtomicMarkableReference<Node> predRef, Node curr) {
            this.pred = pred;
            this.predRef = predRef;
            this.curr = curr;
        }
    }

    private final AtomicMarkableReference<Node> root;

    public ConcurrentDataStructure() {
        this.root = new AtomicMarkableReference<>(null, false);
    }

    private Window find(int key) {
        retry:
        while (true) {
            Node pred = null;
            AtomicMarkableReference<Node> predRef = root;
            boolean[] predMarked = {false};
            Node curr = predRef.get(predMarked);

            if (predMarked[0]) {
                continue retry;
            }

            while (curr != null) {
                boolean[] currMarked = {false};
                AtomicMarkableReference<Node> nextRef;
                Node next;

                if (key < curr.key) {
                    nextRef = curr.left;
                } else if (key > curr.key) {
                    nextRef = curr.right;
                } else {
                    next = curr.right.get(currMarked);
                    if (currMarked[0]) {
                        if (!predRef.compareAndSet(curr, next, false, false)) {
                            continue retry;
                        }
                        curr = next;
                    }
                    return new Window(pred, predRef, curr);
                }

                next = nextRef.get(currMarked);
                if (currMarked[0]) {
                    if (!predRef.compareAndSet(curr, next, false, false)) {
                        continue retry;
                    }
                    curr = next;
                } else {
                    pred = curr;
                    predRef = nextRef;
                    curr = next;
                }
            }

            return new Window(pred, predRef, null);
        }
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Window w = find(key);
            if (w.curr != null) {
                return false;
            }
            Node newNode = new Node(key);
            if (w.predRef.compareAndSet(null, newNode, false, false)) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Window w = find(key);
            if (w.curr == null) {
                return false;
            }
            Node curr = w.curr;
            boolean[] leftMarked = {false};
            boolean[] rightMarked = {false};
            Node leftChild = curr.left.get(leftMarked);
            Node rightChild = curr.right.get(rightMarked);

            if (leftMarked[0] || rightMarked[0]) {
                continue;
            }

            if (leftChild == null && rightChild == null) {
                if (curr.right.compareAndSet(null, null, false, true)) {
                    // Node has been marked
                    w.predRef.compareAndSet(curr, null, false, false);
                    return true;
                }
            } else if (leftChild == null) {
                if (curr.right.compareAndSet(rightChild, rightChild, false, true)) {
                    // Node has been marked
                    w.predRef.compareAndSet(curr, rightChild, false, false);
                    return true;
                }
            } else if (rightChild == null) {
                if (curr.left.compareAndSet(leftChild, leftChild, false, true)) {
                    // Node has been marked
                    w.predRef.compareAndSet(curr, leftChild, false, false);
                    return true;
                }
            } else {
                Node successor = findMin(rightChild);
                if (successor == null) {
                    continue;
                }
                if (curr.right.compareAndSet(rightChild, rightChild, false, true)) {
                    // Node has been marked
                    Node replacer = new Node(successor.key);
                    replacer.left.compareAndSet(null, leftChild, false, false);
                    boolean[] rMarked = {false};
                    Node rRight = curr.right.getReference();
                    replacer.right.compareAndSet(null, rRight, false, false);
                    w.predRef.compareAndSet(curr, replacer, false, false);
                    return true;
                }
            }
        }
    }

    private Node findMin(Node node) {
        Node curr = node;
        while (curr != null) {
            boolean[] marked = {false};
            Node left = curr.left.get(marked);
            if (marked[0]) return null;
            if (left == null) return curr;
            curr = left;
        }
        return null;
    }

    @Override
    public boolean contains(int key) {
        boolean[] marked = {false};
        Node curr = root.get(marked);
        if (marked[0]) return false;

        while (curr != null) {
            if (key == curr.key) {
                boolean[] currMarked = {false};
                curr.right.get(currMarked);
                return !currMarked[0];
            }
            if (key < curr.key) {
                curr = curr.left.getReference();
            } else {
                curr = curr.right.getReference();
            }
        }
        return false;
    }
}