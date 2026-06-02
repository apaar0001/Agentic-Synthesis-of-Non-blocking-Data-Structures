package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        int key;
        AtomicReference<Node> left;
        AtomicReference<Node> right;
        AtomicMarkableReference<Node> isLeaf;

        Node(int key) {
            this.key = key;
            this.left = new AtomicReference<>();
            this.right = new AtomicReference<>();
            this.isLeaf = new AtomicMarkableReference<>(null, false);
        }
    }

    private AtomicReference<Node> root;

    public ConcurrentDataStructure() {
        root = new AtomicReference<>();
        root.set(new Node(Integer.MIN_VALUE));
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node parent = null;
            Node current = root.get();

            while (!current.isLeaf.getReference() == null) {
                parent = current;

                if (key < current.key) {
                    current = current.left.get();
                } else {
                    current = current.right.get();
                }
            }

            if (current.key == Integer.MIN_VALUE) {
                current.key = key;
                return true;
            }

            if (current.key == key) {
                return false;
            }

            int oldKey = current.key;

            Node newLeaf = new Node(key);
            Node oldLeaf = new Node(oldKey);
            Node newInternal = new Node(Math.max(key, oldKey));

            newInternal.left.set(newLeaf);
            newInternal.right.set(oldLeaf);

            if (parent == null) {
                if (root.compareAndSet(current, newInternal)) {
                    return true;
                }
            } else {
                if (parent.left.get() == current) {
                    if (parent.left.compareAndSet(current, newInternal)) {
                        return true;
                    }
                } else {
                    if (parent.right.compareAndSet(current, newInternal)) {
                        return true;
                    }
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node parent = null;
            Node grandParent = null;
            Node current = root.get();

            while (!current.isLeaf.getReference() == null) {
                grandParent = parent;
                parent = current;

                if (key < current.key) {
                    current = current.left.get();
                } else {
                    current = current.right.get();
                }
            }

            if (current.key == Integer.MIN_VALUE || current.key != key) {
                return false;
            }

            if (parent == null) {
                if (current.isLeaf.compareAndSet(null, null, false, true)) {
                    // Node has been marked
                    return true;
                }
            }

            Node sibling;
            if (parent.left.get() == current) {
                sibling = parent.right.get();
            } else {
                sibling = parent.left.get();
            }

            if (grandParent == null) {
                if (root.compareAndSet(current, sibling)) {
                    return true;
                }
            } else {
                if (grandParent.left.get() == parent) {
                    if (grandParent.left.compareAndSet(parent, sibling)) {
                        return true;
                    }
                } else {
                    if (grandParent.right.compareAndSet(parent, sibling)) {
                        return true;
                    }
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        while (true) {
            Node current = root.get();

            while (!current.isLeaf.getReference() == null) {
                if (current.isLeaf.isMarked()) {
                    continue;
                }

                if (key < current.key) {
                    current = current.left.get();
                } else {
                    current = current.right.get();
                }
            }

            if (current.key != Integer.MIN_VALUE && current.key == key) {
                return true;
            }

            return false;
        }
    }
}