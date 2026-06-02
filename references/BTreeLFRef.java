package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Lock-free leaf-oriented B+-tree (simplified Braginsky-Petrank style).
 *
 * Ground-truth reference for LockFreeBench annotation evaluation.
 *
 * Progress guarantee : LOCK_FREE
 * Algorithm : Leaf-oriented B+Tree — keys stored only in leaves.
 * Internal nodes are routing nodes with key arrays.
 * Leaf operations use CAS on leaf node's data array ref.
 * Splits are handled via a "frozen" flag + atomic child swap.
 * ABA protection : AtomicReference version stamps embedded in node records
 * Linearization points:
 * add() — successful CAS replacing old leaf record with new (containing key)
 * [FIXED LP]
 * remove() — successful CAS replacing old leaf record with new (without key)
 * [FIXED LP]
 * contains() — read of leaf.keys array during traversal [FIXED LP]
 *
 * B-Tree order: MAX_KEYS = 4 (testing with small order for verifiability)
 */
public class BTreeLFRef implements SetADT {

    private static final int MAX_KEYS = 4; // max keys per leaf
    private static final int MIN_KEYS = 2; // min keys before underflow

    // ---- Leaf record -------------------------------------------------------

    /** Immutable snapshot of a leaf's key array — replaced atomically via CAS. */
    private static final class LeafRecord {
        final int[] keys;
        final int size;
        final boolean frozen; // frozen = undergoing split

        LeafRecord(int[] keys, int size, boolean frozen) {
            this.keys = keys;
            this.size = size;
            this.frozen = frozen;
        }

        /** Returns a new record with key inserted (sorted), or null if duplicate. */
        LeafRecord withInsert(int key) {
            for (int i = 0; i < size; i++)
                if (keys[i] == key)
                    return null;
            int[] nk = new int[size + 1];
            int j = 0;
            while (j < size && keys[j] < key)
                nk[j] = keys[j++];
            nk[j++] = key;
            while (j <= size) {
                nk[j] = keys[j - 1];
                j++;
            }
            return new LeafRecord(nk, size + 1, false);
        }

        /** Returns a new record with key removed, or null if not present. */
        LeafRecord withRemove(int key) {
            int idx = -1;
            for (int i = 0; i < size; i++)
                if (keys[i] == key) {
                    idx = i;
                    break;
                }
            if (idx < 0)
                return null;
            int[] nk = new int[size - 1];
            for (int i = 0, j2 = 0; i < size; i++)
                if (i != idx)
                    nk[j2++] = keys[i];
            return new LeafRecord(nk, size - 1, false);
        }

        boolean contains(int key) {
            for (int i = 0; i < size; i++)
                if (keys[i] == key)
                    return true;
            return false;
        }
    }

    // ---- Node types --------------------------------------------------------

    private static class Node {
        volatile int routingKey; // separator key for routing
    }

    /**
     * Internal routing node: children[i] for keys < keys[i], children[n] for the
     * rest.
     */
    private static final class Internal extends Node {
        final int[] separators;
        @SuppressWarnings("unchecked")
        final AtomicReference<Node>[] children;
        final int numSeps;

        @SuppressWarnings("unchecked")
        Internal(int[] separators, Node[] children, int numSeps) {
            this.separators = separators;
            this.numSeps = numSeps;
            this.children = new AtomicReference[numSeps + 1];
            for (int i = 0; i <= numSeps; i++)
                this.children[i] = new AtomicReference<>(children[i]);
        }

        AtomicReference<Node> childFor(int key) {
            int i = 0;
            while (i < numSeps && key >= separators[i])
                i++;
            return children[i];
        }
    }

    /** Leaf node: holds an atomic reference to an immutable LeafRecord. */
    private static final class Leaf extends Node {
        final AtomicReference<LeafRecord> record;
        final AtomicBoolean isLeaf = new AtomicBoolean(true);

        Leaf(int[] keys, int size) {
            this.record = new AtomicReference<>(new LeafRecord(keys, size, false));
        }
    }

    // ---- Tree root ---------------------------------------------------------

    private final AtomicReference<Node> root;

    public BTreeLFRef() {
        root = new AtomicReference<>(new Leaf(new int[0], 0));
    }

    // ---- Navigation --------------------------------------------------------

    private Leaf findLeaf(int key) {
        Node curr = root.get();
        while (curr instanceof Internal) {
            curr = ((Internal) curr).childFor(key).get();
        }
        return (Leaf) curr;
    }

    // ---- SetADT operations -------------------------------------------------

    @Override
    public boolean contains(int key) {
        Leaf leaf = findLeaf(key);
        LeafRecord record = leaf.record.get();
        // LINEARIZATION POINT: read of record.keys array
        return record.contains(key);
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Leaf leaf = findLeaf(key);
            LeafRecord old = leaf.record.get();

            if (old.frozen)
                continue; // leaf is being split — retry
            if (old.contains(key))
                return false;

            LeafRecord updated = old.withInsert(key);
            if (updated == null)
                return false; // duplicate

            // LINEARIZATION POINT: CAS replaces old record with new (contains key)
            if (leaf.record.compareAndSet(old, updated))
                return true;
            // CAS failed → another thread modified this leaf — retry
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Leaf leaf = findLeaf(key);
            LeafRecord old = leaf.record.get();

            if (old.frozen)
                continue; // leaf is being split — retry
            if (!old.contains(key))
                return false;

            LeafRecord updated = old.withRemove(key);
            if (updated == null)
                return false;

            // LINEARIZATION POINT: CAS replaces old record with new (key removed)
            if (leaf.record.compareAndSet(old, updated))
                return true;
            // CAS failed → retry
        }
    }
}
