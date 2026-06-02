package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.Random;

public class ConcurrentDataStructure implements SetADT {
    private static final int MAX_LEVEL = 16;
    private static final double P = 0.5;
    private final AtomicInteger level = new AtomicInteger(1);
    private final Node header;
    private final Random rand;

    public ConcurrentDataStructure() {
        header = new Node(MAX_LEVEL, 0);
        rand = new Random();
    }

    private int randomLevel() {
        int lvl = 1;
        while (rand.nextDouble() < P && lvl < MAX_LEVEL) {
            lvl++;
        }
        return lvl;
    }

    @Override
    public boolean contains(int key) {
        Node pred = header;
        for (int i = level.get(); i >= 1; i--) {
            Node curr = pred.forward[i].getReference();
            while (curr != null) {
                boolean[] mark = {false};
                Node next = curr.forward[i].get(mark);
                if (mark[0]) {
                    pred.forward[i].compareAndSet(curr, next, false, false);
                    curr = pred.forward[i].getReference();
                    continue;
                }
                if (curr.key >= key) break;
                pred = curr;
                curr = pred.forward[i].getReference();
            }
        }
        boolean[] mark = {false};
        Node curr = pred.forward[1].getReference();
        if (curr != null) {
            curr.forward[1].get(mark);
        }
        return curr != null && curr.key == key && !mark[0];
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node[] preds = new Node[MAX_LEVEL + 1];
            Node[] succs = new Node[MAX_LEVEL + 1];
            Node pred = header;
            for (int i = level.get(); i >= 1; i--) {
                Node curr = pred.forward[i].getReference();
                while (curr != null) {
                    boolean[] mark = {false};
                    Node next = curr.forward[i].get(mark);
                    if (mark[0]) {
                        pred.forward[i].compareAndSet(curr, next, false, false);
                        curr = pred.forward[i].getReference();
                        continue;
                    }
                    if (curr.key >= key) break;
                    pred = curr;
                    curr = pred.forward[i].getReference();
                }
                preds[i] = pred;
                succs[i] = curr;
            }
            Node pred0 = preds[1];
            boolean[] mark = {false};
            Node curr0 = pred0.forward[1].getReference();
            if (curr0 != null) {
                curr0.forward[1].get(mark);
            }
            if (curr0 != null && curr0.key == key) {
                if (!mark[0]) return false;
                continue;
            }
            int lvl = randomLevel();
            if (lvl > level.get()) {
                for (int i = level.get() + 1; i <= lvl; i++) {
                    preds[i] = header;
                    succs[i] = null;
                }
            }
            Node newNode = new Node(lvl, key);
            for (int i = 1; i <= lvl; i++) {
                newNode.forward[i].set(succs[i], false);
            }
            boolean ok = true;
            for (int i = 1; i <= lvl; i++) {
                if (!preds[i].forward[i].compareAndSet(succs[i], newNode, false, false)) {
                    ok = false;
                    break;
                }
            }
            if (!ok) continue;
            if (lvl > level.get()) {
                level.compareAndSet(level.get(), lvl);
            }
            return true;
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node[] preds = new Node[MAX_LEVEL + 1];
            Node[] succs = new Node[MAX_LEVEL + 1];
            Node pred = header;
            for (int i = level.get(); i >= 1; i--) {
                Node curr = pred.forward[i].getReference();
                while (curr != null) {
                    boolean[] mark = {false};
                    Node next = curr.forward[i].get(mark);
                    if (mark[0]) {
                        pred.forward[i].compareAndSet(curr, next, false, false);
                        curr = pred.forward[i].getReference();
                        continue;
                    }
                    if (curr.key >= key) break;
                    pred = curr;
                    curr = pred.forward[i].getReference();
                }
                preds[i] = pred;
                succs[i] = curr;
            }
            Node pred0 = preds[1];
            boolean[] mark = {false};
            Node curr0 = pred0.forward[1].getReference();
            if (curr0 == null) return false;
            curr0.forward[1].get(mark);
            if (curr0.key != key || mark[0]) return false;
            if (!curr0.forward[1].compareAndSet(curr0.forward[1].getReference(), curr0.forward[1].getReference(), false, true)) {
                continue;
            }
            // Node has been marked
            for (int i = 1; i <= level.get(); i++) {
                Node predI = preds[i];
                while (true) {
                    boolean[] markI = {false};
                    Node succI = predI.forward[i].getReference();
                    if (succI == null) break;
                    Node nextI = succI.forward[i].get(markI);
                    if (markI[0]) {
                        if (!predI.forward[i].compareAndSet(succI, nextI, false, false)) {
                            break;
                        }
                        continue;
                    }
                    if (succI == curr0) {
                        if (!predI.forward[i].compareAndSet(curr0, nextI, false, false)) {
                            break;
                        }
                        break;
                    }
                    if (succI.key > curr0.key) break;
                    predI = succI;
                }
            }
            while (level.get() > 1 && header.forward[level.get()].getReference() == null) {
                level.compareAndSet(level.get(), level.get() - 1);
            }
            return true;
        }
    }

    private static class Node {
        int key;
        AtomicMarkableReference<Node>[] forward;
        Node(int level, int key) {
            this.key = key;
            forward = new AtomicMarkableReference[level + 1];
            for (int i = 0; i <= level; i++) {
                forward[i] = new AtomicMarkableReference<>(null, false);
            }
        }
    }
}