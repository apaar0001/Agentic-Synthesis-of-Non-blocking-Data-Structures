package com.example.Sets;
import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static final int MAX_LEVEL = 16;
    private static final double P = 0.5;
    private final AtomicInteger level;
    private final Node header;

    private class Node {
        int key;
        final AtomicMarkableReference<Node>[] forward;
        Node(int key, int lvl) {
            this.key = key;
            forward = new AtomicMarkableReference[lvl + 1];
            for (int i = 0; i <= lvl; i++) {
                forward[i] = new AtomicMarkableReference<>(null, false);
            }
        }
    }

    public ConcurrentDataStructure() {
        level = new AtomicInteger(1);
        header = new Node(Integer.MIN_VALUE, MAX_LEVEL);
    }

    private int randomLevel() {
        int lvl = 1;
        while (Math.random() < P && lvl < MAX_LEVEL) {
            lvl++;
        }
        return lvl;
    }

    private Node[] find(int key) {
        while (true) {
            Node[] preds = new Node[MAX_LEVEL + 1];
            Node pred = header;
            boolean restart = false;
            for (int lvl = MAX_LEVEL; lvl >= 0; lvl--) {
                Node curr = pred.forward[lvl].getReference();
                while (true) {
                    if (curr == null) {
                        preds[lvl] = pred;
                        break;
                    }
                    boolean marked = curr.forward[lvl].isMarked();
                    Node next = curr.forward[lvl].getReference();
                    if (marked) {
                        if (pred.forward[lvl].compareAndSet(curr, next, false, false)) {
                            curr = next;
                            continue;
                        } else {
                            restart = true;
                            break;
                        }
                    }
                    if (curr.key < key) {
                        pred = curr;
                        curr = next;
                    } else {
                        preds[lvl] = pred;
                        break;
                    }
                }
                if (restart) break;
            }
            if (!restart) {
                return preds;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node[] preds = find(key);
        Node node = preds[0].forward[0].getReference();
        return node != null && node.key == key && !preds[0].forward[0].isMarked();
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node[] preds = find(key);
            Node node = preds[0].forward[0].getReference();
            if (node != null && node.key == key) {
                if (!preds[0].forward[0].isMarked()) {
                    return false;
                }
                continue;
            }
            int newLvl = randomLevel();
            Node newNode = new Node(key, newLvl);
            for (int i = 0; i <= newLvl; i++) {
                newNode.forward[i].set(preds[i].forward[i].getReference(), false);
            }
            boolean ok = true;
            for (int i = 0; i <= newLvl; i++) {
                if (!preds[i].forward[i].compareAndSet(
                        newNode.forward[i].getReference(), newNode, false, false)) {
                    ok = false;
                    break;
                }
            }
            if (ok) {
                if (newLvl > level.get()) {
                    level.compareAndSet(level.get(), newLvl);
                }
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node[] preds = find(key);
            Node node = preds[0].forward[0].getReference();
            if (node == null || node.key != key) {
                return false;
            }
            if (preds[0].forward[0].isMarked()) {
                return false;
            }
            boolean marked = preds[0].forward[0].compareAndSet(node, node, false, true);
            if (!marked) {
                continue;
            }
            // Node has been marked
            int maxLvl = node.forward.length - 1;
            for (int i = 1; i <= maxLvl; i++) {
                Node succ = node.forward[i].getReference();
                while (true) {
                    boolean marked_i = node.forward[i].isMarked();
                    if (marked_i) {
                        break;
                    }
                    Node pred_i = preds[i];
                    if (pred_i.forward[i].compareAndSet(node, succ, false, false)) {
                        break;
                    } else {
                        preds = find(key);
                        if (preds[0].forward[0].getReference() != node) {
                            return true;
                        }
                    }
                }
            }
            return true;
        }
    }
}