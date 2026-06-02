package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Lock-free hash set based on a split‑ordered list (Shalev‑Shavit).
 * Uses CAS for all updates, logical deletion via marking, and
 * lock‑free resizing by atomically increasing the length parameter.
 */
public class ConcurrentDataStructure implements SetADT {

    /** Node stored in the split‑ordered list. */
    private static final class Node {
        final int key;          // original key
        final int spread;       // spread‑ordered hash of the key
        final AtomicMarkableReference<Node> next;

        Node(int key, int spread) {
            this.key = key;
            this.spread = spread;
            this.next = new AtomicMarkableReference<>(null, false);
        }
    }

    /** Head sentinel (smallest spread). */
    private final Node head = new Node(0, 0);
    /** Tail sentinel (largest spread). */
    private final Node tail = new Node(Integer.MAX_VALUE, Integer.MAX_VALUE);
    /** Current number of elements. */
    private final AtomicInteger size = new AtomicInteger(0);
    /** Current table size (power of two). */
    private final AtomicInteger length = new AtomicInteger(1);

    public ConcurrentDataStructure() {
        head.next.set(tail, false);
        tail.next.set(null, false);
    }

    /** Spread‑ordering hash (based on Integer.reverse). */
    private static int spread(int key) {
        int h = Integer.reverse(key);
        // make non‑negative
        if (h < 0) h = ~h;
        return h;
    }

    /**
     * Find predecessor and current node for a given spread.
     * Helps remove logically deleted nodes during the search.
     */
    private Node[] find(int spread) {
        while (true) {
            Node pred = head;
            Node curr = pred.next.getReference();
            while (true) {
                Node succ = curr.next.getReference();
                boolean marked = curr.next.isMarked();

                if (marked) {
                    // Help unlink the marked node.
                    if (pred.next.compareAndSet(curr, succ, false, false)) {
                        curr = succ;
                        continue;
                    } else {
                        break; // restart outer search
                    }
                }

                // curr is unmarked (or tail)
                if (curr == tail || curr.spread >= spread) {
                    return new Node[]{pred, curr};
                }
                pred = curr;
                curr = succ;
            }
            // If we failed to help unlink, retry from the start.
        }
    }

    @Override
    public boolean add(int key) {
        int s = spread(key);
        while (true) {
            Node[] predCurr = find(s);
            Node pred = predCurr[0];
            Node curr = predCurr[1];

            // Key already present?
            if (curr != tail && curr.spread == s && curr.key == key) {
                return false;
            }

            Node node = new Node(key, s);
            node.next.set(curr, false);

            if (pred.next.compareAndSet(curr, node, false, false)) {
                int newSize = size.incrementAndGet();
                maybeResize();
                return true;
                // else retry
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int s = spread(key);
        while (true) {
            Node[] predCurr = find(s);
            Node pred = predCurr[0];
            Node curr = predCurr[1];

            if (curr == tail || curr.spread != s || curr.key != key) {
                return false;
            }

            Node succ = curr.next.getReference();
            if (curr.next.attemptMark(succ, true)) {
                // Node has been marked
                pred.next.compareAndSet(curr, succ, false, false);
                size.decrementAndGet();
                return true;
            }
            // If marking failed (already marked), help unlink and retry.
            Node next = curr.next.getReference();
            boolean marked = curr.next.isMarked();
            if (marked) {
                pred.next.compareAndSet(curr, next, false, false);
            }
            // retry outer loop
        }
    }

    @Override
    public boolean contains(int key) {
        int s = spread(key);
        Node[] predCurr = find(s);
        Node curr = predCurr[1];
        return (curr != tail && curr.spread == s && curr.key == key && !curr.next.isMarked());
    }

    /** Attempt to double the capacity when the load factor exceeds 0.75. */
    private void maybeResize() {
        int currentSize = size.get();
        int currentLen = length.get();
        if (currentSize * 4 > currentLen * 3) { // > 0.75
            int newLen = currentLen << 1;
            if (newLen >= (1 << 30)) return; // cap to avoid overflow
            length.compareAnd