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

        Node(int key) {
            this.key = key;
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
            this.isLeaf = true;
        }

        Node(int key, Node left, Node right) {
            this.key = key;
            this.left = new AtomicMarkableReference<>(left, false);
            this.right = new AtomicMarkableReference<>(right, false);
            this.isLeaf = false;
        }
    }

    private AtomicReference<Node> root;

    public ConcurrentDataStructure() {
        Node sentinel = new Node(Integer.MIN_VALUE);
        sentinel.isLeaf = true;
        root = new AtomicReference<>(sentinel);
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node current = root.get();
            Node parent = null;

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

            Node newLeaf = new Node(key);
            Node oldLeaf = new Node(oldKey);

            Node newInternal = new Node(oldKey, newLeaf, oldLeaf);

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
                AtomicMarkableReference<Node> childRef;
                if (parent.left.getReference() == current) {
                    childRef = parent.left;
                } else {
                    childRef = parent.right;
                }

                if (childRef.compareAndSet(current, newInternal)) {
                    return true;
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node current = root.get();
            Node parent = null;
            Node grandParent = null;

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

            AtomicMarkableReference<Node> currentRef = new AtomicMarkableReference<>(current, false);
            if (currentRef.getReference() != current) {
                continue;
            }

            if (currentRef.attemptMark(current, true)) {
                // Node has been marked
                if (parent == null) {
                    if (root.compareAndSet(current, null)) {
                        return true;
                    }
                } else {
                    AtomicMarkableReference<Node> childRef;
                    if (parent.left.getReference() == current) {
                        childRef = parent.left;
                    } else {
                        childRef = parent.right;
                    }

                    if (childRef.compareAndSet(current, null)) {
                        return true;
                    }
                }
            } else {
                return false;
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