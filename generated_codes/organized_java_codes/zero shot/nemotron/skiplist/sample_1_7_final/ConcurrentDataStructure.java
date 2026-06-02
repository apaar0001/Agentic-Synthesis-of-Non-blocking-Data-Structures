package com.example.Sets;
import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.Random;

public class ConcurrentDataStructure implements SetADT {
    private static final int MAX_LEVEL = 16;
    private static final double P = 0.5;
    private static final int MIN_KEY = Integer.MIN_VALUE;
    private static final int MAX_KEY = Integer.MAX_VALUE;

    private final Node header;
    private final Node tail;
    private final AtomicInteger topLevel;
    private final Random rand;

    public ConcurrentDataStructure() {
        topLevel = new AtomicInteger(1);
        rand = new Random();
        header = new Node(MIN_KEY);
        tail = new Node(MAX_KEY);
        for (int i = 1; i <= MAX_LEVEL; i++) {
            header.forward[i].set(tail, false);
            tail.forward[i].set(null, false);
        }
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node[] pred = new Node[MAX_LEVEL + 1];
            Node[] succ = new Node[MAX_LEVEL + 1];
            Node node = find(key, pred, succ);
            if (node != null && node.key == key && !isMarked(node.forward[1])) {
                return false;
            }
            int level = randomLevel();
            Node newNode = new Node(key);
            for (int i = 1; i <= level; i++) {
                newNode.forward[i].set(succ[i], false);
            }
            boolean ok = true;
            for (int i = 1; i <= level; i++) {
                if (!pred[i].forward[i].compareAndSet(succ[i], newNode, false, false)) {
                    ok = false;
                    break;
                }
            }
            if (ok) {
                if (level > topLevel.get()) {
                    topLevel.compareAndSet(topLevel.get(), level);
                }
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node[] pred = new Node[MAX_LEVEL + 1];
            Node[] succ = new Node[MAX_LEVEL + 1];
            Node node = find(key, pred, succ);
            if (node == null || node.key != key || isMarked(node.forward[1])) {
                return false;
            }
            boolean marked = pred[1].forward[1].attemptMark(node, true);
            if (!marked) {
                continue;
            }
            // Node has been marked
            for (int i = 2; i <= topLevel.get(); i++) {
                Node succNode = node.forward[i].getReference();
                pred[i].forward[i].compareAndSet(node, succNode, false, false);
            }
            while (topLevel.get() > 1 && header.forward[topLevel.get()].getReference() == tail) {
                topLevel.compareAndSet(topLevel.get(), topLevel.get() - 1);
            }
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        Node[] pred = new Node[MAX_LEVEL + 1];
        Node[] succ = new Node[MAX_LEVEL + 1];
        Node node = find(key, pred, succ);
        return node != null && node.key == key && !isMarked(node.forward[1]);
    }

    private Node find(int key, Node[] pred, Node[] succ) {
        int level = topLevel.get();
        Node predNode = header;
        for (int i = level; i >= 1; i--) {
            Node curr = predNode.forward[i].getReference();
            while (true) {
                if (curr == null) {
                    pred[i] = predNode;
                    succ[i] = null;
                    break;
                }
                AtomicMarkableReference<Node> ref = predNode.forward[i];
                Node next = curr.forward[i].getReference();
                boolean marked = ref.isMarked();
                if (marked) {
                    Node succNode = curr.forward[i].getReference();
                    if (predNode.forward[i].compareAndSet(ref, new AtomicMarkableReference<>(succNode, false), false, false)) {
                        curr = succNode;
                        continue;
                    } else {
                        break;
                    }
                }
                if (next != null && next.key < key) {
                    predNode = curr;
                    curr = next;
                } else {
                    pred[i] = predNode;
                    succ[i] = next;
                    break;
                }
            }
            predNode = pred[i];
        }
        return succ[1];
    }

    private boolean isMarked(AtomicMarkableReference<Node> ref) {
        return ref.isMarked();
    }

    private int randomLevel() {
        int lvl = 1;
        while (rand.nextDouble() < P && lvl < MAX_LEVEL) {
            lvl++;
        }
        return lvl;
    }

    private static class Node {
        int key;
        AtomicMarkableReference<Node>[] forward;

        Node(int key) {
            this.key = key;
            this.forward = new AtomicMarkableReference[MAX_LEVEL + 1];
            for (int i = 0; i <= MAX_LEVEL; i++) {
                this.forward[i] = new AtomicMarkableReference<>(null, false);
            }
        }
    }
}