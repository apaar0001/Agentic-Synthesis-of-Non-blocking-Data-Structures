package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicReferenceArray;

public class ConcurrentDataStructure implements SetADT {
private static final int DEFAULT_CAPACITY = 1 << 16;


private final AtomicReferenceArray<AtomicReference<Node>> buckets;
private final AtomicInteger size;

private static final class Node {
    final int key;
    final AtomicMarkableReference<Node> next;

    Node(int key, Node next) {
        this.key = key;
        this.next = new AtomicMarkableReference<>(next, false);
    }
}

private static final class Window {
    final Node pred;
    final Node curr;

    Window(Node pred, Node curr) {
        this.pred = pred;
        this.curr = curr;
    }
}

public ConcurrentDataStructure() {
    this.buckets = new AtomicReferenceArray<>(DEFAULT_CAPACITY);
    this.size = new AtomicInteger(0);
    for (int i = 0; i < DEFAULT_CAPACITY; i++) {
        buckets.set(i, new AtomicReference<>(new Node(0, null)));
    }
}

@Override
public boolean add(int key) {
    AtomicReference<Node> bucket = bucketFor(key);

    while (true) {
        Window window = find(bucket, key);
        Node pred = window.pred;
        Node curr = window.curr;

        if (curr != null && curr.key == key) {
            return false;
        }

        Node node = new Node(key, curr);
        if (pred.next.compareAndSet(curr, node, false, false)) {
            size.incrementAndGet();
            return true;
        }
    }
}

@Override
public boolean remove(int key) {
    AtomicReference<Node> bucket = bucketFor(key);

    while (true) {
        Window window = find(bucket, key);
        Node pred = window.pred;
        Node curr = window.curr;

        if (curr == null || curr.key != key) {
            return false;
        }

        Node succ = curr.next.getReference();
        if (curr.next.compareAndSet(succ, succ, false, true)) {
            // Node has been marked
            pred.next.compareAndSet(curr, succ, false, false);
            size.decrementAndGet();
            return true;
        }
    }
}

@Override
public boolean contains(int key) {
    AtomicReference<Node> bucket = bucketFor(key);
    Node curr = bucket.get().next.getReference();

    while (curr != null && curr.key < key) {
        curr = curr.next.getReference();
    }

    if (curr == null || curr.key != key) {
        return false;
    }

    boolean[] marked = new boolean[1];
    curr.next.get(marked);
    return !marked[0];
}

private Window find(AtomicReference<Node> bucket, int key) {
    retry:
    while (true) {
        Node pred = bucket.get();
        Node curr = pred.next.getReference();

        while (true) {
            if (curr == null) {
                return new Window(pred, null);
            }

            boolean[] marked = new boolean[1];
            Node succ = curr.next.get(marked);

            while (marked[0]) {
                if (!pred.next.compareAndSet(curr, succ, false, false)) {
                    continue retry;
                }

                curr = succ;

                if (curr == null) {
                    return new Window(pred, null);
                }

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

private AtomicReference<Node> bucketFor(int key) {
    return buckets.get(spread(key) & (DEFAULT_CAPACITY - 1));
}

private static int spread(int key) {
    int h = key;
    h ^= h >>> 16;
    h *= 0x7feb352d;
    h ^= h >>> 15;
    h *= 0x846ca68b;
    h ^= h >>> 16;
    return h;
}


}
