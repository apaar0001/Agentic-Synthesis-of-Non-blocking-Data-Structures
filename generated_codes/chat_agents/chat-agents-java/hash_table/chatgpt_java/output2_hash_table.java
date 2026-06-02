package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicReferenceArray;

public class ConcurrentDataStructure implements SetADT {
private static final int DEFAULT_CAPACITY = 1024;
private static final float LOAD_FACTOR = 4.0f;


private final AtomicReference<Table> table;

private static final class Node {
    final int key;
    final long order;
    final AtomicMarkableReference<Node> next;

    Node(int key, long order, Node next) {
        this.key = key;
        this.order = order;
        this.next = new AtomicMarkableReference<>(next, false);
    }
}

private static final class Bucket {
    final AtomicReference<Node> head;

    Bucket() {
        Node tail = new Node(0, Long.MAX_VALUE, null);
        Node sentinel = new Node(0, Long.MIN_VALUE, tail);
        this.head = new AtomicReference<>(sentinel);
    }
}

private static final class Table {
    final int capacity;
    final AtomicReferenceArray<Bucket> buckets;
    final AtomicReference<Table> next;
    final java.util.concurrent.atomic.AtomicInteger size;

    Table(int capacity) {
        this.capacity = capacity;
        this.buckets = new AtomicReferenceArray<>(capacity);
        this.next = new AtomicReference<>(null);
        this.size = new java.util.concurrent.atomic.AtomicInteger(0);
        for (int i = 0; i < capacity; i++) {
            this.buckets.set(i, new Bucket());
        }
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
    this.table = new AtomicReference<>(new Table(DEFAULT_CAPACITY));
}

@Override
public boolean add(int key) {
    while (true) {
        Table t = table.get();
        boolean added = addToTable(t, key);
        if (added) {
            int sz = t.size.incrementAndGet();
            if (sz > (int) (t.capacity * LOAD_FACTOR)) {
                resize(t);
            }
            return true;
        }
        if (containsInTable(t, key)) {
            return false;
        }
    }
}

@Override
public boolean remove(int key) {
    while (true) {
        Table t = table.get();
        Bucket bucket = bucketFor(t, key);
        long order = orderOf(key);

        while (true) {
            Window window = find(bucket, order, key);
            Node pred = window.pred;
            Node curr = window.curr;

            if (curr.order != order || curr.key != key) {
                return false;
            }

            Node succ = curr.next.getReference();
            if (curr.next.attemptMark(succ, true)) {
                // Node has been marked
                pred.next.compareAndSet(curr, succ, false, false);
                t.size.decrementAndGet();
                return true;
            }
        }
    }
}

@Override
public boolean contains(int key) {
    Table t = table.get();
    return containsInTable(t, key);
}

private boolean addToTable(Table t, int key) {
    Bucket bucket = bucketFor(t, key);
    long order = orderOf(key);

    while (true) {
        Window window = find(bucket, order, key);
        Node pred = window.pred;
        Node curr = window.curr;

        if (curr.order == order && curr.key == key) {
            return false;
        }

        Node node = new Node(key, order, curr);
        if (pred.next.compareAndSet(curr, node, false, false)) {
            return true;
        }
    }
}

private boolean containsInTable(Table t, int key) {
    Bucket bucket = bucketFor(t, key);
    long order = orderOf(key);
    Node curr = bucket.head.get().next.getReference();

    while (curr.order < order) {
        curr = curr.next.getReference();
    }

    boolean[] marked = new boolean[1];
    curr.next.get(marked);
    return curr.order == order && curr.key == key && !marked[0];
}

private Window find(Bucket bucket, long order, int key) {
    retry:
    while (true) {
        Node pred = bucket.head.get();
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

            if (curr.order >= order) {
                return new Window(pred, curr);
            }

            pred = curr;
            curr = succ;
        }
    }
}

private Bucket bucketFor(Table t, int key) {
    int index = spread(key) & (t.capacity - 1);
    return t.buckets.get(index);
}

private static long orderOf(int key) {
    return ((long) key) - Integer.MIN_VALUE;
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

private void resize(Table oldTable) {
    if (oldTable.capacity >= (1 << 30)) {
        return;
    }

    Table observed = table.get();
    if (observed != oldTable) {
        return;
    }

    Table newTable = oldTable.next.get();
    if (newTable == null) {
        Table candidate = new Table(oldTable.capacity << 1);
        if (oldTable.next.compareAndSet(null, candidate)) {
            newTable = candidate;
        } else {
            newTable = oldTable.next.get();
        }
    }

    migrate(oldTable, newTable);
    table.compareAndSet(oldTable, newTable);
}

private void migrate(Table oldTable, Table newTable) {
    for (int i = 0; i < oldTable.capacity; i++) {
        Bucket oldBucket = oldTable.buckets.get(i);
        Node curr = oldBucket.head.get().next.getReference();

        while (curr.order != Long.MAX_VALUE) {
            boolean[] marked = new boolean[1];
            curr.next.get(marked);
            if (!marked[0]) {
                addToTable(newTable, curr.key);
            }
            curr = curr.next.getReference();
        }
    }
    newTable.size.set(oldTable.size.get());
}


}
