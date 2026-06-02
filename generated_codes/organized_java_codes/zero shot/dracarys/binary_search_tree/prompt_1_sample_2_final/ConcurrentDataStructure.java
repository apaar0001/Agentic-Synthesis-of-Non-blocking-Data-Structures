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
    }

    private AtomicReference<Node> root;

    public ConcurrentDataStructure() {
        Node sentinel = new Node();
        sentinel.isLeaf = true;
        sentinel.key = Integer.MIN_VALUE;
        sentinel.left = new AtomicMarkableReference<>(null, false);
        sentinel.right = new AtomicMarkableReference<>(null, false);
        root = new AtomicReference<>(sentinel);
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
                newInternal.left = new AtomicMarkableReference<>(newLeaf, false);
                newInternal.right = new AtomicMarkableReference<>(oldLeaf, false);
            } else {
                newInternal.key = key;
                newInternal.left = new AtomicMarkableReference<>(oldLeaf, false);
                newInternal.right = new AtomicMarkableReference<>(newLeaf, false);
            }

            if (parent == null) {
                if (root.compareAndSet(current, newInternal)) {
                    return true;
                }
            } else {
                if (parent.left.getReference() == current) {
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

            AtomicMarkableReference<Node> ref;
            if (grandParent == null) {
                ref = root;
            } else {
                if (grandParent.left.getReference() == parent) {
                    ref = grandParent.left;
                } else {
                    ref = grandParent.right;
                }
            }

            if (ref.compareAndSet(parent, sibling, false, true)) {
                // Node has been marked
                return true;
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

            return false;
        }
    }
}