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
        Node newNode = new Node(key);
        while (true) {
            AtomicMarkableReference<Node> currentRef = root;
            Node current = currentRef.getReference();
            boolean[] marked = {false};

            if (current == null) {
                if (root.compareAndSet(null, newNode, false, false)) {
                    return true;
                }
                continue;
            }

            Node parent = null;
            AtomicMarkableReference<Node> parentRef = null;
            boolean isLeft = false;

            while (current != null) {
                parent = current;
                int cmp = Integer.compare(key, current.key);

                if (cmp == 0) {
                    return false;
                } else if (cmp < 0) {
                    parentRef = current.left;
                    current = parentRef.getReference();
                    isLeft = true;
                } else {
                    parentRef = current.right;
                    current = parentRef.getReference();
                    isLeft = false;
                }

                if (current != null && parentRef.isMarked()) {
                    break;
                }
            }

            if (parentRef.isMarked()) {
                continue;
            }

            if (isLeft) {
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

    @Override
    public boolean remove(int key) {
        while (true) {
            Node current = root.getReference();
            Node parent = null;
            AtomicMarkableReference<Node> parentRef = null;
            boolean isLeft = false;

            while (current != null) {
                int cmp = Integer.compare(key, current.key);
                if (cmp == 0) {
                    if (current.left.isMarked() || current.right.isMarked()) {
                        break;
                    }
                    if (current.left.compareAndSet(current.left.getReference(), current.left.getReference(), false, true)) {
                        // Node has been marked
                        helpRemove(parent, current, isLeft);
                        return true;
                    }
                    break;
                } else if (cmp < 0) {
                    parent = current;
                    parentRef = current.left;
                    current = parentRef.getReference();
                    isLeft = true;
                } else {
                    parent = current;
                    parentRef = current.right;
                    current = parentRef.getReference();
                    isLeft = false;
                }

                if (current != null && parentRef.isMarked()) {
                    break;
                }
            }
            return false;
        }
    }

    private void helpRemove(Node parent, Node target, boolean isLeft) {
        Node left = target.left.getReference();
        Node right = target.right.getReference();

        if (left == null && right == null) {
            if (parent == null) {
                root.compareAndSet(target, null, false, false);
            } else if (isLeft) {
                parent.left.compareAndSet(target, null, false, false);
            } else {
                parent.right.compareAndSet(target, null, false, false);
            }
        } else if (left == null) {
            if (parent == null) {
                root.compareAndSet(target, right, false, false);
            } else if (isLeft) {
                parent.left.compareAndSet(target, right, false, false);
            } else {
                parent.right.compareAndSet(target, right, false, false);
            }
        } else if (right == null) {
            if (parent == null) {
                root.compareAndSet(target, left, false, false);
            } else if (isLeft) {
                parent.left.compareAndSet(target, left, false, false);
            } else {
                parent.right.compareAndSet(target, left, false, false);
            }
        } else {
            Node successor = right;
            Node successorParent = target;
            boolean successorIsLeft = false;

            while (successor.left.getReference() != null) {
                successorParent = successor;
                successor = successor.left.getReference();
                successorIsLeft = true;
            }

            if (successorParent != target) {
                successorParent.left.compareAndSet(successor, successor.right.getReference(), false, false);
                successor.right = new AtomicMarkableReference<>(right, false);
            }
            successor.left = new AtomicMarkableReference<>(left, false);

            if (parent == null) {
                root.compareAndSet(target, successor, false, false);
            } else if (isLeft) {
                parent.left.compareAndSet(target, successor, false, false);
            } else {
                parent.right.compareAndSet(target, successor, false, false);
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node current = root.getReference();
        while (current != null) {
            int cmp = Integer.compare(key, current.key);
            if (cmp == 0) {
                return !current.left.isMarked() && !current.right.isMarked();
            } else if (cmp < 0) {
                current = current.left.getReference();
            } else {
                current = current.right.getReference();
            }
        }
        return false;
    }
}