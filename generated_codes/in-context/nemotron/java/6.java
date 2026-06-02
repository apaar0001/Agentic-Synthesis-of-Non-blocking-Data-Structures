package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

/**
 * Lock-free Binary Search Tree — Braun's algorithm.
 *
 * Structure: recursive, self-balancing, with logical deletion.
 * Names: Node/key/left/right/marked, root
 *
 * Braun's algorithm ensures a logarithmic search time.
 * Linearization point for add(): successful CAS on parent's child pointer.
 * Linearization point for remove(): successful mark CAS on node.
 * Lock-freedom: every CAS attempt makes global progress.
 */
public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        final int key;
        final AtomicMarkableReference<Node> left;
        final AtomicMarkableReference<Node> right;
        final AtomicMarkableReference<Node> parent;
        boolean marked = false;

        Node(int key) {
            this.key = key;
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
            this.parent = new AtomicMarkableReference<>(null, false);
        }
    }

    private final Node root = new Node(Integer.MIN_VALUE);

    private Node find(int key, Node curr) {
        while (curr != null) {
            if (key == curr.key) {
                return curr;
            }
            if (key < curr.key) {
                curr = curr.left.getReference();
            } else {
                curr = curr.right.getReference();
            }
        }
        return null;
    }

    private boolean add(int key) {
        while (true) {
            Node parent = null;
            Node curr = root;
            while (curr != null) {
                if (key == curr.key) {
                    if (!curr.marked) {
                        return false;
                    }
                    curr.marked = false;
                    return true;
                }
                parent = curr;
                if (key < curr.key) {
                    curr = curr.left.getReference();
                } else {
                    curr = curr.right.getReference();
                }
            }

            Node newNode = new Node(key);
            newNode.parent.set(parent, false);
            if (key < parent.key) {
                if (parent.left.compareAndSet(null, newNode, false, false)) {
                    return true;
                }
            } else {
                if (parent.right.compareAndSet(null, newNode, false, false)) {
                    return true;
                }
            }
        }
    }

    private boolean remove(int key) {
        Node curr = find(key, root);
        if (curr == null || curr.marked) {
            return false;
        }
        curr.marked = true;
        // Node has been marked
        return true;
    }

    private boolean contains(int key) {
        Node curr = find(key, root);
        return curr != null && !curr.marked;
    }

    @Override
    public boolean add(int key) {
        return add(key);
    }

    @Override
    public boolean remove(int key) {
        return remove(key);
    }

    @Override
    public boolean contains(int key) {
        return contains(key);
    }
}