package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Lock-free BST Set for integers.
 *
 * Design:
 * - AtomicReference<Node> root
 * - AtomicReference<Node> left/right child pointers
 * - AtomicBoolean deleted for logical deletion
 *
 * add():
 * - If key exists and is logically deleted, revive it using CAS on deleted.
 * - If key is absent, insert using CAS on the null child pointer.
 *
 * remove():
 * - Logical deletion using CAS on deleted.
 *
 * contains():
 * - Traverses BST and checks deleted flag.
 *
 * No locks, synchronized blocks, or blocking primitives are used.
 */
public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        final int key;
        final AtomicReference<Node> left;
        final AtomicReference<Node> right;
        final AtomicBoolean deleted;

        Node(int key) {
            this.key = key;
            this.left = new AtomicReference<>(null);
            this.right = new AtomicReference<>(null);
            this.deleted = new AtomicBoolean(false);
        }
    }

    private final AtomicReference<Node> root;

    public ConcurrentDataStructure() {
        root = new AtomicReference<>(null);
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node r = root.get();

            if (r == null) {
                Node newNode = new Node(key);
                if (root.compareAndSet(null, newNode)) {
                    return true;
                }
                continue;
            }

            Node curr = r;

            while (true) {
                if (key == curr.key) {
                    while (true) {
                        boolean isDeleted = curr.deleted.get();
                        if (!isDeleted) {
                            return false;
                        }
                        if (curr.deleted.compareAndSet(true, false)) {
                            return true;
                        }
                    }
                } else if (key < curr.key) {
                    Node left = curr.left.get();

                    if (left == null) {
                        Node newNode = new Node(key);
                        if (curr.left.compareAndSet(null, newNode)) {
                            return true;
                        }
                    } else {
                        curr = left;
                    }
                } else {
                    Node right = curr.right.get();

                    if (right == null) {
                        Node newNode = new Node(key);
                        if (curr.right.compareAndSet(null, newNode)) {
                            return true;
                        }
                    } else {
                        curr = right;
                    }
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        Node curr = root.get();

        while (curr != null) {
            if (key == curr.key) {
                while (true) {
                    boolean isDeleted = curr.deleted.get();
                    if (isDeleted) {
                        return false;
                    }
                    if (curr.deleted.compareAndSet(false, true)) {
                        // Node has been marked
                        return true;
                    }
                }
            } else if (key < curr.key) {
                curr = curr.left.get();
            } else {
                curr = curr.right.get();
            }
        }

        return false;
    }

    @Override
    public boolean contains(int key) {
        Node curr = root.get();

        while (curr != null) {
            if (key == curr.key) {
                return !curr.deleted.get();
            } else if (key < curr.key) {
                curr = curr.left.get();
            } else {
                curr = curr.right.get();
            }
        }

        return false;
    }
}