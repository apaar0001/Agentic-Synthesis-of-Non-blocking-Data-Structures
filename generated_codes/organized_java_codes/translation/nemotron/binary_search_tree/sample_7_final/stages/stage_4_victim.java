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
        int key;
        final AtomicMarkableReference<Node> left;
        final AtomicMarkableReference<Node> right;
        Node(int k) {
            key = k;
            left = new AtomicMarkableReference<>(null, false);
            right = new AtomicMarkableReference<>(null, false);
        }
    }

    private final AtomicMarkableReference<Node> root = new AtomicMarkableReference<>(null, false);

    private static Node getReference(AtomicMarkableReference<Node> ref, boolean[] marked) {
        return ref.getReference(marked);
    }

    private void helpRemove(AtomicMarkableReference<Node> parentRef, Node markedNode) {
        boolean[] marked = {true};
        Node expected = markedNode;
        Node left = getReference(markedNode.left, new boolean[1]);
        Node right = getReference(markedNode.right, new boolean[1]);
        Node replacement = (left != null) ? left : right;
        parentRef.compareAndSet(expected, replacement, true, false);
    }

    private boolean tryMarkNode(AtomicMarkableReference<Node> parentRef, Node node) {
        boolean[] marked = {false};
        Node expected = getReference(parentRef, marked);
        if (expected != node || marked[0]) {
            return false;
        }
        boolean success = parentRef.compareAndSet(expected, node, false, true);
        if (success) {
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
        return success;
    }

    private Node findNode(int key, AtomicMarkableReference<Node>[] parentHolder) {
        boolean[] marked = {false};
        AtomicMarkableReference<Node> currRef = root;
        Node curr = getReference(currRef, marked);
        parentHolder[0] =