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
        this.root = new AtomicMarkableReference<>(null, false);
    }

    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        Node newNode = new Node(key);
        while (true) {
            AtomicMarkableReference<Node> currRef = root;
            Node parent = null;
            boolean restart = false;
            while (true) {
                Node curr = currRef.getReference();
                boolean marked = currRef.isMarked();
                if (marked) {
                    helpRemove(parent, currRef, curr);
                    restart = true;
                    break;
                }
                if (curr == null) {
                    if (currRef.compareAndSet(null, newNode, false, false)) {
                        return true;
                    } else {
                        restart = true;
                        break;
                    }
                }
                if (key == curr.key) {
                    return false;
                }
                boolean goLeft = key < curr.key;
                parent = curr;
                currRef = goLeft ? curr.left : curr.right;
            }
            if (!restart) {
                // Should not reach here
                break;
            }
            // restart outer while loop
        }
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        AtomicMarkableReference<Node> currRef = root;
        Node parent = null;
        while (true) {
            Node curr = currRef.getReference();
            boolean marked = currRef.isMarked();
            if (marked) {
                helpRemove(parent, currRef, curr);
                parent = null;
                currRef = root;
                continue;
            }
            if (curr == null) {
                return false;
            }
            if (key == curr.key) {
                return true;
            }
            boolean goLeft = key < curr.key;
            parent = curr;
            currRef = goLeft ? curr.left : curr.right;
        }
    }

    @Override
    public boolean remove(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        while (true) {
            AtomicMarkableReference<Node> currRef = root;
            Node parent = null;
            boolean restart = false;
            while (true) {
                Node curr = currRef.getReference();
                boolean marked = currRef.isMarked();
                if (marked) {
                    helpRemove(parent, currRef, curr);
                    restart = true;
                    break;
                }
                if (curr == null) {
                    return false;
                }
                if (key == curr.key) {
                    if (currRef.attemptMark(curr, true)) {
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
                        helpRemove(parent, currRef, curr);
                        return true;
                    } else {
                        restart = true;
                        break;
                    }
                }
                boolean goLeft = key < curr.key;
                parent = curr;
                currRef = goLeft ? curr.left : curr.right;
            }
            if (!restart) {
                break;
            }
        }
    }

    private void helpRemove(Node parent, AtomicMarkableReference<Node> link, Node victim) {
        // Assume victim is marked
        Node left = victim.left.getReference();
        boolean leftMarked = victim.left.isMarked();
        Node right = victim.right.getReference();
        boolean rightMarked = victim.right.isMarked();

        // Help remove marked children
        while (left != null && leftMarked) {
            helpRemove(victim, victim.left, left);
            left = victim.left.getReference();
            leftMarked = victim.left.isMarked();
        }
        while (right != null && rightMarked) {
            helpRemove(victim, victim.right, right);
            right = victim.right.getReference();
            rightMarked = victim.right.isMarked();
        }

        Node replacement;
        if (left == null && right == null) {
            replacement = null;
        } else if (left == null) {
            replacement = right;
        } else if (right == null) {
            replacement = left;
        } else {
            // Two children: cannot remove now
            return;
        }

        // Attempt to replace victim with replacement (or null) and unmark
        link.compareAndSet(victim, replacement, true, false);
    }
}