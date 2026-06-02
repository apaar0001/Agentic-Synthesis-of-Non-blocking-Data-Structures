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
        AtomicMarkableReference<Node> next;

        Node(int key) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, false);
        }
    }

    private final AtomicReference<Node> head;

    public ConcurrentDataStructure() {
        Node minNode = new Node(Integer.MIN_VALUE);
        Node maxNode = new Node(Integer.MAX_VALUE);
        minNode.next.set(maxNode, false);
        head.set(minNode);
    }

    private Node[] find(int key) {
        retry:
        while (true) {
            Node pred = head.get();
            Node curr = pred.next.getReference();
            while (true) {
                Node succ = curr.next.getReference();
                boolean marked = curr.next.isMarked();
                while (marked) {
                    Node succNext = succ.next.getReference();
                    if (pred.next.compareAndSet(curr, succ, false, false)) {
                        // helped remove curr
                    } else {
                        continue retry;
                    }
                    curr = succ;
                    succ = succNext;
                    marked = succ.next.isMarked();
                }
                if (curr.key >= key) {
                    return new Node[]{pred, curr};
                }
                pred = curr;
                curr = succ;
            }
        }
    }

    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        while (true) {
            Node[] predCurr = find(key);
            Node pred = predCurr[0];
            Node curr = predCurr[1];
            if (curr.key == key) {
                return false;
            }
            Node node = new Node(key);
            node.next.set(curr, false);
            if (pred.next.compareAndSet(curr, node, false, false)) {
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
            Node[] predCurr = find(key);
            Node pred = predCurr[0];
            Node node = predCurr[1];
            Node succ = node.next.getReference();
            if (node.key != key) {
                return false;
            }
            if (node.next.compareAndSet(node, succ, false, true)) {
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
                pred.next.compareAndSet(node, succ, false, false);
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        Node curr = head.get();
        while (true) {
            Node succ = curr.next.getReference();
            boolean marked = curr.next.isMarked();
            while (marked) {
                curr = succ;
                succ = curr.next.getReference();
                marked = curr.next.isMarked();
            }
            if (curr.key >= key) {
                return curr.key == key;
            }
            curr = succ;
        }
    }
}