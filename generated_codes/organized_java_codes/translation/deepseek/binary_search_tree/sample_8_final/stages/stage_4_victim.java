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
            Node node = current;
            AtomicMarkableReference<Node> childRef = root;
            boolean isLeft = false;

            while (node != null) {
                if (key == node.key) {
                    boolean[] marked = new boolean[1];
                    Node next = childRef.get(marked);
                    if (marked[0]) {
                        continue;
                    }
                    return false;
                }

                parent = node;
                if (key < node.key) {
                    childRef = node.left;
                    isLeft = true;
                } else {
                    childRef = node.right;
                    isLeft = false;
                }
                node = childRef.getReference();
            }

            if (childRef.compareAndSet(null, newNode, false, false)) {
                return true;
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
            Node node = root.getReference();
            AtomicMarkableReference<Node> childRef = root;
            boolean isLeft = false;

            while (node != null) {
                if (key == node.key) {
                    boolean[] marked = new boolean[1];
                    Node left = node.left.get(marked);
                    if (marked[0]) {
                        continue;
                    }
                    Node right = node.right.get(marked);
                    if (marked[0]) {
                        continue;
                    }

                    if (!childRef.attemptMark(node, true)) {
                        continue;
                    }
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

                    if (left != null && right != null) {
                        Node successor = right;
                        Node succParent = node;
                        AtomicMarkableReference<Node> succRef = node.right;

                        while (successor.left.getReference() != null) {
                            succParent = successor;
                            succRef = successor.left;
                            successor = succRef.getReference();
                        }

                        if (succParent != node) {
                            succParent.left.compareAndSet(successor, successor.right.getReference(), false, false);
                            successor.right = new AtomicMarkableReference<>(right, false);
                        }
                        successor.left = new AtomicMarkableReference<>(left, false);

                        if (childRef.compareAndSet(node, successor, true, false)) {
                            return true;
                        }
                    } else if (left != null) {
                        if (childRef.compareAndSet(node, left, true, false)) {
                            return true;
                        }
                    } else if (right != null) {
                        if (childRef.compareAndSet(node, right, true, false)) {
                            return true;
                        }
                    } else {
                        if (childRef.compareAndSet(node, null, true, false)) {
                            return true;
                        }
                    }
                    continue;
                }

                parent = node;
                if (key < node.key) {
                    childRef = node.left;
                    isLeft = true;
                } else {
                    childRef = node.right;
                    isLeft = false;
                }
                node = childRef.getReference();
            }
            return false;
        }
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        Node node = root.getReference();
        while (node != null) {
            if (key == node.key) {
                boolean[] marked = new boolean[1];
                node.left.get(marked);
                if (marked[0]) {
                    return false;
                }
                node.right.get(marked);
                if (marked[0]) {
                    return false;
                }
                return true;
            } else if (key < node.key) {
                node = node.left.getReference();
            } else {
                node = node.right.getReference();
            }
        }
        return false;
    }
}