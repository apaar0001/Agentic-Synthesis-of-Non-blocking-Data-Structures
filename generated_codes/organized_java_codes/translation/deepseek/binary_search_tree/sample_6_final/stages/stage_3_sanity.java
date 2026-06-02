package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static class Node {
        final int key;
        AtomicMarkableReference<Node> left;
        AtomicMarkableReference<Node> right;

        Node(int key) {
            this.key = key;
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
        }
    }

    private final AtomicMarkableReference<Node> root;

    public ConcurrentDataStructure() {
        root = new AtomicMarkableReference<>(null, false);
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node parent = null;
            Node current = root.getReference();
            AtomicMarkableReference<Node> childRef = root;
            boolean[] markedHolder = new boolean[1];

            while (current != null) {
                parent = current;
                if (key == current.key) {
                    if (childRef.isMarked()) {
                        helpDelete(parent, childRef);
                        return add(key);
                    }
                    return false;
                } else if (key < current.key) {
                    childRef = current.left;
                    current = childRef.get(markedHolder);
                    if (markedHolder[0]) {
                        helpDelete(parent, childRef);
                        return add(key);
                    }
                } else {
                    childRef = current.right;
                    current = childRef.get(markedHolder);
                    if (markedHolder[0]) {
                        helpDelete(parent, childRef);
                        return add(key);
                    }
                }
            }

            Node newNode = new Node(key);
            if (childRef.compareAndSet(null, newNode, false, false)) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node parent = null;
            Node current = root.getReference();
            AtomicMarkableReference<Node> childRef = root;
            boolean[] markedHolder = new boolean[1];

            while (current != null) {
                if (key == current.key) {
                    if (childRef.compareAndSet(current, current, false, true)) {
                        // Node has been marked
                        helpDelete(parent, childRef);
                        return true;
                    }
                    return remove(key);
                }

                parent = current;
                if (key < current.key) {
                    childRef = current.left;
                    current = childRef.get(markedHolder);
                    if (markedHolder[0]) {
                        helpDelete(parent, childRef);
                        return remove(key);
                    }
                } else {
                    childRef = current.right;
                    current = childRef.get(markedHolder);
                    if (markedHolder[0]) {
                        helpDelete(parent, childRef);
                        return remove(key);
                    }
                }
            }
            return false;
        }
    }

    @Override
    public boolean contains(int key) {
        Node current = root.getReference();
        boolean[] markedHolder = new boolean[1];

        while (current != null) {
            if (key == current.key) {
                return !root.isMarked() && !current.left.isMarked() && !current.right.isMarked();
            } else if (key < current.key) {
                current = current.left.get(markedHolder);
                if (markedHolder[0]) {
                    return false;
                }
            } else {
                current = current.right.get(markedHolder);
                if (markedHolder[0]) {
                    return false;
                }
            }
        }
        return false;
    }

    private void helpDelete(Node parent, AtomicMarkableReference<Node> childRef) {
        Node child = childRef.getReference();
        if (child == null) return;

        Node left = child.left.getReference();
        Node right = child.right.getReference();

        if (left == null && right == null) {
            childRef.compareAndSet(child, null, true, false);
        } else if (left == null) {
            childRef.compareAndSet(child, right, true, false);
        } else if (right == null) {
            childRef.compareAndSet(child, left, true, false);
        } else {
            Node successor = findSuccessor(child);
            if (successor != null) {
                child.key = successor.key;
                child.right.compareAndSet(successor, successor.right.getReference(), false, false);
            }
        }
    }

    private Node findSuccessor(Node node) {
        Node current = node.right.getReference();
        while (current != null && current.left.getReference() != null) {
            current = current.left.getReference();
        }
        return current;
    }
}