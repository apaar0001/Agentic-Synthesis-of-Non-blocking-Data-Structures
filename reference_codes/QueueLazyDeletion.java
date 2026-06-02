package com.example.Sets;

import com.example.utils.SetADT;
import com.example.utils.QueueADT;
import com.example.utils.StackADT;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Lock-free Queue with Lazy Deletion mark.
 */
public class QueueLazyDeletion implements QueueADT {
    private static class Node {
        final int v;
        final AtomicReference<Node> n;
        final AtomicBoolean d; // Dequeued

        Node(int val) {
            v = val;
            n = new AtomicReference<>(null);
            d = new AtomicBoolean(false);
        }
    }

    private final AtomicReference<Node> h, t;

    public QueueLazyDeletion() {
        Node s = new Node(0);
        s.d.set(true);
        h = new AtomicReference<>(s);
        t = new AtomicReference<>(s);
    }

    @Override
    public void enqueue(int v) {
        Node nn = new Node(v);
        while (true) {
            Node la = t.get(), ne = la.n.get();
            if (la == t.get()) {
                if (ne == null) {
                    if (la.n.compareAndSet(null, nn)) {
                        t.compareAndSet(la, nn);
                        return;
                    }
                } else
                    t.compareAndSet(la, ne);
            }
        }
    }

    @Override
    public int dequeue() {
        while (true) {
            Node fi = h.get(), la = t.get(), ne = fi.n.get();
            if (fi == h.get()) {
                if (fi == la) {
                    if (ne == null)
                        return -1;
                    t.compareAndSet(la, ne);
                } else {
                    int val = ne.v;
                    // Dequeue victim point
                    if (ne.d.compareAndSet(false, true)) {
                        h.compareAndSet(fi, ne);
                        return val;
                    }
                    h.compareAndSet(fi, ne);
                }
            }
        }
    }

    @Override
    public boolean isEmpty() {
        return h.get().n.get() == null;
    }

}
