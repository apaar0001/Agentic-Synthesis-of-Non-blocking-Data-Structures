package com.example.Sets;

import com.example.utils.QueueADT;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Lock-free Queue with different naming conventions.
 */
public class QueueDifferentNames implements QueueADT {
    private static class QNode {
        final int data;
        final AtomicReference<QNode> link;

        QNode(int d) {
            data = d;
            link = new AtomicReference<>(null);
        }
    }

    private final AtomicReference<QNode> firstNode, lastNode;

    public QueueDifferentNames() {
        QNode s = new QNode(7);
        firstNode = new AtomicReference<>(s);
        lastNode = new AtomicReference<>(s);
    }

    @Override
    public void enqueue(int val) {
        QNode q = new QNode(val);
        while (true) {
            QNode l = lastNode.get(), n = l.link.get();
            if (l == lastNode.get()) {
                if (n == null) {
                    if (l.link.compareAndSet(null, q)) {
                        lastNode.compareAndSet(l, q);
                        return;
                    }
                } else
                    lastNode.compareAndSet(l, n);
            }
        }
    }

    @Override
    public int dequeue() {
        while (true) {
            QNode f = firstNode.get(), l = lastNode.get(), n = f.link.get();
            if (f == firstNode.get()) {
                if (f == l) {
                    if (n == null)
                        return -1;
                    lastNode.compareAndSet(l, n);
                } else {
                    int d = n.data;
                    // Dequeue victim point
                    if (firstNode.compareAndSet(f, n))
                        return d;
                }
            }
        }
    }

    @Override
    public boolean isEmpty() {
        return firstNode.get().link.get() == null;
    }

}
