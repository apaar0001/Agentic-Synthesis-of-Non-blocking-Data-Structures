package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

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
        int key;
        AtomicMarkableReference<Node> left;
        AtomicMarkableReference<Node> right;

        Node(int key) {
            this.key = key;
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
        }
    }

    private AtomicReference<Node> root;

    public ConcurrentDataStructure() {
        this.root = new AtomicReference<>(null);
    }

    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        while (true) {
            Node current = root.get();
            if (current == null) {
                Node newNode = new Node(key);
                if (root.compareAndSet(null, newNode)) {
                    return true;
                }
            } else {
                Node parent = findParent(current, key);
                if (parent == null) {
                    return false;
                }
                Node child = key < parent.key ? parent.left.getReference() : parent.right.getReference();
                if (child != null && child.key == key) {
                    return false;
                }
                Node newNode = new Node(key);
                if (key < parent.key) {
                    if (parent.left.compareAndSet(child, newNode)) {
                        return true;
                    }
                } else {
                    if (parent.right.compareAndSet(child, newNode)) {
                        return true;
                    }
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
            Node current = root.get();
            if (current == null) {
                return false;
            }
            Node parent = findParent(current, key);
            if (parent == null) {
                return false;
            }
            Node child = key < parent.key ? parent.left.getReference() : parent.right.getReference();
            if (child == null || child.key != key) {
                return false;
            }
            if (child.left.getReference() != null && child.right.getReference() != null) {
                Node successor = findSuccessor(child);
                if (successor.key == key) {
                    if (successor.left.compareAndSet(null, child.left.getReference())) {
                        if (successor.right.compareAndSet(null, child.right.getReference())) {
                            if (key < parent.key) {
                                if (parent.left.compareAndSet(child, successor)) {
                                    return true;
                                }
                            } else {
                                if (parent.right.compareAndSet(child, successor)) {
                                    return true;
                                }
                            }
                        }
                    }
                }
            } else {
                Node replacement = child.left.getReference() != null ? child.left.getReference() : child.right.getReference();
                if (key < parent.key) {
                    if (parent.left.compareAndSet(child, replacement)) {
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
                        child.left.set(null, true);
                        child.right.set(null, true);
                        return true;
                    }
                } else {
                    if (parent.right.compareAndSet(child, replacement)) {
                        // Node has been marked
                        child.left.set(null, true);
                        child.right.set(null, true);
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
        Node current = root.get();
        while (current != null) {
            if (current.key == key) {
                return true;
            }
            current = key < current.key ? current.left.getReference() : current.right.getReference();
        }
        return false;
    }

    private Node findParent(Node current, int key) {
        if (current == null) {
            return null;
        }
        if (key < current.key) {
            if (current.left.getReference() == null) {
                return current;
            }
            return findParent(current.left.getReference(), key);
        } else if (key > current.key) {
            if (current.right.getReference() == null) {
                return current;
            }
            return findParent(current.right.getReference(), key);
        } else {
            return null;
        }
    }

    private Node findSuccessor(Node current) {
        if (current.right.getReference() == null) {
            return current;
        }
        return findSuccessor(current.right.getReference());
    }
}