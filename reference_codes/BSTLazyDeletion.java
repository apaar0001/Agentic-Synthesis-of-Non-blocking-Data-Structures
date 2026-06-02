package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Lock-free BST with Lazy (Logical) Deletion.
 *
 * True binary search tree structure where:
 * - Nodes are never physically removed (nodes persist in the tree)
 * - add() inserts a new node or revives a logically-deleted one via CAS
 * - remove() logically marks the node as deleted via CAS on isDeleted flag
 * - contains() returns true only for unmarked present nodes
 *
 * Lock-freedom: all operations use CAS on AtomicReference; no blocking.
 * The tree may grow unboundedly under heavy churn, but all ops are lock-free.
 */
public class BSTLazyDeletion implements SetADT {

    private static class Node {
        final int key;
        final AtomicReference<Boolean> isDeleted;
        final AtomicReference<Node> left, right;

        Node(int k) {
            key = k;
            isDeleted = new AtomicReference<>(false);
            left = new AtomicReference<>(null);
            right = new AtomicReference<>(null);
        }
    }

    private final AtomicReference<Node> root = new AtomicReference<>(null);

    public BSTLazyDeletion() {
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node n = new Node(key);
            if (root.compareAndSet(null, n))
                return true;
            Node c = root.get();
            while (true) {
                if (c.key == key) {
                    // Revive a logically-deleted node
                    if (c.isDeleted.compareAndSet(true, false))
                        return true;
                    return false; // Already active
                }
                AtomicReference<Node> next = (key < c.key) ? c.left : c.right;
                Node m = next.get();
                if (m == null) {
                    if (next.compareAndSet(null, n))
                        return true;
                    // else: another thread added; restart inner search
                } else {
                    c = m;
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        Node c = root.get();
        while (c != null) {
            if (c.key == key) {
                // Logical deletion: CAS isDeleted from false → true
                if (c.isDeleted.compareAndSet(false, true)) {
                    // Node has been marked
                    return true;
                }
                return false; // Already deleted by another thread
            }
            c = (key < c.key) ? c.left.get() : c.right.get();
        }
        return false;
    }

    @Override
    public boolean contains(int key) {
        Node c = root.get();
        while (c != null) {
            if (c.key == key)
                return !c.isDeleted.get();
            c = (key < c.key) ? c.left.get() : c.right.get();
        }
        return false;
    }
}
