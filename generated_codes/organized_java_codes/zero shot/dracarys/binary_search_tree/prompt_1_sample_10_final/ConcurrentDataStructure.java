package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        int key;
        AtomicReference<Node> left;
        AtomicReference<Node> right;
        AtomicMarkableReference<Node> marked;

        public Node(int key) {
            this.key = key;
            this.left = new AtomicReference<>();
            this.right = new AtomicReference<>();
            this.marked = new AtomicMarkableReference<>(null, false);
        }
    }

    private AtomicReference<Node> root;

    public ConcurrentDataStructure() {
        root = new AtomicReference<>();
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node current = root.get();
            if (current == null) {
                Node newNode = new Node(key);
                if (root.compareAndSet(null, newNode)) {
                    return true;
                }
            } else {
                if (current.marked.getReference() != null) {
                    continue;
                }

                if (key < current.key) {
                    Node leftChild = current.left.get();
                    if (leftChild == null) {
                        Node newNode = new Node(key);
                        if (current.left.compareAndSet(null, newNode)) {
                            return true;
                        }
                    } else {
                        current = leftChild;
                    }
                } else {
                    Node rightChild = current.right.get();
                    if (rightChild == null) {
                        Node newNode = new Node(key);
                        if (current.right.compareAndSet(null, newNode)) {
                            return true;
                        }
                    } else {
                        current = rightChild;
                    }
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node current = root.get();
            if (current == null) {
                return false;
            }

            if (current.marked.getReference() != null) {
                continue;
            }

            if (key < current.key) {
                Node leftChild = current.left.get();
                if (leftChild == null) {
                    return false;
                } else {
                    current = leftChild;
                }
            } else if (key > current.key) {
                Node rightChild = current.right.get();
                if (rightChild == null) {
                    return false;
                } else {
                    current = rightChild;
                }
            } else {
                if (current.marked.attemptMark(null, true)) {
                    // Node has been marked
                    return true;
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        while (true) {
            Node current = root.get();
            if (current == null) {
                return false;
            }

            if (current.marked.getReference() != null) {
                continue;
            }

            if (key < current.key) {
                Node leftChild = current.left.get();
                if (leftChild == null) {
                    return false;
                } else {
                    current = leftChild;
                }
            } else if (key > current.key) {
                Node rightChild = current.right.get();
                if (rightChild == null) {
                    return false;
                } else {
                    current = rightChild;
                }
            } else {
                return true;
            }
        }
    }
}