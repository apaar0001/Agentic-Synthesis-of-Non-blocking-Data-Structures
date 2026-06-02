package com.example.Sets;

import com.example.utils.QueueADT;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Lock-free Queue using Sentinel Node.
 */
public class QueueSentinelNode implements QueueADT {
    private static class Node {
        final int v;
        final AtomicReference<Node> f;

        Node(int val) {
            v = val;
            f = new AtomicReference<>(null);
        }
    }

    private final AtomicReference<Node> headRef, tailRef;

    public QueueSentinelNode() {
        Node s = new Node(-1);
        headRef = new AtomicReference<>(s);
        tailRef = new AtomicReference<>(s);
    }

    @Override
    public void enqueue(int v) {
        Node n = new Node(v);
        while (true) {
            Node t = tailRef.get(), f = t.f.get();
            if (t == tailRef.get()) {
                if (f == null) {
                    if (t.f.compareAndSet(null, n)) {
                        tailRef.compareAndSet(t, n);
                        return;
                    }
                } else
                    tailRef.compareAndSet(t, f);
            }
        }
    }

    @Override
    public int dequeue() {
        while (true) {
            Node h = headRef.get(), t = tailRef.get(), f = h.f.get();
            if (h == headRef.get()) {
                if (h == t) {
                    if (f == null)
                        return -1;
                    tailRef.compareAndSet(t, f);
                } else {
                    int v = f.v;
                    // Dequeue victim point
                    if (headRef.compareAndSet(h, f))
                        return v;
                }
            }
        }
    }

    @Override
    public boolean isEmpty() {
        return headRef.get().f.get() == null;
    }

}
