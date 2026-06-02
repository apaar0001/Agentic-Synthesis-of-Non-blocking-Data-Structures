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
        final int key;
        AtomicMarkableReference<Node> left;
        AtomicMarkableReference<Node> right;

        Node(int key) {
            this.key = key;
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
        }
    }

    private final AtomicReference<Node> root;

    public ConcurrentDataStructure() {
        root = new AtomicReference<>(null);
    }

    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        while (true) {
            Node current = root.get();
            if (current == null) {
                if (root.compareAndSet(null, new Node(key))) {
                    return true;
                }
                continue;
            }

            Node parent = null;
            Node child = current;
            AtomicMarkableReference<Node> childRef = null;

            while (child != null) {
                parent = child;
                if (key == child.key) {
                    boolean[] marked = {false};
                    Node next = child.left.get(marked);
                    if (marked[0]) {
                        helpRemove(parent, child);
                        return add(key);
                    }
                    return false;
                } else if (key < child.key) {
                    childRef = child.left;
                } else {
                    childRef = child.right;
                }

                boolean[] marked = {false};
                Node next = childRef.get(marked);
                if (marked[0]) {
                    helpRemove(parent, child);
                    return add(key);
                }
                child = next;
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
            Node current = root.get();
            if (current == null) {
                return false;
            }

            Node parent = null;
            Node child = current;
            AtomicMarkableReference<Node> childRef = null;

            while (child != null) {
                if (key == child.key) {
                    if (child.left.attemptMark(child, true)) {
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
                        helpRemove(parent, child);
                        return true;
                    }
                    return remove(key);
                }

                parent = child;
                if (key < child.key) {
                    childRef = child.left;
                } else {
                    childRef = child.right;
                }

                boolean[] marked = {false};
                Node next = childRef.get(marked);
                if (marked[0]) {
                    helpRemove(parent, child);
                    return remove(key);
                }
                child = next;
            }
            return false;
        }
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        Node current = root.get();
        while (current != null) {
            if (key == current.key) {
                boolean[] marked = {false};
                current.left.get(marked);
                return !marked[0];
            } else if (key < current.key) {
                boolean[] marked = {false};
                Node next = current.left.get(marked);
                if (marked[0]) {
                    helpRemove(null, current);
                    current = root.get();
                } else {
                    current = next;
                }
            } else {
                boolean[] marked = {false};
                Node next = current.right.get(marked);
                if (marked[0]) {
                    helpRemove(null, current);
                    current = root.get();
                } else {
                    current = next;
                }
            }
        }
        return false;
    }

    private void helpRemove(Node parent, Node target) {
        if (parent == null) {
            boolean[] marked = {false};
            Node left = target.left.get(marked);
            Node right = target.right.get(marked);
            if (right == null) {
                if (root.compareAndSet(target, left)) {
                    return;
                }
            } else {
                Node successor = findSuccessor(target);
                if (successor != null) {
                    successor.left.set(target.left.getReference(), false);
                    if (root.compareAndSet(target, successor)) {
                        return;
                    }
                }
            }
        } else {
            boolean[] marked = {false};
            Node left = target.left.get(marked);
            Node right = target.right.get(marked);

            if (right == null) {
                if (target.key < parent.key) {
                    parent.left.compareAndSet(target, left, false, false);
                } else {
                    parent.right.compareAndSet(target, left, false, false);
                }
            } else {
                Node successor = findSuccessor(target);
                if (successor != null) {
                    successor.left.set(target.left.getReference(), false);
                    if (target.key < parent.key) {
                        parent.left.compareAndSet(target, successor, false, false);
                    } else {
                        parent.right.compareAndSet(target, successor, false, false);
                    }
                }
            }
        }
    }

    private Node findSuccessor(Node node) {
        Node current = node.right.getReference();
        Node parent = node;
        while (current != null) {
            boolean[] marked = {false};
            Node left = current.left.get(marked);
            if (marked[0]) {
                helpRemove(parent, current);
                current = node.right.getReference();
                parent = node;
            } else if (left == null) {
                return current;
            } else {
                parent = current;
                current = left;
            }
        }
        return null;
    }
}