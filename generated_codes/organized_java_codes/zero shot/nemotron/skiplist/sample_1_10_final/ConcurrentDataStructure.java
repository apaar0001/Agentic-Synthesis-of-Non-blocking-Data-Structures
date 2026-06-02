package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.Random;

public class ConcurrentDataStructure implements SetADT {
    private static final int MAX_LEVEL = 16;
    private static final double P = 0.5;

    private final Node header;
    private final AtomicInteger maxLevel;
    private final Random rand;

    private static class Node {
        int key;
        final AtomicMarkableReference<Node>[] forward;
        Node(int key, int level) {
            this.key = key;
            @SuppressWarnings("unchecked")
            AtomicMarkableReference<Node>[] f = new AtomicMarkableReference[level + 1];
            for (int i = 1; i <= level; i++) {
                f[i] = new AtomicMarkableReference<>(null, false);
            }
            this.forward = f;
        }
    }

    private static class SearchResult {
        final Node[] pred;
        final Node node;
        SearchResult(Node[] pred, Node node) {
            this.pred = pred;
            this.node = node;
        }
    }

    public ConcurrentDataStructure() {
        this.header = new Node(Integer.MIN_VALUE, MAX_LEVEL);
        this.maxLevel = new AtomicInteger(1);
        this.rand = new Random();
    }

    private int randomLevel() {
        int lvl = 1;
        while (rand.nextDouble() < P && lvl < MAX_LEVEL) {
            lvl++;
        }
        return lvl;
    }

    private SearchResult find(int key) {
        while (true) {
            int level = maxLevel.get();
            Node[] pred = new Node[MAX_LEVEL + 1];
            Node[] succ = new Node[MAX_LEVEL + 1];
            Node prev = header;
            outer:
            for (int i = level; i >= 1; i--) {
                Node curr = prev.forward[i].getReference();
                while (true) {
                    if (curr == null) {
                        pred[i] = prev;
                        succ[i] = null;
                        break;
                    }
                    boolean[] marked = {false};
                    Node next = curr.forward[i].get(marked);
                    while (marked[0]) {
                        boolean unlinked = prev.forward[i].compareAndSet(curr, next, false, false);
                        if (!unlinked) {
                            continue outer;
                        }
                        curr = prev.forward[i].getReference();
                        if (curr == null) break;
                        next = curr.forward[i].get(marked);
                    }
                    if (curr.key >= key) {
                        pred[i] = prev;
                        succ[i] = curr;
                        break;
                    }
                    prev = curr;
                    curr = prev.forward[i].getReference();
                }
            }
            boolean consistent = true;
            for (int i = 1; i <= level; i++) {
                boolean[] marked = {false};
                Node succNode = pred[i].forward[i].get(marked);
                if (marked[0] || succNode != succ[i]) {
                    consistent = false;
                    break;
                }
            }
            if (consistent) {
                Node found = succ[1];
                return new SearchResult(pred, found);
            }
        }
    }

    @Override
    public boolean add(int key) {
        while (true) {
            SearchResult res = find(key);
            Node[] pred = res.pred;
            Node node = res.node;
            if (node != null && node.key == key) {
                return false;
            }
            int lvl = randomLevel();
            if (lvl > maxLevel.get()) {
                while (true) {
                    int cur = maxLevel.get();
                    if (lvl <= cur) break;
                    if (maxLevel.compareAndSet(cur, lvl)) {
                        break;
                    }
                }
            }
            Node newNode = new Node(key, lvl);
            for (int i = 1; i <= lvl; i++) {
                newNode.forward[i] = new AtomicMarkableReference<>(pred[i].forward[i].getReference(), false);
            }
            boolean success = true;
            outer:
            for (int i = 1; i <= lvl; i++) {
                while (true) {
                    boolean[] marked = {false};
                    Node expected = pred[i].forward[i].get(marked);
                    if (marked[0]) {
                        boolean unlinked = pred[i].forward[i].compareAndSet(expected, expected.getReference(), false, false);
                        if (!unlinked) {
                            success = false;
                            break outer;
                        }
                        continue;
                    }
                    if (pred[i].forward[i].compareAndSet(expected, newNode, false, false)) {
                        break;
                    }
                }
            }
            if (success) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            SearchResult res = find(key);
            Node[] pred = res.pred;
            Node node = res.node;
            if (node == null || node.key != key) {
                return false;
            }
            boolean snip = pred[0].forward[0].compareAndSet(node, node, false, true);
            if (!snip) {
                continue;
            }
            // Node has been marked
            for (int i = 1; i < node.forward.length; i++) {
                Node succ = node.forward[i].getReference();
                pred[i].forward[i].compareAndSet(node, succ, false, false);
            }
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        SearchResult res = find(key);
        Node node = res.node;
        return node != null && node.key == key;
    }
}