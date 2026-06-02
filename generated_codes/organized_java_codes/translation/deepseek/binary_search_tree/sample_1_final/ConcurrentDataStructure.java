package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {
    private static class Node {
        final int key;
        AtomicMarkableReference<Node> left;
        AtomicMarkableReference<Node> right;

        Node(int key) {
            this.key = key;
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
        }
    }

    private final AtomicMarkableReference<Node> root;

    public ConcurrentDataStructure() {
        root = new AtomicMarkableReference<>(null, false);
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node parent = null;
            Node current = root.getReference();
            AtomicMarkableReference<Node> childRef = root;
            boolean isLeft = true;

            while (current != null) {
                parent = current;
                if (key < current.key) {
                    childRef = current.left;
                    current = childRef.getReference();
                    isLeft = true;
                } else if (key > current.key) {
                    childRef = current.right;
                    current = childRef.getReference();
                    isLeft = false;
                } else {
                    boolean[] markHolder = new boolean[1];
                    Node node = childRef.get(markHolder);
                    if (markHolder[0]) {
                        if (isLeft) {
                            parent.left.compareAndSet(node, null, false, false);
                        } else {
                            parent.right.compareAndSet(node, null, false, false);
                        }
                        continue;
                    }
                    return false;
                }
            }

            Node newNode = new Node(key);
            if (childRef.compareAndSet(null, newNode, false, false)) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node parent = null;
            Node current = root.getReference();
            AtomicMarkableReference<Node> childRef = root;
            boolean isLeft = true;

            while (current != null) {
                if (key < current.key) {
                    parent = current;
                    childRef = current.left;
                    current = childRef.getReference();
                    isLeft = true;
                } else if (key > current.key) {
                    parent = current;
                    childRef = current.right;
                    current = childRef.getReference();
                    isLeft = false;
                } else {
                    if (childRef.attemptMark(current, true)) {
                        // Node has been marked
                        if (isLeft) {
                            parent.left.compareAndSet(current, null, true, false);
                        } else {
                            parent.right.compareAndSet(current, null, true, false);
                        }
                        return true;
                    }
                    break;
                }
            }
            return false;
        }
    }

    @Override
    public boolean contains(int key) {
        Node current = root.getReference();
        while (current != null) {
            if (key < current.key) {
                current = current.left.getReference();
            } else if (key > current.key) {
                current = current.right.getReference();
            } else {
                boolean[] markHolder = new boolean[1];
                current.left.get(markHolder);
                return !markHolder[0];
            }
        }
        return false;
    }
}