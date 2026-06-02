package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    // Lock-freedom test helpers (auto-injected)
    private static final java.util.concurrent.atomic.AtomicBoolean _lfVictimChosen = new java.util.concurrent.atomic.AtomicBoolean(false);
    private static final ThreadLocal<Integer> _lfOpCount = ThreadLocal.withInitial(() -> 0);
    private static final ThreadLocal<Boolean> _lfRetired =
            ThreadLocal.withInitial(() -> false);

    /**
     * Decide if the *current* thread should become the victim.
     *
     * Each call increments a per-thread operation counter. Once a thread
     * has executed more than 100 operations and no victim has been chosen,
     * it atomically claims the victim role and will then stall.
     */
    private static boolean _lfShouldStall() {
        int c = _lfOpCount.get() + 1;
        _lfOpCount.set(c);
        if (c > 100 && !_lfVictimChosen.get() && _lfVictimChosen.compareAndSet(false, true)) {
            return true;
        }
        return false;
    }

    private static class Node {
        final int key;
        final AtomicMarkableReference<Node> left;
        final AtomicMarkableReference<Node> right;

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
        if (_lfRetired.get()) {
            return false;
        }
        while (true) {
            Node current = root.getReference();
            if (current == null) {
                Node newNode = new Node(key);
                if (root.compareAndSet(null, newNode, false, false)) {
                    return true;
                }
                continue;
            }

            Node parent = null;
            Node grandParent = null;
            Node[] parentInfo = new Node[2];
            Node[] childInfo = new Node[1];
            boolean[] marked = new boolean[1];

            if (!find(key, parentInfo, childInfo, marked)) {
                return false;
            }
            parent = parentInfo[0];
            grandParent = parentInfo[1];
            Node child = childInfo[0];

            if (child != null) {
                return false;
            }

            Node newNode = new Node(key);
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

    @Override
    public boolean remove(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        while (true) {
            Node[] parentInfo = new Node[2];
            Node[] childInfo = new Node[1];
            boolean[] marked = new boolean[1];

            if (!find(key, parentInfo, childInfo, marked)) {
                return false;
            }
            Node parent = parentInfo[0];
            Node grandParent = parentInfo[1];
            Node child = childInfo[0];

            if (child == null) {
                return false;
            }

            Node leftChild = child.left.getReference();
            Node rightChild = child.right.getReference();

            if (leftChild == null && rightChild == null) {
                if (child == parent.left.getReference()) {
                    if (parent.left.compareAndSet(child, null, false, false)) {
                        // Node has been marked
            // Lock-freedom victim stall injection (auto-injected)
            if (_lfShouldStall()) {
                System.err.println("LOG: Victim thread stalling inside remove()");
                try {
                    Thread.sleep(10_000);
                } catch (InterruptedException ignored) {
                }
                System.err.println("LOG: Victim resumed and retiring");
                _lfRetired.set(true);
                return false;
            }
                        return true;
                    }
                } else {
                    if (parent.right.compareAndSet(child, null, false, false)) {
                        // Node has been marked
                        return true;
                    }
                }
            } else if (leftChild == null) {
                if (child == parent.left.getReference()) {
                    if (parent.left.compareAndSet(child, rightChild, false, false)) {
                        // Node has been marked
                        return true;
                    }
                } else {
                    if (parent.right.compareAndSet(child, rightChild, false, false)) {
                        // Node has been marked
                        return true;
                    }
                }
            } else if (rightChild == null) {
                if (child == parent.left.getReference()) {
                    if (parent.left.compareAndSet(child, leftChild, false, false)) {
                        // Node has been marked
                        return true;
                    }
                } else {
                    if (parent.right.compareAndSet(child, leftChild, false, false)) {
                        // Node has been marked
                        return true;
                    }
                }
            } else {
                Node successor = rightChild;
                Node successorParent = child;
                while (successor.left.getReference() != null) {
                    successorParent = successor;
                    successor = successor.left.getReference();
                }

                if (successorParent != child) {
                    successorParent.left.set(successor.right.getReference(), false);
                    successor.right.set(rightChild, false);
                }
                successor.left.set(leftChild, false);

                if (child == parent.left.getReference()) {
                    if (parent.left.compareAndSet(child, successor, false, false)) {
                        // Node has been marked
                        return true;
                    }
                } else {
                    if (parent.right.compareAndSet(child, successor, false, false)) {
                        // Node has been marked
                        return true;
                    }
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        Node current = root.getReference();
        while (current != null) {
            if (key == current.key) {
                return true;
            } else if (key < current.key) {
                current = current.left.getReference();
            } else {
                current = current.right.getReference();
            }
        }
        return false;
    }

    private boolean find(int key, Node[] parentInfo, Node[] childInfo, boolean[] marked) {
        Node parent = null;
        Node grandParent = null;
        Node current = root.getReference();

        while (current != null) {
            if (key == current.key) {
                childInfo[0] = current;
                parentInfo[0] = parent;
                parentInfo[1] = grandParent;
                return true;
            }

            grandParent = parent;
            parent = current;

            if (key < current.key) {
                current = current.left.getReference();
            } else {
                current = current.right.getReference();
            }
        }

        childInfo[0] = null;
        parentInfo[0] = parent;
        parentInfo[1] = grandParent;
        return true;
    }
}