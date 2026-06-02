package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicReferenceArray;

public class ConcurrentDataStructure implements SetADT {
private static final int INITIAL_CAPACITY = 16;
private static final int LOAD_FACTOR = 4;


private final Node head;
private final Node tail;
private final AtomicInteger size;
private final AtomicInteger capacity;
private final AtomicReference<AtomicReferenceArray<AtomicReference<Node>>> buckets;

private static final class Node {
    final int key;
    final int order;
    final AtomicMarkableReference<Node> next;

    Node(int key, int order, Node next) {
        this.key = key;
        this.order = order;
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
    tail = new Node(Integer.MAX_VALUE, Integer.MAX_VALUE, null);
    head = new Node(Integer.MIN_VALUE, Integer.MIN_VALUE, tail);
    size = new AtomicInteger(0);
    capacity = new AtomicInteger(INITIAL_CAPACITY);

    AtomicReferenceArray<AtomicReference<Node>> initial = new AtomicReferenceArray<>(INITIAL_CAPACITY);
    for (int i = 0; i < INITIAL_CAPACITY; i++) {
        initial.set(i, new AtomicReference<>(head));
    }
    buckets = new AtomicReference<>(initial);
}

@Override
public boolean add(int key) {
    int order = orderOf(key);

    while (true) {
        Window window = find(key, order);
        Node pred = window.pred;
        Node curr = window.curr;

        if (curr != tail && curr.order == order && curr.key == key) {
            return false;
        }

        Node node = new Node(key, order, curr);
        if (pred.next.compareAndSet(curr, node, false, false)) {
            int newSize = size.incrementAndGet();
            tryResize(newSize);
            return true;
        }
    }
}

@Override
public boolean remove(int key) {
    int order = orderOf(key);

    while (true) {
        Window window = find(key, order);
        Node pred = window.pred;
        Node curr = window.curr;

        if (curr == tail || curr.order != order || curr.key != key) {
            return false;
        }

        Node succ = curr.next.getReference();
        if (!curr.next.attemptMark(succ, true)) {
            continue;
        }
        // Node has been marked
        pred.next.compareAndSet(curr, succ, false, false);
        size.decrementAndGet();
        return true;
    }
}

@Override
public boolean contains(int key) {
    int order = orderOf(key);
    Node curr = head.next.getReference();

    while (curr != tail && lessThan(curr, order, key)) {
        curr = curr.next.getReference();
    }

    return curr != tail
            && curr.order == order
            && curr.key == key
            && !curr.next.isMarked();
}

private Window find(int key, int order) {
    retry:
    while (true) {
        Node pred = head;
        Node curr = pred.next.getReference();

        while (true) {
            Node succ = curr.next.getReference();

            while (curr.next.isMarked()) {
                if (!pred.next.compareAndSet(curr, succ, false, false)) {
                    continue retry;
                }
                curr = succ;
                succ = curr.next.getReference();
            }

            if (curr == tail || !lessThan(curr, order, key)) {
                return new Window(pred, curr);
            }

            pred = curr;
            curr = succ;
        }
    }
}

private boolean lessThan(Node node, int order, int key) {
    return node.order < order || (node.order == order && node.key < key);
}

private static int orderOf(int key) {
    int h = key;
    h ^= (h >>> 16);
    h *= 0x7feb352d;
    h ^= (h >>> 15);
    h *= 0x846ca68b;
    h ^= (h >>> 16);
    return h & 0x7fffffff;
}

private void tryResize(int currentSize) {
    while (true) {
        int cap = capacity.get();
        if (currentSize <= cap * LOAD_FACTOR || cap >= (1 << 30)) {
            return;
        }

        int newCap = cap << 1;
        if (!capacity.compareAndSet(cap, newCap)) {
            continue;
        }

        AtomicReferenceArray<AtomicReference<Node>> oldBuckets = buckets.get();
        AtomicReferenceArray<AtomicReference<Node>> newBuckets = new AtomicReferenceArray<>(newCap);

        for (int i = 0; i < newCap; i++) {
            AtomicReference<Node> ref = i < oldBuckets.length()
                    ? oldBuckets.get(i)
                    : new AtomicReference<>(head);
            newBuckets.set(i, ref);
        }

        buckets.compareAndSet(oldBuckets, newBuckets);
        return;
    }
}


}
