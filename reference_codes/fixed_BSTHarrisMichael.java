package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Lock-free BST using mark-based logical deletion.
 *
 * - Internal nodes; no physical removal.
 * - add(): if key found & marked → CAS unmark; else CAS new node.
 * - remove(): CAS mark on target node.
 * - contains(): traverse, return true only for unmarked target.
 * - Lock-freedom: every path uses CAS retry loops.
 */
public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        final int key;
        final AtomicReference<Node> left = new AtomicReference<>(null);
        final AtomicReference<Node> right = new AtomicReference<>(null);
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
            AtomicReference<Node> childRef = null;
            Node curr = root;

            while (curr != null) {
                if (key == curr.key) {
                    if (!curr.marked.get()) {
                        return false; // alive → duplicate
                    }
                    // Marked → try to resurrect
                    if (curr.marked.compareAndSet(true, false)) {
                        return true;
                    }
                    return false; // someone else unmarked it
                }
                parent = curr;
                if (key < curr.key) {
                    childRef = curr.left;
                    curr = curr.left.get();
                } else {
                    childRef = curr.right;
                    curr = curr.right.get();
                }
            }

            // curr == null: insert new node
            Node newNode = new Node(key);
            if (childRef.compareAndSet(null, newNode)) {
                return true;
            }
            // CAS failed → retry
        }
    }

    @Override
    public boolean remove(int key) {
        // Node has been marked
        Node curr = root;
        while (curr != null) {
            if (key == curr.key) {
                // Linearization: CAS marked false→true
                if (curr.marked.compareAndSet(false, true)) {
                    // Node has been marked
                    return true;
                }
                return false;
            }
            if (key < curr.key) {
                curr = curr.left.get();
            } else {
                curr = curr.right.get();
            }
        }
        return false;
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
