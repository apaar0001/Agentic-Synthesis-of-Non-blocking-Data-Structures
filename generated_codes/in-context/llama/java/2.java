package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Lock-free binary search tree set with logical deletion.
 * Nodes use AtomicReference for child pointers and AtomicBoolean for the mark.
 * Add: unmarks a removed node if found, otherwise inserts a new node via CAS.
 * Remove: marks a node if found and unmarked.
 * Contains: returns true only for unmarked nodes.
 */
public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        final int key;
        final AtomicReference<Node> left;
        final AtomicReference<Node> right;
        final AtomicBoolean marked;

        Node(int key) {
            this.key = key;
            this.left = new AtomicReference<>(null);
            this.right = new AtomicReference<>(null);
            this.marked = new AtomicBoolean(false);
        }
    }

    private final Node root = new Node(Integer.MIN_VALUE);

    @Override
    public boolean add(int key) {
        outer: while (true) {
            Node parent = null;
            Node curr = root;
            boolean wentLeft = false;
            while (curr != null) {
                if (key == curr.key) {
                    if (curr.marked.get()) {
                        // try to unmark
                        if (curr.marked.compareAndSet(true, false)) {
                            return true;
                        } else {
                            // lost race, restart search
                            continue outer;
                        }
                    } else {
                        return false; // already present
                    }
                }
                parent = curr;
                if (key < curr.key) {
                    wentLeft = true;
                    curr = curr.left.get();
                } else {
                    wentLeft = false;
                    curr = curr.right.get();
                }
            }
            // curr is null, attempt to link new node
            Node newNode = new Node(key);
            if (wentLeft) {
                if (parent.left.compareAndSet(null, newNode)) {
                    return true;
                }
                // else retry
            } else {
                if (parent.right.compareAndSet(null, newNode)) {
                    return true;
                }
                // else retry
            }
        }
    }

    @Override
    public boolean remove(int key) {
        outer: while (true) {
            Node curr = root;
            while (curr != null) {
                if (key == curr.key) {
                    if (!curr.marked.get()) {
                        if (curr.marked.compareAndSet(false, true)) {
                            // Node has been marked
                            return true;
                        } else {
                            // lost race, restart
                            continue outer;
                        }
                    } else {
                        return false; // already removed
                    }
                }
                if (key < curr.key) {
                    curr = curr.left.get();
                } else {
                    curr = curr.right.get();
                }
            }
            return false; // not found
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