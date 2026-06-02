package com.example.Sets;
import com.example.utils.SetADT;

public class ConcurrentDataStructure implements SetADT {
    private Node root;

    public ConcurrentDataStructure() {
        root = new Node();
        root.isLeaf = true;
        root.key = null;
    }

    @Override
    public boolean add(int key) {
        Node parent = null;
        Node current = root;

        while (!current.isLeaf) {
            parent = current;

            if (key < current.key) {
                current = current.left;
            } else {
                current = current.right;
            }
        }

        if (current.key == null) {
            current.key = key;
            return true;
        }

        if (current.key == key) {
            return false;
        }

        int oldKey = current.key;

        Node newLeaf = new Node();
        newLeaf.isLeaf = true;
        newLeaf.key = key;

        Node oldLeaf = new Node();
        oldLeaf.isLeaf = true;
        oldLeaf.key = oldKey;

        Node newInternal = new Node();
        newInternal.isLeaf = false;

        if (key < oldKey) {
            newInternal.key = oldKey;
            newInternal.left = newLeaf;
            newInternal.right = oldLeaf;
        } else {
            newInternal.key = key;
            newInternal.left = oldLeaf;
            newInternal.right = newLeaf;
        }

        if (parent == null) {
            root = newInternal;
        } else {
            if (parent.left == current) {
                parent.left = newInternal;
            } else {
                parent.right = newInternal;
            }
        }

        return true;
    }

    @Override
    public boolean remove(int key) {
        Node parent = null;
        Node grandParent = null;
        Node current = root;

        while (!current.isLeaf) {
            grandParent = parent;
            parent = current;

            if (key < current.key) {
                current = current.left;
            } else {
                current = current.right;
            }
        }

        if (current.key == null || current.key != key) {
            return false;
        }

        if (parent == null) {
            current.key = null;
            return true;
        }

        Node sibling;
        if (parent.left == current) {
            sibling = parent.right;
        } else {
            sibling = parent.left;
        }

        if (grandParent == null) {
            root = sibling;
            return true;
        }

        if (grandParent.left == parent) {
            grandParent.left = sibling;
        } else {
            grandParent.right = sibling;
        }

        return true;
    }

    @Override
    public boolean contains(int key) {
        Node current = root;

        while (!current.isLeaf) {
            if (key < current.key) {
                current = current.left;
            } else {
                current = current.right;
            }
        }

        return current.key != null && current.key == key;
    }

    private static class Node {
        int key;
        Node left;
        Node right;
        boolean isLeaf;
    }
}