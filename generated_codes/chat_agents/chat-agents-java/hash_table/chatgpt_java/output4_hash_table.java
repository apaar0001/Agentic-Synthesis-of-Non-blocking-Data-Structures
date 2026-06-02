package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicReferenceArray;

public class ConcurrentDataStructure implements SetADT {
private static final int BUCKET_COUNT = 1 << 16;


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
    this.buckets = new AtomicReferenceArray<>(BUCKET_COUNT);
    this.size = new AtomicInteger(0);

    for (int i = 0; i < BUCKET_COUNT; i++) {
        Node tail = new Node(Integer.MAX_VALUE, null);
        Node head = new Node(Integer.MIN_VALUE, tail);
        buckets.set(i, new AtomicReference<>(head));
    }
}

@Override
public boolean add(int key) {
    AtomicReference<Node> bucket = bucket(key);

    while (true) {
        Window window = find(bucket, key);
        Node pred = window.pred;
        Node curr = window.curr;

        if (curr.key == key) {
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
    AtomicReference<Node> bucket = bucket(key);

    while (true) {
        Window window = find(bucket, key);
        Node pred = window.pred;
        Node curr = window.curr;

        if (curr.key != key) {
            return false;
        }

        Node succ = curr.next.getReference();
        if (curr.next.attemptMark(succ, true)) {
            // Node has been marked
            pred.next.compareAndSet(curr, succ, false, false);
            size.decrementAndGet();
            return true;
        }
    }
}

@Override
public boolean contains(int key) {
    AtomicReference<Node> bucket = bucket(key);
    Node curr = bucket.get().next.getReference();

    while (curr.key < key) {
        curr = curr.next.getReference();
    }

    boolean[] marked = new boolean[1];
    curr.next.get(marked);
    return curr.key == key && !marked[0];
}

private Window find(AtomicReference<Node> bucket, int key) {
    retry:
    while (true) {
        Node pred = bucket.get();
        Node curr = pred.next.getReference();

        while (true) {
            boolean[] marked = new boolean[1];
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

private AtomicReference<Node> bucket(int key) {
    return buckets.get(hash(key) & (BUCKET_COUNT - 1));
}

private static int hash(int key) {
    int h = key;
    h ^= h >>> 16;
    h *= 0x7feb352d;
    h ^= h >>> 15;
    h *= 0x846ca68b;
    h ^= h >>> 16;
    return h;
}


}
