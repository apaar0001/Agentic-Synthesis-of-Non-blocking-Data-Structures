package com.example.Sets;

import com.example.utils.SetADT;

/**
 * Reference sequential implementation of a Quad Tree.
 * Keys are stored as 1D integer values and split into 4 quadrants by bit
 * ranges.
 * Used as CodeBLEU reference baseline for semantic scoring.
 */
public class QuadTreeRef implements SetADT {

    private static final int NW = 0, NE = 1, SW = 2, SE = 3;

    private static class Node {
        int key;
        Node[] children; // [NW, NE, SW, SE]
        int lo, hi; // range [lo, hi) this node covers

        Node(int lo, int hi) {
            this.lo = lo;
            this.hi = hi;
            this.key = Integer.MIN_VALUE; // empty sentinel
            this.children = new Node[4];
        }
    }

    private final Node root;
    private static final int LO = Integer.MIN_VALUE;
    private static final int HI = Integer.MAX_VALUE;

    public QuadTreeRef() {
        root = new Node(LO, HI);
    }

    private int quadrant(int key, int lo, int hi) {
        int mid = lo / 2 + hi / 2;
        return key < mid ? SW : SE; // simplified 1D split
    }

    @Override
    public boolean add(int key) {
        Node curr = root;
        while (true) {
            if (curr.key == Integer.MIN_VALUE) {
                curr.key = key;
                return true;
            }
            if (curr.key == key) {
                return false;
            }
            int q = quadrant(key, curr.lo, curr.hi);
            int mid = curr.lo / 2 + curr.hi / 2;
            int childLo = (q == SW || q == NW) ? curr.lo : mid;
            int childHi = (q == SW || q == NW) ? mid : curr.hi;
            if (curr.children[q] == null) {
                curr.children[q] = new Node(childLo, childHi);
            }
            curr = curr.children[q];
        }
    }

    @Override
    public boolean remove(int key) {
        return removeHelper(root, key);
    }

    private boolean removeHelper(Node node, int key) {
        if (node == null)
            return false;
        if (node.key == key) {
            node.key = Integer.MIN_VALUE;
            return true;
        }
        int q = quadrant(key, node.lo, node.hi);
        return removeHelper(node.children[q], key);
    }

    @Override
    public boolean contains(int key) {
        Node curr = root;
        while (curr != null) {
            if (curr.key == key)
                return true;
            if (curr.key == Integer.MIN_VALUE)
                return false;
            curr = curr.children[quadrant(key, curr.lo, curr.hi)];
        }
        return false;
    }
}
