package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Lock-free BST with Sentinel Node.
 *
 * Variant characteristics vs ref2 (BSTLazyDeletion):
 * - Uses AtomicBoolean (not AtomicReference<Boolean>) for the deletion flag.
 * - A dedicated sentinel root wrapper (AtomicReference<Node>) holds the real
 * root, avoiding the MIN_VALUE dummy-head issue.
 * - Deletion flag semantics: marked=true → node is logically deleted.
 *
 * Nodes are never physically removed. Lock-free: all ops are CAS-only.
 */
public class BSTSentinelNode implements SetADT {

    private static class Node {
        final int key;
        final AtomicBoolean marked; // true = logically deleted
        final AtomicReference<Node> left, right;

        Node(int k) {
            key = k;
            marked = new AtomicBoolean(false);
            left = new AtomicReference<>(null);
            right = new AtomicReference<>(null);
        }
    }

    // Sentinel: just an AtomicReference wrapper (no dummy key node)
    private final AtomicReference<Node> root = new AtomicReference<>(null);

    public BSTSentinelNode() {
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node n = new Node(key);
            Node r = root.get();
            if (r == null) {
                if (root.compareAndSet(null, n))
                    return true;
                continue;
            }
            Node c = r;
            while (true) {
                if (c.key == key) {
                    // Revive a logically-deleted node
                    if (c.marked.compareAndSet(true, false))
                        return true;
                    return false; // Already active
                }
                AtomicReference<Node> next = (key < c.key) ? c.left : c.right;
                Node m = next.get();
                if (m == null) {
                    if (next.compareAndSet(null, n))
                        return true;
                    // CAS failed: another thread inserted; re-read and continue
                    m = next.get();
                    if (m == null)
                        continue; // still null? retry
                }
                c = m;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        Node c = root.get();
        while (c != null) {
            if (c.key == key) {
                if (c.marked.compareAndSet(false, true)) {
                    // Node has been marked
                    return true;
                }
                return false; // Already deleted
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
                return !c.marked.get();
            c = (key < c.key) ? c.left.get() : c.right.get();
        }
        return false;
    }
}
