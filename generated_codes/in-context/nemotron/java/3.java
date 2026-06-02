package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Lock-free Binary Search Tree — using logical deletion.
 *
 * Algorithm: Brown (2014) — uses AtomicReference and AtomicBoolean
 * Names: Node/key/left/right/marked, root
 * Linearization point for add(): successful CAS on parent's child pointer
 * Linearization point for remove(): successful CAS on node's marked field
 *
 * Lock-freedom: every CAS attempt makes global progress.
 */
public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        final int key;
        final AtomicReference<Node> left = new AtomicReference<>();
        final AtomicReference<Node> right = new AtomicReference<>();
        final AtomicBoolean marked = new AtomicBoolean(false);

        Node(int key) {
            this.key = key;
        }
    }

    private final Node root = new Node(Integer.MIN_VALUE);

    @Override
    public boolean add(int key) {
        while (true) {
            Node parent = null;
            Node curr = root;

            while (curr != null) {
                if (key == curr.key) {
                    if (!curr.marked.get()) {
                        return false;
                    }
                    if (curr.marked.compareAndSet(true, false)) {
                        return true;
                    }
                }
                parent = curr;
                if (key < curr.key) {
                    curr = curr.left.get();
                } else {
                    curr = curr.right.get();
                }
            }

            Node newNode = new Node(key);
            if (key < parent.key) {
                if (parent.left.compareAndSet(null, newNode)) {
                    return true;
                }
            } else {
                if (parent.right.compareAndSet(null, newNode)) {
                    return true;
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node curr = root;
            while (curr != null) {
                if (key == curr.key) {
                    if (curr.marked.get()) {
                        return false;
                    }
                    if (curr.marked.compareAndSet(false, true)) {
                        // Node has been marked
                        return true;
                    }
                }
                if (key < curr.key) {
                    curr = curr.left.get();
                } else {
                    curr = curr.right.get();
                }
            }
            return false;
        }
    }

    @Override
    public boolean contains(int key) {
        Node curr = root;
        while (curr != null) {
            if (key == curr.key) {
                return !curr.marked.get();
            }
            if (key < curr.key) {
                curr = curr.left.get();
            } else {
                curr = curr.right.get();
            }
        }
        return false;
    }
}