package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        int key;
        AtomicMarkableReference<Node> left;
        AtomicMarkableReference<Node> right;
        AtomicBoolean marked;

        Node(int key) {
            this.key = key;
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
            this.marked = new AtomicBoolean(false);
        }
    }

    private final AtomicMarkableReference<Node> root = new AtomicMarkableReference<>(null, false);

    @Override
    public boolean add(int key) {
        Node newNode = new Node(key);
        while (true) {
            AtomicMarkableReference<Node> parentRef = root;
            Node curr = root.getReference();
            while (curr != null) {
                // Help remove any logically deleted left child
                Node leftChild = curr.left.getReference();
                boolean[] leftMark = new boolean[1];
                curr.left.get(leftMark);
                if (leftMark[0]) {
                    helpRemove(curr.left, true, leftChild);
                }
                // Help remove any logically deleted right child
                Node rightChild = curr.right.getReference();
                boolean[] rightMark = new boolean[1];
                curr.right.get(rightMark);
                if (rightMark[0]) {
                    helpRemove(curr.right, false, rightChild);
                }

                int cmp = Integer.compare(key, curr.key);
                if (cmp == 0) {
                    return false; // duplicate
                } else if (cmp < 0) {
                    parentRef = curr.left;
                    curr = leftChild;
                } else {
                    parentRef = curr.right;
                    curr = rightChild;
                }
            }
            // Attempt to link newNode
            if (parentRef.compareAndSet(null, newNode, false, false)) {
                return true;
            }
            // else retry
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            AtomicMarkableReference<Node> parentRef = root;
            boolean isLeft = false;
            Node curr = root.getReference();
            while (curr != null) {
                // Help remove any logically deleted children before proceeding
                Node leftChild = curr.left.getReference();
                boolean[] leftMark = new boolean[1];
                curr.left.get(leftMark);
                if (leftMark[0]) {
                    helpRemove(curr.left, true, leftChild);
                }
                Node rightChild = curr.right.getReference();
                boolean[] rightMark = new boolean[1];
                curr.right.get(rightMark);
                if (rightMark[0]) {
                    helpRemove(curr.right, false, rightChild);
                }

                int cmp = Integer.compare(key, curr.key);
                if (cmp == 0) {
                    if (curr.marked.get()) {
                        // Already logically deleted, help physical removal
                        return helpRemove(parentRef, isLeft, curr);
                    }
                    // Attempt logical deletion
                    if (curr.marked.compareAndSet(false, true)) {
                        // Node has been marked
                        return helpRemove(parentRef, isLeft, curr);
                    }
                    // Lost race to mark, retry search
                    break;
                } else if (cmp < 0) {
                    parentRef = curr.left;
                    isLeft = true;
                    curr = leftChild;
                } else {
                    parentRef = curr.right;
                    isLeft = false;
                    curr = rightChild;
                }
            }
            // Key not found
            return false;
        }
    }

    @Override
    public boolean contains(int key) {
        Node curr = root.getReference();
        while (curr != null) {
            if (curr.marked.get()) {
                // Logically deleted node, treat as absent
                return false;
            }
            int cmp = Integer.compare(key, curr.key);
            if (cmp == 0) {
                return !curr.marked.get();
            } else if (cmp < 0) {
                curr = curr.left.getReference();
            } else {
                curr = curr.right.getReference();
            }
        }
        return false;
    }

    /** Helper to physically remove a logically deleted node from its parent reference. */
    private boolean helpRemove(AtomicMarkableReference<Node> parentRef, boolean isLeftChild, Node node) {
        Node left = node.left.getReference();
        Node right = node.right.getReference();
        Node replacement = (left != null) ? left : right; // may be null
        // Expect parentRef to point to node unmarked
        return parentRef.compareAndSet(node, replacement, false, false);
    }
}