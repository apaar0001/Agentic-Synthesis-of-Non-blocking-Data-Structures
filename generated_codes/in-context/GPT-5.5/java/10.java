package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Lock-free Binary Search Tree using immutable path-copying.
 *
 * Linearization points:
 * - add(): successful CAS on root
 * - remove(): successful CAS on root
 * - contains(): atomic root snapshot read
 *
 * No locks are used. Failed CAS means another thread completed an update.
 */
public class ConcurrentDataStructure implements SetADT {

    private static final class Node {
        final int key;
        final Node left;
        final Node right;

        Node(int key, Node left, Node right) {
            this.key = key;
            this.left = left;
            this.right = right;
        }
    }

    private final AtomicReference<Node> root;

    public ConcurrentDataStructure() {
        root = new AtomicReference<>(null);
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node oldRoot = root.get();

            if (contains(oldRoot, key)) {
                return false;
            }

            Node newRoot = insert(oldRoot, key);

            if (root.compareAndSet(oldRoot, newRoot)) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node oldRoot = root.get();

            if (!contains(oldRoot, key)) {
                return false;
            }

            Node newRoot = delete(oldRoot, key);

            if (root.compareAndSet(oldRoot, newRoot)) {
                // Node has been marked
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        return contains(root.get(), key);
    }

    private boolean contains(Node node, int key) {
        Node curr = node;

        while (curr != null) {
            if (key < curr.key) {
                curr = curr.left;
            } else if (key > curr.key) {
                curr = curr.right;
            } else {
                return true;
            }
        }

        return false;
    }

    private Node insert(Node node, int key) {
        if (node == null) {
            return new Node(key, null, null);
        }

        if (key < node.key) {
            return new Node(node.key, insert(node.left, key), node.right);
        }

        if (key > node.key) {
            return new Node(node.key, node.left, insert(node.right, key));
        }

        return node;
    }

    private Node delete(Node node, int key) {
        if (node == null) {
            return null;
        }

        if (key < node.key) {
            return new Node(node.key, delete(node.left, key), node.right);
        }

        if (key > node.key) {
            return new Node(node.key, node.left, delete(node.right, key));
        }

        if (node.left == null) {
            return node.right;
        }

        if (node.right == null) {
            return node.left;
        }

        Node successor = min(node.right);
        Node newRight = deleteMin(node.right);

        return new Node(successor.key, node.left, newRight);
    }

    private Node min(Node node) {
        Node curr = node;

        while (curr.left != null) {
            curr = curr.left;
        }

        return curr;
    }

    private Node deleteMin(Node node) {
        if (node.left == null) {
            return node.right;
        }

        return new Node(node.key, deleteMin(node.left), node.right);
    }
}