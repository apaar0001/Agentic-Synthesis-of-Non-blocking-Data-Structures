package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.Random;

/**
 * Lock-free Skip List — Different Names variant.
 * Names: Tower / levels[] / item / MAX_H=31 / headTower/tailTower / locate()
 *
 * Distinct from all other skip list refs:
 * - Node class: "Tower" (not "Node")
 * - Link array: "levels" (not "next" or "forward")
 * - Key field: "item" (not "key" or "val")
 * - Sentinels: "headTower" / "tailTower"
 * - Traversal: "locate()" (not "find()" or "search()")
 * - Level constant: MAX_H=31
 */
public class SkipListDifferentNames implements SetADT {

    private static final int MAX_H = 31;

    private static class Tower {
        final int item;
        @SuppressWarnings("unchecked")
        final AtomicMarkableReference<Tower>[] levels;

        @SuppressWarnings("unchecked")
        Tower(int i, int h) {
            this.item = i;
            this.levels = new AtomicMarkableReference[h + 1];
            for (int k = 0; k <= h; k++)
                levels[k] = new AtomicMarkableReference<>(null, false);
        }
    }

    private final Tower headTower;
    private final Tower tailTower;
    private final Random rng = new Random();

    public SkipListDifferentNames() {
        headTower = new Tower(Integer.MIN_VALUE, MAX_H);
        tailTower = new Tower(Integer.MAX_VALUE, MAX_H);
        for (int i = 0; i <= MAX_H; i++)
            headTower.levels[i].set(tailTower, false);
    }

    private boolean locate(int key, Tower[] p, Tower[] s) {
        boolean[] m = { false };
        retry: while (true) {
            Tower pt = headTower;
            for (int i = MAX_H; i >= 0; i--) {
                Tower ct = pt.levels[i].getReference();
                while (true) {
                    Tower nt = ct.levels[i].get(m);
                    while (m[0]) {
                        if (!pt.levels[i].compareAndSet(ct, nt, false, false))
                            continue retry;
                        ct = nt;
                        nt = ct.levels[i].get(m);
                    }
                    if (ct.item < key) {
                        pt = ct;
                        ct = nt;
                    } else
                        break;
                }
                p[i] = pt;
                s[i] = ct;
            }
            return s[0].item == key;
        }
    }

    @Override
    public boolean add(int key) {
        int h = rnd();
        Tower[] p = new Tower[MAX_H + 1], s = new Tower[MAX_H + 1];
        while (true) {
            if (locate(key, p, s))
                return false;
            Tower newT = new Tower(key, h);
            for (int i = 0; i <= h; i++)
                newT.levels[i].set(s[i], false);
            if (!p[0].levels[0].compareAndSet(s[0], newT, false, false))
                continue;
            for (int i = 1; i <= h; i++) {
                while (!p[i].levels[i].compareAndSet(s[i], newT, false, false))
                    locate(key, p, s);
            }
            return true;
        }
    }

    @Override
    public boolean remove(int key) {
        Tower[] p = new Tower[MAX_H + 1], s = new Tower[MAX_H + 1];
        while (true) {
            if (!locate(key, p, s))
                return false;
            Tower t = s[0];
            // Mark upper levels
            for (int i = t.levels.length - 1; i >= 1; i--) {
                boolean[] m = { false };
                Tower nt = t.levels[i].get(m);
                while (!m[0]) {
                    t.levels[i].compareAndSet(nt, nt, false, true);
                    nt = t.levels[i].get(m);
                }
            }
            boolean[] m = { false };
            Tower nt = t.levels[0].get(m);
            while (true) {
                if (t.levels[0].compareAndSet(nt, nt, false, true)) {
                    // Node has been marked
                    locate(key, p, s);
                    return true;
                }
                nt = t.levels[0].get(m);
                if (m[0])
                    return false;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        boolean[] m = { false };
        Tower pt = headTower;
        for (int i = MAX_H; i >= 0; i--) {
            Tower ct = pt.levels[i].getReference();
            while (true) {
                Tower nt = ct.levels[i].get(m);
                while (m[0]) {
                    ct = nt;
                    nt = ct.levels[i].get(m);
                }
                if (ct.item < key) {
                    pt = ct;
                    ct = nt;
                } else
                    break;
            }
        }
        Tower ct = pt.levels[0].getReference();
        ct.levels[0].get(m);
        return ct.item == key && !m[0];
    }

    private int rnd() {
        int l = 0;
        while (l < MAX_H && rng.nextInt(2) == 0)
            l++;
        return l;
    }
}
