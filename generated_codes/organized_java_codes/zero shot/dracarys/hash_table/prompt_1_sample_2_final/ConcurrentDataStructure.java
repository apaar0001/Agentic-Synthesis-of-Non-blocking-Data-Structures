package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        int key;
        AtomicMarkableReference<Node> next;
        AtomicMarkableReference<Node> prev;

        public Node(int key) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, false);
            this.prev = new AtomicMarkableReference<>(null, false);
        }
    }

    private static class Bucket {
        AtomicMarkableReference<Node> head;

        public Bucket() {
            this.head = new AtomicMarkableReference<>(null, false);
        }
    }

    private static final int CAPACITY = 16;
    private Bucket[] buckets;

    public ConcurrentDataStructure() {
        buckets = new Bucket[CAPACITY];

        for (int i = 0; i < CAPACITY; i++) {
            buckets[i] = new Bucket();
        }
    }

    private int hash(int key) {
        return Math.abs(key) % CAPACITY;
    }

    private boolean casNext(Node node, Node expected, Node newNode) {
        return node.next.compareAndSet(expected, newNode, false, false);
    }

    private boolean casPrev(Node node, Node expected, Node newNode) {
        return node.prev.compareAndSet(expected, newNode, false, false);
    }

    private boolean casHead(Bucket bucket, Node expected, Node newNode) {
        return bucket.head.compareAndSet(expected, newNode, false, false);
    }

    private Node getHead(Bucket bucket) {
        return bucket.head.getReference();
    }

    private Node getNext(Node node) {
        return node.next.getReference();
    }

    private Node getPrev(Node node) {
        return node.prev.getReference();
    }

    private boolean isMarked(Node node) {
        return node.next.isMarked() || node.prev.isMarked();
    }

    private boolean markNode(Node node) {
        return node.next.attemptMark(null, true);
    }

    private Node findNode(Bucket bucket, int key) {
        Node curr = getHead(bucket);
        Node prev = null;

        while (curr != null) {
            if (isMarked(curr)) {
                if (prev != null) {
                    casNext(prev, curr, getNext(curr));
                } else {
                    casHead(bucket, curr, getNext(curr));
                }
                continue;
            }

            if (curr.key == key) {
                return curr;
            }

            prev = curr;
            curr = getNext(curr);
        }

        return null;
    }

    @Override
    public boolean add(int key) {
        int index = hash(key);
        Bucket bucket = buckets[index];

        while (true) {
            Node curr = findNode(bucket, key);

            if (curr != null) {
                return false;
            }

            Node newNode = new Node(key);
            Node head = getHead(bucket);

            newNode.next.set(head, false);
            newNode.prev.set(null, false);

            if (casHead(bucket, head, newNode)) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int index = hash(key);
        Bucket bucket = buckets[index];

        while (true) {
            Node curr = findNode(bucket, key);

            if (curr == null) {
                return false;
            }

            Node next = getNext(curr);
            Node prev = getPrev(curr);

            if (markNode(curr)) {
                if (prev != null) {
                    casNext(prev, curr, next);
                } else {
                    casHead(bucket, curr, next);
                }
                // Node has been marked
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        int index = hash(key);
        Bucket bucket = buckets[index];

        while (true) {
            Node curr = findNode(bucket, key);

            if (curr != null && !isMarked(curr)) {
                return true;
            }

            return false;
        }
    }
}