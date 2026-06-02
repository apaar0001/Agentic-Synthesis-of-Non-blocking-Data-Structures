package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicReferenceArray;

public class ConcurrentDataStructure implements SetADT {
private static final int INITIAL_CAPACITY = 1024;
private static final int MAX_CAPACITY = 1 << 30;
private static final int LOAD_FACTOR = 4;


private final AtomicReference<Table> table;

private static final class Node {
    final int key;
    final int hash;
    final AtomicMarkableReference<Node> next;

    Node(int key, int hash, Node next) {
        this.key = key;
        this.hash = hash;
        this.next = new AtomicMarkableReference<>(next, false);
    }
}

private static final class Bucket {
    final AtomicReference<Node> head;

    Bucket() {
        this.head = new AtomicReference<>(null);
    }
}

private static final class Table {
    final int capacity;
    final AtomicReferenceArray<Bucket> buckets;
    final AtomicInteger size;
    final AtomicReference<Table> next;

    Table(int capacity) {
        this.capacity = capacity;
        this.buckets = new AtomicReferenceArray<>(capacity);
        this.size = new AtomicInteger(0);
        this.next = new AtomicReference<>(null);
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
    this.table = new AtomicReference<>(new Table(INITIAL_CAPACITY));
}

@Override
public boolean add(int key) {
    while (true) {
        Table t = table.get();
        if (addToTable(t, key)) {
            int currentSize = t.size.incrementAndGet();
            if (currentSize > t.capacity * LOAD_FACTOR) {
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
        int hash = spread(key);

        while (true) {
            Window window = find(bucket, hash, key);
            Node pred = window.pred;
            Node curr = window.curr;

            if (curr == null || curr.hash != hash || curr.key != key) {
                return false;
            }

            Node succ = curr.next.getReference();
            if (curr.next.compareAndSet(succ, succ, false, true)) {
                // Node has been marked
                if (pred == null) {
                    bucket.head.compareAndSet(curr, succ);
                } else {
                    pred.next.compareAndSet(curr, succ, false, false);
                }
                t.size.decrementAndGet();
                return true;
            }
        }
    }
}

@Override
public boolean contains(int key) {
    return containsInTable(table.get(), key);
}

private boolean addToTable(Table t, int key) {
    Bucket bucket = bucketFor(t, key);
    int hash = spread(key);

    while (true) {
        Window window = find(bucket, hash, key);
        Node pred = window.pred;
        Node curr = window.curr;

        if (curr != null && curr.hash == hash && curr.key == key) {
            return false;
        }

        Node node = new Node(key, hash, curr);
        if (pred == null) {
            if (bucket.head.compareAndSet(curr, node)) {
                return true;
            }
        } else {
            if (pred.next.compareAndSet(curr, node, false, false)) {
                return true;
            }
        }
    }
}

private boolean containsInTable(Table t, int key) {
    Bucket bucket = bucketFor(t, key);
    int hash = spread(key);
    Node curr = bucket.head.get();

    while (curr != null) {
        boolean[] marked = new boolean[1];
        Node succ = curr.next.get(marked);

        if (!marked[0]) {
            if (curr.hash == hash && curr.key == key) {
                return true;
            }
            if (curr.hash > hash) {
                return false;
            }
        }

        curr = succ;
    }

    return false;
}

private Window find(Bucket bucket, int hash, int key) {
    retry:
    while (true) {
        Node pred = null;
        Node curr = bucket.head.get();

        while (curr != null) {
            boolean[] marked = new boolean[1];
            Node succ = curr.next.get(marked);

            while (marked[0]) {
                boolean snipped;
                if (pred == null) {
                    snipped = bucket.head.compareAndSet(curr, succ);
                } else {
                    snipped = pred.next.compareAndSet(curr, succ, false, false);
                }

                if (!snipped) {
                    continue retry;
                }

                curr = succ;
                if (curr == null) {
                    return new Window(pred, null);
                }
                succ = curr.next.get(marked);
            }

            if (curr.hash > hash || (curr.hash == hash && curr.key >= key)) {
                return new Window(pred, curr);
            }

            pred = curr;
            curr = succ;
        }

        return new Window(pred, null);
    }
}

private Bucket bucketFor(Table t, int key) {
    return t.buckets.get(spread(key) & (t.capacity - 1));
}

private void resize(Table oldTable) {
    if (oldTable.capacity >= MAX_CAPACITY) {
        return;
    }

    if (table.get() != oldTable) {
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

    for (int i = 0; i < oldTable.capacity; i++) {
        Node curr = oldTable.buckets.get(i).head.get();
        while (curr != null) {
            boolean[] marked = new boolean[1];
            Node succ = curr.next.get(marked);
            if (!marked[0]) {
                addToTable(newTable, curr.key);
            }
            curr = succ;
        }
    }

    newTable.size.set(Math.max(newTable.size.get(), oldTable.size.get()));
    table.compareAndSet(oldTable, newTable);
}

private static int spread(int key) {
    int h = key;
    h ^= h >>> 16;
    h *= 0x7feb352d;
    h ^= h >>> 15;
    h *= 0x846ca68b;
    h ^= h >>> 16;
    return h & 0x7fffffff;
}


}
