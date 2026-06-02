package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Lock-free Binary Search Tree.
 *
 * Structure:
 * - AtomicReference<Node> root
 * - AtomicReference<Node> left/right child pointers
 * - AtomicBoolean marked for logical deletion
 *
 * add():
 * - Inserts using CAS on null child pointer
 * - If key exists but is marked deleted, CAS-unmarks it
 *
 * remove():
 * - Logical deletion using CAS on marked flag
 *
 * contains():
 * - Traverses BST and checks key + unmarked status
 *
 * No locks, synchronized blocks, or blocking primitives are used.
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

    private final AtomicReference<Node> root;

    public ConcurrentDataStructure() {
        this.root = new AtomicReference<>(null);
    }

    @Override
    public boolean add(int key) {
        Node newNode = new Node(key);

        while (true) {
            Node r = root.get();

            if (r == null) {
                if (root.compareAndSet(null, newNode)) {
                    return true;
                }
                continue;
            }

            Node curr = r;

            while (true) {
                if (key == curr.key) {
                    while (true) {
                        if (!curr.marked.get()) {
                            return false;
                        }
                        if (curr.marked.compareAndSet(true, false)) {
                            return true;
                        }
                    }
                }

                if (key < curr.key) {
                    Node leftChild = curr.left.get();

                    if (leftChild == null) {
                        Node node = new Node(key);
                        if (curr.left.compareAndSet(null, node)) {
                            return true;
                        }
                    } else {
                        curr = leftChild;
                    }
                } else {
                    Node rightChild = curr.right.get();

                    if (rightChild == null) {
                        Node node = new Node(key);
                        if (curr.right.compareAndSet(null, node)) {
                            return true;
                        }
                    } else {
                        curr = rightChild;
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
                    if (curr.marked.compareAndSet(false, true)) {
                        // Node has been marked
                        return true;
                    }

                    if (curr.marked.get()) {
                        return false;
                    }

                    break;
                }

                curr = key < curr.key ? curr.left.get() : curr.right.get();
            }

            if (curr == null) {
                return false;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node curr = root.get();

        while (curr != null) {
            if (key == curr.key) {
                return !curr.marked.get();
            }

            curr = key < curr.key ? curr.left.get() : curr.right.get();
        }

        return false;
    }
}