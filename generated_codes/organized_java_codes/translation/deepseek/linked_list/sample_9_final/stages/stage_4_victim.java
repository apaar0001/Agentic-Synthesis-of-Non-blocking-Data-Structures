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
        final AtomicMarkableReference<Node> next;

        Node(int key) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, false);
        }
    }

    private final Node head;

    public ConcurrentDataStructure() {
        head = new Node(Integer.MIN_VALUE);
        head.next.set(new Node(Integer.MAX_VALUE), false);
    }

    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        while (true) {
            Window window = find(key);
            Node pred = window.pred;
            Node curr = window.curr;

            if (curr.key == key) {
                return false;
            }

            Node newNode = new Node(key);
            newNode.next.set(curr, false);

            if (pred.next.compareAndSet(curr, newNode, false, false)) {
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
            Window window = find(key);
            Node pred = window.pred;
            Node curr = window.curr;

            if (curr.key != key) {
                return false;
            }

            Node succ = curr.next.getReference();
            if (!curr.next.attemptMark(succ, true)) {
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
            pred.next.compareAndSet(curr, succ, false, false);
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        Node curr = head.next.getReference();
        while (curr.key < key) {
            curr = curr.next.getReference();
        }
        return curr.key == key && !curr.next.isMarked();
    }

    private Window find(int key) {
        retry: while (true) {
            Node pred = head;
            Node curr = pred.next.getReference();
            while (true) {
                boolean[] marked = {false};
                Node succ = curr.next.get(marked);
                while (marked[0]) {
                    if (!pred.next.compareAndSet(curr, succ, false, false)) {
                        continue retry;
                    }
                    curr = succ;
                    succ = curr.next.get(marked);
                }
                if (curr.key >= key) {
                    return new Window(pred, curr);
                }
                pred = curr;
                curr = succ;
            }
        }
    }

    private static class Window {
        final Node pred;
        final Node curr;

        Window(Node pred, Node curr) {
            this.pred = pred;
            this.curr = curr;
        }
    }
}