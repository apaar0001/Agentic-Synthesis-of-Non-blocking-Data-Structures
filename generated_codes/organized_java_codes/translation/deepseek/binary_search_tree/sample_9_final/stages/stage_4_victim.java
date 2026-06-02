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
        Node newNode = new Node(key);
        while (true) {
            Node current = root.get();
            if (current == null) {
                if (root.compareAndSet(null, newNode)) {
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
            Node parent = null;
            Node current = root.get();
            AtomicMarkableReference<Node> childRef = root;

            while (current != null) {
                boolean[] marked = {false};
                Node left = current.left.get(marked);
                if (marked[0]) {
                    helpRemove(parent, current);
                    return remove(key);
                }

                if (key == current.key) {
                    if (current.left.compareAndSet(left, left, false, true)) {
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
                        helpRemove(parent, current);
                        return true;
                    }
                    continue;
                }

                parent = current;
                if (key < current.key) {
                    childRef = current.left;
                    current = left;
                } else {
                    childRef = current.right;
                    current = current.right.getReference();
                }
            }
            return false;
        }
    }

    private void helpRemove(Node parent, Node target) {
        Node left = target.left.getReference();
        Node right = target.right.getReference();

        if (left == null && right == null) {
            if (parent == null) {
                root.compareAndSet(target, null);
            } else if (parent.left.getReference() == target) {
                parent.left.compareAndSet(target, null, false, false);
            } else if (parent.right.getReference() == target) {
                parent.right.compareAndSet(target, null, false, false);
            }
        } else if (left != null && right == null) {
            if (parent == null) {
                root.compareAndSet(target, left);
            } else if (parent.left.getReference() == target) {
                parent.left.compareAndSet(target, left, false, false);
            } else if (parent.right.getReference() == target) {
                parent.right.compareAndSet(target, left, false, false);
            }
        } else if (left == null && right != null) {
            if (parent == null) {
                root.compareAndSet(target, right);
            } else if (parent.left.getReference() == target) {
                parent.left.compareAndSet(target, right, false, false);
            } else if (parent.right.getReference() == target) {
                parent.right.compareAndSet(target, right, false, false);
            }
        } else {
            Node successor = right;
            Node successorParent = target;
            while (successor.left.getReference() != null) {
                successorParent = successor;
                successor = successor.left.getReference();
            }

            if (successorParent != target) {
                successorParent.left.set(successor.right.getReference(), false);
                successor.right.set(right, false);
            }
            successor.left.set(left, false);

            if (parent == null) {
                root.compareAndSet(target, successor);
            } else if (parent.left.getReference() == target) {
                parent.left.compareAndSet(target, successor, false, false);
            } else if (parent.right.getReference() == target) {
                parent.right.compareAndSet(target, successor, false, false);
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
            boolean[] marked = {false};
            Node left = current.left.get(marked);
            if (marked[0]) {
                current = root.get();
                continue;
            }

            if (key == current.key) {
                return true;
            } else if (key < current.key) {
                current = left;
            } else {
                current = current.right.getReference();
            }
        }
        return false;
    }
}