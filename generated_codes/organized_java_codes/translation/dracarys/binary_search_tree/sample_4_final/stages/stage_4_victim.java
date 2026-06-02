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
            Node curr = root.get();
            if (curr == null) {
                Node newNode = new Node(key);
                if (root.compareAndSet(null, newNode)) {
                    return true;
                }
            } else {
                Node[] predAndSucc = findPredAndSucc(curr, key);
                Node pred = predAndSucc[0];
                Node succ = predAndSucc[1];
                if (succ == null || succ.key != key) {
                    Node newNode = new Node(key);
                    newNode.left.set(pred);
                    newNode.right.set(succ);
                    if (pred.right.compareAndSet(succ, newNode)) {
                        return true;
                    }
                } else {
                    return false;
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
            } else {
                Node[] predAndSucc = findPredAndSucc(curr, key);
                Node pred = predAndSucc[0];
                Node succ = predAndSucc[1];
                if (succ == null || succ.key != key) {
                    return false;
                } else {
                    Node succSucc = succ.right.getReference();
                    if (succSucc == null) {
                        if (succ.marked.compareAndSet(false, true)) {
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
                            if (pred.right.compareAndSet(succ, null)) {
                                return true;
                            }
                        }
                    } else {
                        if (succSucc.marked.compareAndSet(false, true)) {
                            // Node has been marked
                            if (succ.right.compareAndSet(succSucc, succSucc.right.getReference())) {
                                return true;
                            }
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
        while (true) {
            Node curr = root.get();
            if (curr == null) {
                return false;
            } else {
                Node[] predAndSucc = findPredAndSucc(curr, key);
                Node pred = predAndSucc[0];
                Node succ = predAndSucc[1];
                if (succ == null || succ.key != key) {
                    return false;
                } else {
                    return !succ.marked.get();
                }
            }
        }
    }

    private Node[] findPredAndSucc(Node curr, int key) {
        Node pred = null;
        Node succ = curr;
        while (succ != null) {
            if (succ.key == key) {
                return new Node[]{pred, succ};
            } else if (succ.key < key) {
                pred = succ;
                succ = succ.right.getReference();
            } else {
                succ = succ.left.getReference();
            }
        }
        return new Node[]{pred, null};
    }

    private class Node {
        int key;
        AtomicReference<Node> left;
        AtomicReference<Node> right;
        AtomicMarkableReference<Boolean> marked;

        Node(int key) {
            this.key = key;
            left = new AtomicReference<>(null);
            right = new AtomicReference<>(null);
            marked = new AtomicMarkableReference<>(false);
        }
    }
}