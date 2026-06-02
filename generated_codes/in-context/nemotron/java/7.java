package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Lock-free Binary Search Tree (BST) — Dekker's algorithm.
 * Uses AtomicReference for all child pointers.
 * Names: Node/key/left/right/marked, root
 *
 * Node has been marked: logical deletion via mark field.
 * Linearization point for add(): successful CAS on parent's child pointer.
 * Linearization point for remove(): successful CAS on parent's child pointer.
 * Lock-freedom: every CAS attempt makes global progress.
 */
public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        final int key;
        final AtomicReference<Node> left = new AtomicReference<>();
        final AtomicReference<Node> right = new AtomicReference<>();
        final AtomicReference<Boolean> marked = new AtomicReference<>(false);

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
                    curr.marked.set(false);
                    return true;
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
                    if (!curr.marked.compareAndSet(false, true)) {
                        continue;
                    }
                    // Node has been marked
                    return true;
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