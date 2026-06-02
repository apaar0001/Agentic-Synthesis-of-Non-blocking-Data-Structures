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
        root = new AtomicReference<>(null);
    }

    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        while (true) {
            Node curr = root.get();
            if (curr == null) {
                if (root.compareAndSet(null, new Node(key))) {
                    return true;
                }
            } else {
                if (curr.key == key) {
                    return false;
                } else if (curr.key < key) {
                    Node right = curr.right.getReference();
                    if (right == null) {
                        if (curr.right.compareAndSet(null, new Node(key))) {
                            return true;
                        }
                    } else if (right.isMarked()) {
                        helpRemove(curr, right);
                    } else {
                        curr = right;
                    }
                } else {
                    Node left = curr.left.getReference();
                    if (left == null) {
                        if (curr.left.compareAndSet(null, new Node(key))) {
                            return true;
                        }
                    } else if (left.isMarked()) {
                        helpRemove(curr, left);
                    } else {
                        curr = left;
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
            Node curr = root.get();
            if (curr == null) {
                return false;
            } else if (curr.key == key) {
                if (curr.left.getReference() == null && curr.right.getReference() == null) {
                    if (root.compareAndSet(curr, null)) {
                        return true;
                    }
                } else if (curr.left.getReference() == null) {
                    Node right = curr.right.getReference();
                    if (right.isMarked()) {
                        helpRemove(curr, right);
                    } else if (curr.right.compareAndSet(right, null)) {
                        if (curr.left.compareAndSet(null, null)) {
                            if (root.compareAndSet(curr, right)) {
                                return true;
                            }
                        }
                    }
                } else if (curr.right.getReference() == null) {
                    Node left = curr.left.getReference();
                    if (left.isMarked()) {
                        helpRemove(curr, left);
                    } else if (curr.left.compareAndSet(left, null)) {
                        if (curr.right.compareAndSet(null, null)) {
                            if (root.compareAndSet(curr, left)) {
                                return true;
                            }
                        }
                    }
                } else {
                    AtomicMarkableReference<Node> ref = curr.left.getReference().key < curr.right.getReference().key ? curr.left : curr.right;
                    if (ref.getReference().isMarked()) {
                        helpRemove(curr, ref.getReference());
                    } else if (ref.compareAndSet(ref.getReference(), null)) {
                        if (curr.left.getReference() == null && curr.right.getReference() == null) {
                            if (root.compareAndSet(curr, null)) {
                                return true;
                            }
                        } else if (curr.left.getReference() == null) {
                            if (curr.left.compareAndSet(null, null)) {
                                if (root.compareAndSet(curr, curr.right.getReference())) {
                                    return true;
                                }
                            }
                        } else if (curr.right.getReference() == null) {
                            if (curr.right.compareAndSet(null, null)) {
                                if (root.compareAndSet(curr, curr.left.getReference())) {
                                    return true;
                                }
                            }
                        }
                    }
                }
            } else if (curr.key < key) {
                Node right = curr.right.getReference();
                if (right == null) {
                    return false;
                } else if (right.isMarked()) {
                    helpRemove(curr, right);
                } else {
                    curr = right;
                }
            } else {
                Node left = curr.left.getReference();
                if (left == null) {
                    return false;
                } else if (left.isMarked()) {
                    helpRemove(curr, left);
                } else {
                    curr = left;
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        while (true) {
            Node curr = root.get();
            if (curr == null) {
                return false;
            } else if (curr.key == key) {
                return !curr.left.getReference().isMarked() && !curr.right.getReference().isMarked();
            } else if (curr.key < key) {
                Node right = curr.right.getReference();
                if (right == null) {
                    return false;
                } else if (right.isMarked()) {
                    helpRemove(curr, right);
                } else {
                    curr = right;
                }
            } else {
                Node left = curr.left.getReference();
                if (left == null) {
                    return false;
                } else if (left.isMarked()) {
                    helpRemove(curr, left);
                } else {
                    curr = left;
                }
            }
        }
    }

    private void helpRemove(Node parent, Node node) {
        if (node.isMarked()) {
            if (parent.left.getReference() == node) {
                parent.left.compareAndSet(node, null);
            } else if (parent.right.getReference() == node) {
                parent.right.compareAndSet(node, null);
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
        }
    }
}