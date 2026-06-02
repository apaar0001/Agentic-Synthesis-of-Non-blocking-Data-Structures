package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        int key;
        AtomicMarkableReference<Node> left;
        AtomicMarkableReference<Node> right;

        Node(int key) {
            this.key = key;
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
        }
    }

    private AtomicReference<Node> root;

    public ConcurrentDataStructure() {
        this.root = new AtomicReference<>(null);
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node current = root.get();
            if (current == null) {
                Node newNode = new Node(key);
                if (root.compareAndSet(null, newNode)) {
                    return true;
                }
            } else {
                Node parent = findParent(current, key);
                if (parent == null) {
                    return false;
                }
                Node child = key < parent.key ? parent.left.getReference() : parent.right.getReference();
                if (child != null && child.key == key) {
                    return false;
                }
                Node newNode = new Node(key);
                if (key < parent.key) {
                    if (parent.left.compareAndSet(child, newNode)) {
                        return true;
                    }
                } else {
                    if (parent.right.compareAndSet(child, newNode)) {
                        return true;
                    }
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node current = root.get();
            if (current == null) {
                return false;
            }
            Node parent = findParent(current, key);
            if (parent == null) {
                return false;
            }
            Node child = key < parent.key ? parent.left.getReference() : parent.right.getReference();
            if (child == null || child.key != key) {
                return false;
            }
            if (child.left.getReference() != null && child.right.getReference() != null) {
                Node successor = findSuccessor(child);
                if (successor.key == key) {
                    if (successor.left.compareAndSet(null, child.left.getReference())) {
                        if (successor.right.compareAndSet(null, child.right.getReference())) {
                            if (key < parent.key) {
                                if (parent.left.compareAndSet(child, successor)) {
                                    return true;
                                }
                            } else {
                                if (parent.right.compareAndSet(child, successor)) {
                                    return true;
                                }
                            }
                        }
                    }
                }
            } else {
                Node replacement = child.left.getReference() != null ? child.left.getReference() : child.right.getReference();
                if (key < parent.key) {
                    if (parent.left.compareAndSet(child, replacement)) {
                        // Node has been marked
                        child.left.set(null, true);
                        child.right.set(null, true);
                        return true;
                    }
                } else {
                    if (parent.right.compareAndSet(child, replacement)) {
                        // Node has been marked
                        child.left.set(null, true);
                        child.right.set(null, true);
                        return true;
                    }
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node current = root.get();
        while (current != null) {
            if (current.key == key) {
                return true;
            }
            current = key < current.key ? current.left.getReference() : current.right.getReference();
        }
        return false;
    }

    private Node findParent(Node current, int key) {
        if (current == null) {
            return null;
        }
        if (key < current.key) {
            if (current.left.getReference() == null) {
                return current;
            }
            return findParent(current.left.getReference(), key);
        } else if (key > current.key) {
            if (current.right.getReference() == null) {
                return current;
            }
            return findParent(current.right.getReference(), key);
        } else {
            return null;
        }
    }

    private Node findSuccessor(Node current) {
        if (current.right.getReference() == null) {
            return current;
        }
        return findSuccessor(current.right.getReference());
    }
}