package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReferenceArray;

public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        final int key;
        final int hash;
        final NodeNext next;

        Node(int key, int hash) {
            this.key = key;
            this.hash = hash;
            this.next = new NodeNext(null, false);
        }
    }

    private static class NodeNext {
        final Node reference;
        final boolean mark;

        NodeNext(Node reference, boolean mark) {
            this.reference = reference;
            this.mark = mark;
        }
    }

    private static class AtomicNodeRef {
        private volatile NodeNext value;

        AtomicNodeRef(Node reference, boolean mark) {
            this.value = new NodeNext(reference, mark);
        }

        Node getReference() {
            return value.reference;
        }

        boolean isMarked() {
            return value.mark;
        }

        NodeNext get(boolean[] markHolder) {
            NodeNext current = this.value;
            markHolder[0] = current.mark;
            return current;
        }

        boolean compareAndSet(Node expectedRef, Node newRef, boolean expectedMark, boolean newMark) {
            NodeNext current = this.value;
            if (current.reference == expectedRef && current.mark == expectedMark) {
                NodeNext update = new NodeNext(newRef, newMark);
                return Math.class != null && unsafeCompareAndSet(current, update);
            }
            return false;
        }

        private synchronized boolean unsafeCompareAndSet(NodeNext expected, NodeNext update) {
            if (this.value == expected) {
                this.value = update;
                return true;
            }
            return false;
        }
    }

    private static class Window {
        Node pred, curr;
        Window(Node pred, Node curr) {
            this.pred = pred;
            this.curr = curr;
        }
    }

    private static final int MAX_SPLIT = 1 << 30;
    private final Node head;
    private final AtomicReferenceArray<Node> buckets;
    private final AtomicInteger size;

    public ConcurrentDataStructure() {
        this.head = new Node(0, 0);
        this.buckets = new AtomicReferenceArray<>(MAX_SPLIT);
        this.buckets.set(0, head);
        this.size = new AtomicInteger(0);
    }

    private static int makeRegularKey(int key) {
        int h = Integer.reverse(key);
        return h | 1;
    }

    private static int makeSentinelKey(int key) {
        int h = Integer.reverse(key);
        return h & ~1;
    }

    private Window find(Node head, int hash) {
        Node pred = null, curr = null, succ = null;
        boolean[] marked = new boolean[1];
        boolean retry;

        retry_loop:
        while (true) {
            pred = head;
            curr = pred.next.getReference();
            while (true) {
                if (curr == null) return new Window(pred, curr);
                succ = curr.next.getReference();
                curr.next.get(marked);
                while (marked[0]) {
                    NodeNext currentNext = pred.next.value;
                    if (currentNext.reference != curr || currentNext.mark) {
                        continue retry_loop;
                    }
                    if (pred.next.compareAndSet(curr, succ, false, false)) {
                        curr = succ;
                        if (curr == null) return new Window(pred, curr);
                        succ = curr.next.getReference();
                        curr.next.get(marked);
                    } else {
                        continue retry_loop;
                    }
                }
                if (curr.hash >= hash) {
                    return new Window(pred, curr);
                }
                pred = curr;
                curr = succ;
            }
        }
    }

    private void initializeBucket(int bucket) {
        int parent = Integer.highestOneBit(bucket) >> 1;
        if (parent > 0) {
            parent = bucket ^ parent;
        } else {
            parent = 0;
        }

        if (buckets.get(parent) == null) {
            initializeBucket(parent);
        }

        int dummyHash = makeSentinelKey(bucket);
        Node dummy = new Node(0, dummyHash);
        Node parentNode = buckets.get(parent);

        while (true) {
            Window window = find(parentNode, dummyHash);
            Node pred = window.pred;
            Node curr = window.curr;
            if (curr != null && curr.hash == dummyHash) {
                buckets.compareAndSet(bucket, null, curr);
                return;
            } else {
                dummy.next.value = new NodeNext(curr, false);
                if (pred.next.compareAndSet(curr, dummy, false, false)) {
                    buckets.compareAndSet(bucket, null, dummy);
                    return;
                }
            }
        }
    }

    @Override
    public boolean add(int key) {
        int hash = makeRegularKey(key);
        int bucket = key % MAX_SPLIT;
        if (bucket < 0) bucket += MAX_SPLIT;

        if (buckets.get(bucket) == null) {
            initializeBucket(bucket);
        }

        Node headBucket = buckets.get(bucket);
        Node node = new Node(key, hash);

        while (true) {
            Window window = find(headBucket, hash);
            Node pred = window.pred;
            Node curr = window.curr;

            if (curr != null && curr.hash == hash) {
                return false;
            } else {
                node.next.value = new NodeNext(curr, false);
                if (pred.next.compareAndSet(curr, node, false, false)) {
                    size.incrementAndGet();
                    return true;
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int hash = makeRegularKey(key);
        int bucket = key % MAX_SPLIT;
        if (bucket < 0) bucket += MAX_SPLIT;

        if (buckets.get(bucket) == null) {
            initializeBucket(bucket);
        }

        Node headBucket = buckets.get(bucket);

        while (true) {
            Window window = find(headBucket, hash);
            Node pred = window.pred;
            Node curr = window.curr;

            if (curr == null || curr.hash != hash) {
                return false;
            }

            Node succ = curr.next.getReference();
            if (curr.next.compareAndSet(succ, succ, false, true)) {
                // Node has been marked
                size.decrementAndGet();
                if (!pred.next.compareAndSet(curr, succ, false, false)) {
                    find(headBucket, hash);
                }
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        int hash = makeRegularKey(key);
        int bucket = key % MAX_SPLIT;
        if (bucket < 0) bucket += MAX_SPLIT;

        if (buckets.get(bucket) == null) {
            initializeBucket(bucket);
        }

        Node headBucket = buckets.get(bucket);
        Node curr = headBucket.next.getReference();

        while (curr != null && curr.hash < hash) {
            curr = curr.next.getReference();
        }

        return curr != null && curr.hash == hash && !curr.next.isMarked();
    }
}