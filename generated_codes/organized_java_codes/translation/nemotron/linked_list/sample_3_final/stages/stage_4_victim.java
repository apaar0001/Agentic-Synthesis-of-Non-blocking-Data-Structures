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
        AtomicMarkableReference<Node> next;

        Node(int key) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, false);
        }
    }

    private final Node head;
    private final Node tail;

    public ConcurrentDataStructure() {
        head = new Node(Integer.MIN_VALUE);
        tail = new Node(Integer.MAX_VALUE);
        head.next.set(tail, false);
    }

    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        while (true) {
            Node pred = head;
            Node curr = head.next.getReference();
            boolean[] marked = {false};
            Node succ;

            while (true) {
                succ = curr.next.get(marked);
                while (marked[0]) {
                    Node nextUnmarked = curr.next.getReference();
                    if (pred.next.compareAndSet(curr, nextUnmarked, false, false)) {
                        // helped unlink
                    } else {
                        break;
                    }
                    curr = nextUnmarked;
                    succ = curr.next.get(marked);
                }
                if (curr.key >= key) {
                    break;
                }
                pred = curr;
                curr = succ;
            }

            if (curr.key == key) {
                return false;
            }

            Node node = new Node(key);
            node.next.set(curr, false);
            if (pred.next.compareAndSet(curr, node, false, false)) {
                return true;
            }
            // else retry
        }
    }

    @Override
    public boolean remove(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        while (true) {
            Node pred = head;
            Node curr = head.next.getReference();
            boolean[] marked = {false};
            Node succ;

            while (true) {
                succ = curr.next.get(marked);
                while (marked[0]) {
                    Node nextUnmarked = curr.next.getReference();
                    if (pred.next.compareAndSet(curr, nextUnmarked, false, false)) {
                        // helped unlink
                    } else {
                        break;
                    }
                    curr = nextUnmarked;
                    succ = curr.next.get(marked);
                }
                if (curr.key >= key) {
                    break;
                }
                pred = curr;
                curr = succ;
            }

            if (curr.key != key) {
                return false;
            }

            Node next = curr.next.getReference();
            boolean markedSuccess;
            do {
                markedSuccess = curr.next.attemptMark(next, true);
                if (!markedSuccess) {
                    next = curr.next.getReference();
                }
            } while (!markedSuccess);

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
            // Attempt physical removal (optional for lock-freedom)
            pred.next.compareAndSet(curr, next, false, false);
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        Node curr = head.next.getReference();
        boolean[] marked = {false};
        Node succ;

        while (true) {
            succ = curr.next.get(marked);
            while (marked[0]) {
                curr = succ;
                succ = curr.next.get(marked);
            }
            if (curr.key >= key) {
                return curr.key == key && !marked[0];
            }
            curr = succ;
        }
    }
}