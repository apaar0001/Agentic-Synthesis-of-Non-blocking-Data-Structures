package com.example.Sets;

import com.example.utils.SetADT;

/**
 * Reference sequential implementation of a Binary Search Tree.
 * Used as CodeBLEU reference baseline for semantic scoring.
 */
public class BSTRef implements SetADT {

    private static class Node {
        int key;
        Node left;
        Node right;

        Node(int key) {
            this.key = key;
            this.left = null;
            this.right = null;
        }
    }

    private Node root;

    public BSTRef() {
        this.root = null;
    }

    @Override
    public boolean add(int key) {
        if (root == null) {
            root = new Node(key);
            return true;
        }
        Node curr = root;
        while (true) {
            if (key < curr.key) {
                if (curr.left == null) {
                    curr.left = new Node(key);
                    return true;
                }
                curr = curr.left;
            } else if (key > curr.key) {
                if (curr.right == null) {
                    curr.right = new Node(key);
                    return true;
                }
                curr = curr.right;
            } else {
                return false; // duplicate
            }
        }
    }

    @Override
    public boolean remove(int key) {
        Node parent = null;
        Node curr = root;
        while (curr != null && curr.key != key) {
            parent = curr;
            curr = key < curr.key ? curr.left : curr.right;
        }
        if (curr == null) {
            return false;
        }
        // Two children: replace with in-order successor
        if (curr.left != null && curr.right != null) {
            Node succParent = curr;
            Node succ = curr.right;
            while (succ.left != null) {
                succParent = succ;
                succ = succ.left;
            }
            curr.key = succ.key;
            curr = succ;
            parent = succParent;
        }
        // 0 or 1 child
        Node child = (curr.left != null) ? curr.left : curr.right;
        if (parent == null) {
            root = child;
        } else if (parent.left == curr) {
            parent.left = child;
        } else {
            parent.right = child;
        }
        return true;
    }

    @Override
    public boolean contains(int key) {
        Node curr = root;
        while (curr != null) {
            if (key < curr.key)
                curr = curr.left;
            else if (key > curr.key)
                curr = curr.right;
            else
                return true;
        }
        return false;
    }
}
