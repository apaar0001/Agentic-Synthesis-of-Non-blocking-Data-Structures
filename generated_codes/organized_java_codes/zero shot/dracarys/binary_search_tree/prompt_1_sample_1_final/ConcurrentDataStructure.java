package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        int key;
        AtomicReference<Node> left;
        AtomicReference<Node> right;
        AtomicMarkableReference<Node> parent;
        boolean isLeaf;

        public Node(int key, boolean isLeaf) {
            this.key = key;
            this.isLeaf = isLeaf;
            this.left = new AtomicReference<>();
            this.right = new AtomicReference<>();
            this.parent = new AtomicMarkableReference<>(null, false);
        }
    }

    private AtomicReference<Node> root;

    public ConcurrentDataStructure() {
        root = new AtomicReference<>();
        Node sentinel = new Node(Integer.MIN_VALUE, true);
        root.set(sentinel);
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node parent = null;
            Node current = root.get();

            while (!current.isLeaf) {
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

            Node newLeaf = new Node(key, true);
            Node oldLeaf = new Node(oldKey, true);
            Node newInternal = new Node(0, false);

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
                if (parent.left.compareAndSet(current, newInternal)) {
                    return true;
                } else if (parent.right.compareAndSet(current, newInternal)) {
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
                    current = current.left.get();
                } else {
                    current = current.right.get();
                }
            }

            if (current.key == Integer.MIN_VALUE || current.key != key) {
                return false;
            }

            AtomicMarkableReference<Node> parentRef = parent.parent;
            if (parentRef.compareAndSet(current, null, false, true)) {
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
                if (current.key == key) {
                    return true;
                }

                if (key < current.key) {
                    current = current.left.get();
                } else {
                    current = current.right.get();
                }
            }

            if (current.key == key) {
                return true;
            }

            return false;
        }
    }
}