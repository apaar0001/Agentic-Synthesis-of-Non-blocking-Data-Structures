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
        while (true) {
            AtomicMarkableReference<Node> currRef = root;
            Node curr = currRef.getReference();
            boolean currMarked = currRef.isMarked();

            if (currMarked) {
                continue;
            }
            if (curr == null) {
                Node newNode = new Node(key);
                if (root.compareAndSet(null, newNode, false, false)) {
                    return true;
                }
                continue;
            }

            AtomicMarkableReference<Node> parentRef = currRef;
            Node parent = curr;
            while (true) {
                Node node = parentRef.getReference();
                boolean nodeMarked = parentRef.isMarked();

                if (nodeMarked) {
                    break;
                }
                if (key == node.key) {
                    return false;
                }
                boolean goLeft = key < node.key;
                AtomicMarkableReference<Node> childRef = goLeft ? node.left : node.right;
                Node child = childRef.getReference();
                boolean childMarked = childRef.isMarked();

                if (child == null || childMarked) {
                    Node newNode = new Node(key);
                    if (childRef.compareAndSet(child, newNode, childMarked, false)) {
                        return true;
                    }
                    break;
                } else {
                    parentRef = childRef;
                    parent = node;
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
            AtomicMarkableReference<Node> prevRef = root;
            Node prev = prevRef.getReference();
            boolean prevMarked = prevRef.isMarked();

            if (prevMarked) {
                continue;
            }
            if (prev == null) {
                return false;
            }

            boolean found = false;
            AtomicMarkableReference<Node> targetRef = null;
            Node target = null;

            while (true) {
                Node node = prevRef.getReference();
                boolean nodeMarked = prevRef.isMarked();

                if (nodeMarked) {
                    break;
                }
                if (key == node.key) {
                    found = true;
                    targetRef = prevRef;
                    target = node;
                    break;
                }
                boolean goLeft = key < node.key;
                AtomicMarkableReference<Node> nextRef = goLeft ? node.left : node.right;
                Node next = nextRef.getReference();
                boolean nextMarked = nextRef.isMarked();

                if (next == null || nextMarked) {
                    break;
                }
                prevRef = nextRef;
                prev = next;
            }

            if (!found) {
                return false;
            }

            if (targetRef.attemptMark(target, true)) {
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
                Node left = target.left.getReference();
                Node right = target.right.getReference();
                Node replacement = (left != null) ? left : right;
                if (targetRef.compareAndSet(target, replacement, true, false)) {
                    return true;
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        AtomicMarkableReference<Node> currRef = root;
        while (true) {
            Node curr = currRef.getReference();
            boolean currMarked = currRef.isMarked();

            if (curr == null || currMarked) {
                return false;
            }
            if (key == curr.key) {
                return true;
            }
            boolean goLeft = key < curr.key;
            AtomicMarkableReference<Node> nextRef = goLeft ? curr.left : curr.right;
            currRef = nextRef;
        }
    }
}