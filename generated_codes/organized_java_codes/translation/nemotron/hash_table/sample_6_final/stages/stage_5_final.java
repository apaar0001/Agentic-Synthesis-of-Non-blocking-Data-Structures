package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicInteger;

public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        int key;
        AtomicMarkableReference<Node> next;

        Node(int key) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, false);
        }
    }

    private final Node head;
    private final AtomicInteger level;
    private final AtomicInteger size;
    private static final double LOAD_FACTOR = 0.75f;
    private static final int MAX_LEVEL = 30;

    public ConcurrentDataStructure() {
        this.head = new Node(0);
        this.head.next = new AtomicMarkableReference<>(null, false);
        this.level = new AtomicInteger(4);
        this.size = new AtomicInteger(0);
    }

    private long splitOrder(int key) {
        return ((long) Integer.reverse(key)) & 0xffffffffL;
    }

    private void maybeResize() {
        int currentSize = size.get();
        int currentLevel = level.get();
        int capacity = 1 << currentLevel;
        if (currentSize > (int) (capacity * LOAD_FACTOR)) {
            int newLevel = Math.min(currentLevel + 1, MAX_LEVEL);
            level.compareAndSet(currentLevel, newLevel);
        }
    }

    @Override
    public boolean add(int key) {
        long h = splitOrder(key);
        while (true) {
            Node pred = head;
            AtomicMarkableReference<Node> predNext = pred.next;
            while (true) {
                Object[] nextAndMark = predNext.get();
                Node curr = (Node) nextAndMark[0];
                boolean marked = (boolean) nextAndMark[1];
                if (marked) {
                    Node succ = curr.next.getReference();
                    if (predNext.compareAndSet(curr, false, succ, false)) {
                        continue;
                    } else {
                        break;
                    }
                }
                if (curr == null || splitOrder(curr.key) > h) {
                    break;
                }
                if (splitOrder(curr.key) == h && curr.key == key) {
                    return false;
                }
                pred = curr;
                predNext = pred.next;
            }
            Node curr = (Node) ((Object[]) predNext.get())[0];
            Node newNode = new Node(key);
            newNode.next = new AtomicMarkableReference<>(curr, false);
            if (predNext.compareAndSet(curr, false, newNode, false)) {
                size.incrementAndGet();
                maybeResize();
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        long h = splitOrder(key);
        Node pred = head;
        while (true) {
            Object[] nextAndMark = pred.next.get();
            Node