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
            Node current = root.get();
            if (current == null) {
                Node newNode = new Node(key);
                if (root.compareAndSet(null, newNode)) {
                    return true;
                }
            } else {
                Node next = findNext(current, key);
                if (next.key == key) {
                    return false;
                }
                Node newNode = new Node(key);
                if (key < next.key) {
                    if (next.left.compareAndSet(null, newNode)) {
                        return true;
                    }
                } else {
                    if (next.right.compareAndSet(null, newNode)) {
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
            Node next = findNext(current, key);
            if (next.key != key) {
                return false;
            }
            if (next.marked.get()) {
                return false;
            }
            if (next.left.get() == null && next.right.get() == null) {
                if (next.marked.compareAndSet(false, true)) {
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
                    Node parent = findParent(current, key);
                    if (key < parent.key) {
                        parent.left.compareAndSet(next, null);
                    } else {
                        parent.right.compareAndSet(next, null);
                    }
                    return true;
                }
            } else if (next.left.get() == null) {
                if (next.marked.compareAndSet(false, true)) {
                    // Node has been marked
                    Node parent = findParent(current, key);
                    if (key < parent.key) {
                        parent.left.compareAndSet(next, next.right.get());
                    } else {
                        parent.right.compareAndSet(next, next.right.get());
                    }
                    return true;
                }
            } else if (next.right.get() == null) {
                if (next.marked.compareAndSet(false, true)) {
                    // Node has been marked
                    Node parent = findParent(current, key);
                    if (key < parent.key) {
                        parent.left.compareAndSet(next, next.left.get());
                    } else {
                        parent.right.compareAndSet(next, next.left.get());
                    }
                    return true;
                }
            } else {
                Node successor = findSuccessor(next);
                if (successor.marked.get()) {
                    continue;
                }
                if (next.key == successor.key) {
                    if (next.marked.compareAndSet(false, true)) {
                        // Node has been marked
                        Node parent = findParent(current, key);
                        if (key < parent.key) {
                            parent.left.compareAndSet(next, successor.left.get());
                        } else {
                            parent.right.compareAndSet(next, successor.left.get());
                        }
                        return true;
                    }
                } else {
                    if (next.key == successor.key) {
                        if (next.marked.compareAndSet(false, true)) {
                            // Node has been marked
                            Node parent = findParent(current, key);
                            if (key < parent.key) {
                                parent.left.compareAndSet(next, successor.right.get());
                            } else {
                                parent.right.compareAndSet(next, successor.right.get());
                            }
                            return true;
                        }
                    } else {
                        if (next.marked.compareAndSet(false, true)) {
                            // Node has been marked
                            Node parent = findParent(current, key);
                            if (key < parent.key) {
                                parent.left.compareAndSet(next, successor.left.get());
                            } else {
                                parent.right.compareAndSet(next, successor.left.get());
                            }
                            return true;
                        }
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
            if (current.key == key && !current.marked.get()) {
                return true;
            }
            if (key < current.key) {
                current = current.left.get();
            } else {
                current = current.right.get();
            }
        }
        return false;
    }

    private Node findNext(Node current, int key) {
        while (true) {
            if (key < current.key) {
                if (current.left.get() != null) {
                    current = current.left.get();
                } else {
                    return current;
                }
            } else if (key > current.key) {
                if (current.right.get() != null) {
                    current = current.right.get();
                } else {
                    return current;
                }
            } else {
                if (current.marked.get()) {
                    if (current.left.get() != null) {
                        current = current.left.get();
                    } else {
                        return current;
                    }
                } else {
                    return current;
                }
            }
        }
    }

    private Node findParent(Node current, int key) {
        if (current == null) {
            return null;
        }
        if (key < current.key) {
            if (current.left.get() != null && current.left.get().key == key) {
                return current;
            }
            return findParent(current.left.get(), key);
        } else {
            if (current.right.get() != null && current.right.get().key == key) {
                return current;
            }
            return findParent(current.right.get(), key);
        }
    }

    private Node findSuccessor(Node current) {
        current = current.right.get();
        while (current.left.get() != null) {
            current = current.left.get();
        }
        return current;
    }

    private static class Node {
        int key;
        AtomicReference<Node> left;
        AtomicReference<Node> right;
        AtomicMarkableReference<Node> marked;

        Node(int key) {
            this.key = key;
            this.left = new AtomicReference<>(null);
            this.right = new AtomicReference<>(null);
            this.marked = new AtomicMarkableReference<>(null, false);
        }
    }
}