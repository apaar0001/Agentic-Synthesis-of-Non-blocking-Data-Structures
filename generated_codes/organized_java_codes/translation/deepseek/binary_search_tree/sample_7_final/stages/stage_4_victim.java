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
        if (_lfRetired.get()) {
            return false;
        }
        while (true) {
            Node current = root.getReference();
            boolean[] marked = {false};
            Node newChild = new Node(key);

            if (current == null) {
                if (root.compareAndSet(null, newChild, false, false)) {
                    return true;
                }
                continue;
            }

            Node parent = null;
            Node child = current;
            AtomicMarkableReference<Node> childRef = root;

            while (child != null) {
                if (child.key == key) {
                    boolean[] childMarked = {false};
                    Node childNode = childRef.get(childMarked);
                    if (childMarked[0]) {
                        helpRemove(parent, child, childRef);
                        return add(key);
                    }
                    return false;
                }

                parent = child;
                if (key < child.key) {
                    childRef = child.left;
                } else {
                    childRef = child.right;
                }
                child = childRef.getReference();
            }

            if (key < parent.key) {
                if (parent.left.compareAndSet(null, newChild, false, false)) {
                    return true;
                }
            } else {
                if (parent.right.compareAndSet(null, newChild, false, false)) {
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
            Node parent = null;
            Node current = root.getReference();
            AtomicMarkableReference<Node> currentRef = root;
            boolean[] parentMarked = {false};

            while (current != null) {
                boolean[] currentMarked = {false};
                Node currNode = currentRef.get(currentMarked);
                if (currentMarked[0]) {
                    helpRemove(parent, current, currentRef);
                    return remove(key);
                }

                if (current.key == key) {
                    if (currentRef.attemptMark(current, true)) {
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
                        helpRemove(parent, current, currentRef);
                        return true;
                    }
                    continue;
                }

                parent = current;
                parentMarked[0] = currentMarked[0];
                if (key < current.key) {
                    currentRef = current.left;
                } else {
                    currentRef = current.right;
                }
                current = currentRef.getReference();
            }
            return false;
        }
    }

    private void helpRemove(Node parent, Node child, AtomicMarkableReference<Node> childRef) {
        Node left = child.left.getReference();
        Node right = child.right.getReference();

        if (left == null || right == null) {
            Node newChild = (left != null) ? left : right;
            if (parent == null) {
                root.compareAndSet(child, newChild, false, false);
            } else if (parent.left.getReference() == child) {
                parent.left.compareAndSet(child, newChild, false, false);
            } else {
                parent.right.compareAndSet(child, newChild, false, false);
            }
        } else {
            Node successorParent = child;
            Node successor = right;
            AtomicMarkableReference<Node> successorRef = child.right;
            boolean[] marked = {false};

            while (successor.left.getReference() != null) {
                successorParent = successor;
                successorRef = successor.left;
                successor = successorRef.getReference();
            }

            if (successorRef.attemptMark(successor, true)) {
                // Node has been marked
                Node succRight = successor.right.getReference();
                if (successorParent != child) {
                    successorParent.left.compareAndSet(successor, succRight, false, false);
                    successor.right.compareAndSet(right, right, false, false);
                }

                successor.left.compareAndSet(null, left, false, false);
                successor.right.compareAndSet(null, right, false, false);

                if (parent == null) {
                    root.compareAndSet(child, successor, false, false);
                } else if (parent.left.getReference() == child) {
                    parent.left.compareAndSet(child, successor, false, false);
                } else {
                    parent.right.compareAndSet(child, successor, false, false);
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
            boolean[] marked = {false};
            Node currNode = current;
            if (currNode.key == key) {
                if (marked[0]) {
                    return false;
                }
                return true;
            }
            if (key < current.key) {
                current = current.left.getReference();
            } else {
                current = current.right.getReference();
            }
        }
        return false;
    }
}