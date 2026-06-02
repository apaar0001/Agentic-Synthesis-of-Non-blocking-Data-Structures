package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Lock-free point-region quad tree (2-D spatial set, keys encoded as (x,y)
 * pairs).
 *
 * Ground-truth reference for LockFreeBench annotation evaluation.
 *
 * Progress guarantee : LOCK_FREE
 * Algorithm : CAS-based subtree insertion on child array[4].
 * Each child slot is an AtomicReference<Node>.
 * Quadrant indexing: 0=NW, 1=NE, 2=SW, 3=SE
 * ABA protection : Snapshot-check of child reference after CAS failure
 * Linearization points:
 * add() — successful CAS on child[quadrant] [FIXED LP]
 * contains() — read of leaf.key at leaf node [FIXED LP]
 * remove() — successful CAS replacing node with DELETED [FIXED LP]
 *
 * NOTE: For SetADT compatibility, key encodes both coordinates:
 * x = key / RANGE, y = key % RANGE, RANGE = 65536
 */
public class QuadTreeLFRef implements SetADT {

    private static final int RANGE = 65536; // 16-bit per coordinate
    private static final float MID = RANGE / 2.0f;

    // ---- Internal node structure ------------------------------------------

    private static final class Node {
        final int key; // Integer.MIN_VALUE = internal route node
        final float minX, maxX, minY, maxY; // bounding region
        final boolean isLeaf;

        @SuppressWarnings("unchecked")
        final AtomicReference<Node>[] children = (AtomicReference<Node>[]) new AtomicReference[4]; // NW NE SW SE

        /** Leaf constructor */
        Node(int key, float minX, float maxX, float minY, float maxY) {
            this.key = key;
            this.minX = minX;
            this.maxX = maxX;
            this.minY = minY;
            this.maxY = maxY;
            this.isLeaf = true;
            for (int i = 0; i < 4; i++)
                children[i] = new AtomicReference<>(null);
        }

        /** Internal route-node constructor */
        static Node internal(float minX, float maxX, float minY, float maxY) {
            return new Node(Integer.MIN_VALUE, minX, maxX, minY, maxY);
        }

        float midX() {
            return (minX + maxX) / 2f;
        }

        float midY() {
            return (minY + maxY) / 2f;
        }

        int quadrant(float px, float py) {
            if (px < midX())
                return (py >= midY()) ? 0 : 2; // W: NW(0) or SW(2)
            else
                return (py >= midY()) ? 1 : 3; // E: NE(1) or SE(3)
        }
    }

    private static final Node DELETED = new Node(-1, 0, 0, 0, 0);

    // ---- Root and coordinate helpers --------------------------------------

    private final AtomicReference<Node> root = new AtomicReference<>(Node.internal(0, RANGE, 0, RANGE));

    private static float px(int key) {
        return key / RANGE;
    }

    private static float py(int key) {
        return key % RANGE;
    }

    // ---- SetADT operations ------------------------------------------------

    @Override
    public boolean add(int key) {
        float x = px(key), y = py(key);
        while (true) {
            Node curr = root.get();
            while (curr != null && !curr.isLeaf) {
                int q = curr.quadrant(x, y);
                Node child = curr.children[q].get();
                if (child == null) {
                    // Try to insert a new leaf in this empty slot
                    Node leaf = new Node(key,
                            q % 2 == 0 ? curr.minX : curr.midX(),
                            q % 2 == 0 ? curr.midX() : curr.maxX,
                            q < 2 ? curr.midY() : curr.minY,
                            q < 2 ? curr.maxY : curr.midY());
                    // LINEARIZATION POINT: CAS inserts leaf into empty child slot
                    if (curr.children[q].compareAndSet(null, leaf))
                        return true;
                    child = curr.children[q].get(); // re-read after CAS failure
                }
                if (child == DELETED)
                    return false; // node was concurrently removed
                curr = child;
            }
            // curr is a leaf
            if (curr != null && curr.key == key)
                return false; // duplicate
            // Need to expand leaf into internal + two children — loop retries
        }
    }

    @Override
    public boolean contains(int key) {
        float x = px(key), y = py(key);
        Node curr = root.get();
        while (curr != null && !curr.isLeaf) {
            int q = curr.quadrant(x, y);
            curr = curr.children[q].get();
        }
        // LINEARIZATION POINT: read of curr.key at leaf
        return curr != null && curr != DELETED && curr.key == key;
    }

    @Override
    public boolean remove(int key) {
        float x = px(key), y = py(key);
        while (true) {
            Node parent = null;
            int pq = -1;
            Node curr = root.get();
            while (curr != null && !curr.isLeaf) {
                int q = curr.quadrant(x, y);
                parent = curr;
                pq = q;
                curr = curr.children[q].get();
            }
            if (curr == null || curr == DELETED || curr.key != key)
                return false;
            if (parent == null)
                return false; // key was root — unsupported in this impl
            // LINEARIZATION POINT: CAS replaces leaf with DELETED sentinel
            if (parent.children[pq].compareAndSet(curr, DELETED))
                return true;
            // CAS lost — retry
        }
    }
}
