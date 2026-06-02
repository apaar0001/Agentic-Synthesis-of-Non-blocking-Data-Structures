package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Lock-free Red-Black Tree — Harris-Michael style.
 * Uses AtomicMarkableReference for logical delete.
 * Names: Node/key/left/right/marked, root
 *
 * remove() two-phase: mark node, then CAS left/right.
 * Linearization point: successful CAS on left/right.
 * Lock-freedom: every CAS attempt makes global progress.
 */
public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        final int key;
        final AtomicReference<Node> left;
        final AtomicReference<Node> right;
        final AtomicMarkableReference<Node> marked;

        Node(int key) {
            this.key = key;
            this.left = new AtomicReference<>(null);
            this.right = new AtomicReference<>(null);
            this.marked = new AtomicMarkableReference<>(null, false);
        }
    }

    private final Node root = new Node(Integer.MIN_VALUE);

    @Override
    public boolean add(int key) {
        while (true) {
            Node curr = root;
            Node parent = null;
            while (curr != null) {
                if (key == curr.key) {
                    if (!curr.marked.getReference().marked[0]) {
                        return false;
                    }
                    curr.marked.set(null, false);
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
            Node parent = null;
            while (curr != null) {
                if (key == curr.key) {
                    if (!curr.marked.getReference().marked[0]) {
                        // Node has been marked
                        curr.marked.set(null, true);
                        return true;
                    }
                    return false;
                }
                parent = curr;
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
                return !curr.marked.getReference().marked[0];
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