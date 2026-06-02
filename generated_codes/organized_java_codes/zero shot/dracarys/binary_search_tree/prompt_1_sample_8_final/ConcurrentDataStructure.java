package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        int key;
        AtomicMarkableReference<Node> left;
        AtomicMarkableReference<Node> right;
        boolean isLeaf;

        public Node() {
            this.isLeaf = true;
            this.key = Integer.MIN_VALUE;
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
        }
    }

    private AtomicReference<Node> root;

    public ConcurrentDataStructure() {
        root = new AtomicReference<>(new Node());
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node parent = null;
            Node current = root.get();

            while (!current.isLeaf) {
                parent = current;

                if (key < current.key) {
                    current = current.left.getReference();
                } else {
                    current = current.right.getReference();
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

            Node newLeaf = new Node();
            newLeaf.isLeaf = true;
            newLeaf.key = key;

            Node oldLeaf = new Node();
            oldLeaf.isLeaf = true;
            oldLeaf.key = oldKey;

            Node newInternal = new Node();
            newInternal.isLeaf = false;

            if (key < oldKey) {
                newInternal.key = oldKey;
                newInternal.left.set(newLeaf);
                newInternal.right.set(oldLeaf);
            } else {
                newInternal.key = key;
                newInternal.left.set(oldLeaf);
                newInternal.right.set(newLeaf);
            }

            if (parent == null) {
                if (root.compareAndSet(current, newInternal)) {
                    return true;
                }
            } else {
                AtomicMarkableReference<Node> parentRef;
                if (parent.left.getReference() == current) {
                    parentRef = parent.left;
                } else {
                    parentRef = parent.right;
                }

                if (parentRef.compareAndSet(current, newInternal, false, false)) {
                    return true;
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

            while (!current.isLeaf) {
                grandParent = parent;
                parent = current;

                if (key < current.key) {
                    current = current.left.getReference();
                } else {
                    current = current.right.getReference();
                }
            }

            if (current.key == Integer.MIN_VALUE || current.key != key) {
                return false;
            }

            if (parent == null) {
                current.key = Integer.MIN_VALUE;
                return true;
            }

            Node sibling;
            if (parent.left.getReference() == current) {
                sibling = parent.right.getReference();
            } else {
                sibling = parent.left.getReference();
            }

            if (grandParent == null) {
                if (root.compareAndSet(current, sibling)) {
                    // Node has been marked
                    return true;
                }
            } else {
                AtomicMarkableReference<Node> parentRef;
                if (grandParent.left.getReference() == parent) {
                    parentRef = grandParent.left;
                } else {
                    parentRef = grandParent.right;
                }

                if (parentRef.compareAndSet(parent, sibling, false, false)) {
                    // Node has been marked
                    return true;
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        while (true) {
            Node current = root.get();

            while (!current.isLeaf) {
                if (key < current.key) {
                    current = current.left.getReference();
                } else {
                    current = current.right.getReference();
                }
            }

            if (current.key != Integer.MIN_VALUE && current.key == key) {
                return true;
            }

            if (current.key == Integer.MIN_VALUE || current.key != key) {
                return false;
            }
        }
    }
}