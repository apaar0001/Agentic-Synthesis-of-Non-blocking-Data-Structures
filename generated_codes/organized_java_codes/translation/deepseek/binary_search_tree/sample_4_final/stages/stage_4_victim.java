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
        Node newNode = new Node(key);
        while (true) {
            Node current = root.getReference();
            if (current == null) {
                if (root.compareAndSet(null, newNode, false, false)) {
                    return true;
                }
                continue;
            }
            Node parent = null;
            Node child = current;
            AtomicMarkableReference<Node> childRef = root;
            while (child != null) {
                parent = child;
                if (key == child.key) {
                    boolean[] marked = {false};
                    childRef.get(marked);
                    if (marked[0]) {
                        continue;
                    }
                    return false;
                } else if (key < child.key) {
                    childRef = child.left;
                    child = childRef.getReference();
                } else {
                    childRef = child.right;
                    child = childRef.getReference();
                }
                if (child != null) {
                    boolean[] marked = {false};
                    childRef.get(marked);
                    if (marked[0]) {
                        helpRemove(parent, childRef, child);
                        break;
                    }
                }
            }
            if (child == null) {
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
            while (current != null) {
                if (key == current.key) {
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
                        helpRemove(parent, currentRef, current);
                        return true;
                    }
                    break;
                } else if (key < current.key) {
                    parent = current;
                    currentRef = current.left;
                    current = currentRef.getReference();
                } else {
                    parent = current;
                    currentRef = current.right;
                    current = currentRef.getReference();
                }
                if (current != null) {
                    boolean[] marked = {false};
                    currentRef.get(marked);
                    if (marked[0]) {
                        helpRemove(parent, currentRef, current);
                        break;
                    }
                }
            }
            if (current == null) {
                return false;
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
                boolean[] marked = {false};
                root.get(marked);
                return !marked[0];
            } else if (key < current.key) {
                current = current.left.getReference();
            } else {
                current = current.right.getReference();
            }
            if (current != null) {
                boolean[] marked = {false};
                if (current.left.get(marked) || current.right.get(marked)) {
                    if (marked[0]) {
                        continue;
                    }
                }
            }
        }
        return false;
    }

    private void helpRemove(Node parent, AtomicMarkableReference<Node> childRef, Node child) {
        Node left = child.left.getReference();
        Node right = child.right.getReference();
        if (left == null && right == null) {
            if (parent == null) {
                root.compareAndSet(child, null, true, false);
            } else if (parent.left.getReference() == child) {
                parent.left.compareAndSet(child, null, true, false);
            } else {
                parent.right.compareAndSet(child, null, true, false);
            }
        } else if (left != null && right == null) {
            if (parent == null) {
                root.compareAndSet(child, left, true, false);
            } else if (parent.left.getReference() == child) {
                parent.left.compareAndSet(child, left, true, false);
            } else {
                parent.right.compareAndSet(child, left, true, false);
            }
        } else if (left == null && right != null) {
            if (parent == null) {
                root.compareAndSet(child, right, true, false);
            } else if (parent.left.getReference() == child) {
                parent.left.compareAndSet(child, right, true, false);
            } else {
                parent.right.compareAndSet(child, right, true, false);
            }
        } else {
            Node successor = right;
            Node successorParent = child;
            AtomicMarkableReference<Node> successorRef = child.right;
            while (successor.left.getReference() != null) {
                successorParent = successor;
                successorRef = successor.left;
                successor = successorRef.getReference();
            }
            if (successorParent != child) {
                successorParent.left.compareAndSet(successor, successor.right.getReference(), false, false);
                successor.right = new AtomicMarkableReference<>(right, false);
            }
            successor.left = new AtomicMarkableReference<>(left, false);
            if (parent == null) {
                root.compareAndSet(child, successor, true, false);
            } else if (parent.left.getReference() == child) {
                parent.left.compareAndSet(child, successor, true, false);
            } else {
                parent.right.compareAndSet(child, successor, true, false);
            }
        }
    }
}