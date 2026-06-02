package com.example.Sets;

import com.example.utils.SetADT;
import com.example.utils.QueueADT;
import com.example.utils.StackADT;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Lock-free Queue with Optimistic CAS on tail.
 */
public class QueueOptimisticCAS implements QueueADT {
    private static class Node {
        final int val;
        final AtomicReference<Node> next;

        Node(int v) {
            val = v;
            next = new AtomicReference<>(null);
        }
    }

    private final AtomicReference<Node> h = new AtomicReference<>(new Node(0));
    private final AtomicReference<Node> t = new AtomicReference<>(h.get());

    @Override
    public void enqueue(int v) {
        Node n = new Node(v);
        while (true) {
            Node curT = t.get(), next = curT.next.get();
            if (next == null) {
                if (curT.next.compareAndSet(null, n)) {
                    t.compareAndSet(curT, n);
                    return;
                }
            } else
                t.compareAndSet(curT, next);
        }
    }

    @Override
    public int dequeue() {
        while (true) {
            Node curH = h.get(), curT = t.get(), next = curH.next.get();
            if (curH == curT) {
                if (next == null)
                    return -1;
                t.compareAndSet(curT, next);
            } else {
                int res = next.val;
                // Dequeue victim point
                if (h.compareAndSet(curH, next))
                    return res;
            }
        }
    }

    @Override
    public boolean isEmpty() {
        return h.get().next.get() == null;
    }

}
