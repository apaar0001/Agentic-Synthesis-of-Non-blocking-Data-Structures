package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Lock-free BST-style concurrent set.
 *
 * Structure:
 * - Internal binary-search-tree nodes
 * - AtomicReference<Node> left/right child pointers
 * - AtomicBoolean deleted for logical deletion
 *
 * add():
 * - CAS root if empty
 * - CAS parent child pointer if insertion position is empty
 * - If deleted node with same key exists, CAS deleted true -> false
 *
 * remove():
 * - Logical deletion using CAS on deleted flag
 *
 * contains():
 * - BST search and deleted check
 *
 * No locks, synchronized blocks, or blocking primitives.
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
        this.root = new AtomicReference<>(null);
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node r = root.get();

            if (r == null) {
                Node node = new Node(key);
                if (root.compareAndSet(null, node))
                    return true;
                continue;
            }

            Node curr = r;

            while (true) {
                if (key == curr.key) {
                    while (true) {
                        if (!curr.deleted.get())
                            return false;
                        if (curr.deleted.compareAndSet(true, false))
                            return true;
                    }
                }

                if (key < curr.key) {
                    Node left = curr.left.get();
                    if (left == null) {
                        Node node = new Node(key);
                        if (curr.left.compareAndSet(null, node))
                            return true;
                        curr = curr.left.get();
                    } else {
                        curr = left;
                    }
                } else {
                    Node right = curr.right.get();
                    if (right == null) {
                        Node node = new Node(key);
                        if (curr.right.compareAndSet(null, node))
                            return true;
                        curr = curr.right.get();
                    } else {
                        curr = right;
                    }
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node curr = root.get();

            while (curr != null) {
                if (key == curr.key) {
                    while (true) {
                        if (curr.deleted.get())
                            return false;
                        if (curr.deleted.compareAndSet(false, true))
                            // Node has been marked
                            return true;
                    }
                }

                if (key < curr.key)
                    curr = curr.left.get();
                else
                    curr = curr.right.get();
            }

            return false;
        }
    }

    @Override
    public boolean contains(int key) {
        Node curr = root.get();

        while (curr != null) {
            if (key == curr.key)
                return !curr.deleted.get();

            if (key < curr.key)
                curr = curr.left.get();
            else
                curr = curr.right.get();
        }

        return false;
    }
}