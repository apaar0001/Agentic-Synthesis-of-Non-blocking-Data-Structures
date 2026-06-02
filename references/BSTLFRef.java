package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Lock-free external binary search tree (simplified Ellen et al. style).
 *
 * Ground-truth reference for LockFreeBench annotation evaluation.
 *
 * Progress guarantee : LOCK_FREE
 * Algorithm : External BST — values stored in leaves; internal
 * nodes are routing nodes only. Operations use
 * CAS-based flag-and-mark deletion protocol.
 * ABA protection : AtomicReference stamps via info records (flag/mark)
 * Linearization points:
 * add() — successful CAS inserting leaf node [FIXED LP]
 * remove() — successful CAS marking parent child ref [FIXED LP]
 * contains() — read of leaf key during downward traversal [FIXED LP]
 */
public class BSTLFRef implements SetADT {

    // ---- Node types --------------------------------------------------------

    private static class Node {
        final int key;

        Node(int key) {
            this.key = key;
        }
    }

    private static final class Leaf extends Node {
        Leaf(int key) {
            super(key);
        }
    }

    private static final class Internal extends Node {
        final AtomicReference<Node> left = new AtomicReference<>();
        final AtomicReference<Node> right = new AtomicReference<>();
        // update record for deletion protocol
        final AtomicReference<Info> update = new AtomicReference<>(null);

        Internal(int key, Node left, Node right) {
            super(key);
            this.left.set(left);
            this.right.set(right);
        }
    }

    /** Deletion info record — marks an internal node for child replacement. */
    private static final class Info {
        final Internal parent;
        final Internal gp; // grandparent
        final AtomicReference<Node> childRef; // parent's child ref to update
        final Leaf leaf;
        final boolean isLeft; // which child of gp points to parent
        volatile boolean flagged = false;
        volatile boolean marked = false;

        Info(Internal gp, Internal parent, AtomicReference<Node> childRef,
                Leaf leaf, boolean isLeft) {
            this.gp = gp;
            this.parent = parent;
            this.childRef = childRef;
            this.leaf = leaf;
            this.isLeft = isLeft;
        }
    }

    // ---- Two sentinel leaves: -∞ and +∞ ------------------------------------

    private final Internal root;

    public BSTLFRef() {
        Leaf lInf = new Leaf(Integer.MIN_VALUE);
        Leaf rInf = new Leaf(Integer.MAX_VALUE);
        root = new Internal(Integer.MAX_VALUE, lInf, rInf);
    }

    // ---- Search (non-mutating) ---------------------------------------------

    /** Downward search — returns (grandparent, parent, leaf, edge reference). */
    private Object[] search(int key) {
        Internal gp = null;
        Internal parent = null;
        Node curr = root;
        AtomicReference<Node> parentRef = null;

        while (curr instanceof Internal) {
            Internal ic = (Internal) curr;
            gp = parent;
            parent = ic;
            parentRef = (key < ic.key) ? ic.left : ic.right;
            curr = (key < ic.key) ? ic.left.get() : ic.right.get();
        }
        // curr is a Leaf
        return new Object[] { gp, parent, (Leaf) curr, parentRef };
    }

    // ---- SetADT operations -------------------------------------------------

    @Override
    public boolean contains(int key) {
        Object[] r = search(key);
        Leaf leaf = (Leaf) r[2];
        // LINEARIZATION POINT: read of leaf.key
        return leaf.key == key;
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Object[] r = search(key);
            Leaf leaf = (Leaf) r[2];

            if (leaf.key == key)
                return false; // already present

            Internal parent = (Internal) r[1];
            @SuppressWarnings("unchecked")
            AtomicReference<Node> childRef = (AtomicReference<Node>) r[3];

            // Build new subtree: new internal node with two leaves
            Leaf newLeaf = new Leaf(key);
            Internal newInternal;
            if (key < leaf.key)
                newInternal = new Internal(leaf.key, newLeaf, leaf);
            else
                newInternal = new Internal(key, leaf, newLeaf);

            // LINEARIZATION POINT: CAS replaces old leaf with new internal subtree
            if (childRef.compareAndSet(leaf, newInternal))
                return true;
            // CAS failed → retry
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Object[] r = search(key);
            Leaf leaf = (Leaf) r[2];
            if (leaf.key != key)
                return false; // not present

            Internal gp = (Internal) r[0];
            Internal parent = (Internal) r[1];
            if (parent == null || gp == null)
                continue; // root edge — retry

            @SuppressWarnings("unchecked")
            AtomicReference<Node> childRef = (AtomicReference<Node>) r[3];
            boolean isLeft = (gp.left.get() == parent);

            Info info = new Info(gp, parent, childRef, leaf, isLeft);

            // Flag parent's update field
            if (!parent.update.compareAndSet(null, info))
                continue;
            info.flagged = true;

            // Determine sibling — the other child of parent
            Node sibling = (parent.left.get() == leaf)
                    ? parent.right.get()
                    : parent.left.get();

            // LINEARIZATION POINT: CAS replaces parent with sibling at grandparent
            AtomicReference<Node> gpRef = isLeft ? gp.left : gp.right;
            if (gpRef.compareAndSet(parent, sibling)) {
                info.marked = true;
                parent.update.set(null);
                return true;
            }
            // Failed — clean up flag and retry
            parent.update.compareAndSet(info, null);
        }
    }
}
