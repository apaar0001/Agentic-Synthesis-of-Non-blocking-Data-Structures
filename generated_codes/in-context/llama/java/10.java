package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        final AtomicReference<Integer> key;
        final AtomicMarkableReference<Node> left;
        final AtomicMarkableReference<Node> right;
        final boolean isLeaf;

        Node() {
            this.key = new AtomicReference<>(null);
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
            this.isLeaf = true;
        }

        Node(Integer k, boolean leaf) {
            this.key = new AtomicReference<>(k);
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
            this.isLeaf = leaf;
        }
    }

    private final AtomicReference<Node> root = new AtomicReference<>(new Node());

    private void helpIfMarked(AtomicMarkableReference<Node> ref) {
        Node marked = ref.getReference();
        if (marked == null || !ref.isMarked()) {
            return;
        }
        Node child = (marked.left.getReference() != null) ? marked.left.getReference() : marked.right.getReference();
        ref.attemptMark(marked, false);
        ref.compareAndSet(marked, child, false, false);
    }

    private Node getChild(AtomicMarkableReference<Node> ref) {
        while (true) {
            helpIfMarked(ref);
            Node next = ref.getReference();
            if (!ref.isMarked()) {
                return next;
            }
        }
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node cur = root.get();
            Node parent = null;
            while (!cur.isLeaf) {
                Node left = getChild(cur.left);
                Node right = getChild(cur.right);
                if (left == null && right == null) {
                    break;
                }
                parent = cur;
                if (key < cur.key.get()) {
                    cur = left;
                } else {
                    cur = right;
                }
            }
            Integer curKey = cur.key.get();
            if (curKey == null) {
                if (cur.key.compareAndSet(null, key)) {
                    return true;
                }
                continue;
            }
            if (curKey == key) {
                return false;
            }
            int oldKey = curKey;
            Node newLeaf = new Node(key, true);
            Node oldLeaf = new Node(oldKey, true);
            Node newInternal = new Node();
            newInternal.isLeaf = false;
            if (key < oldKey) {
                newInternal.key.set(oldKey);
                newInternal.left.set(newLeaf, false);
                newInternal.right.set(oldLeaf, false);
            } else {
                newInternal.key.set(key);
                newInternal.left.set(oldLeaf, false);
                newInternal.right.set(newLeaf, false);
            }
            if (parent == null) {
                if (root.compareAndSet(cur, newInternal)) {
                    return true;
                }
            } else {
                boolean isLeft = (parent.left.getReference() == cur);
                AtomicMarkableReference<Node> childRef = isLeft ? parent.left : parent.right;
                if (childRef.compareAndSet(cur, newInternal, false, false)) {
                    return true;
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node cur = root.get();
            Node parent = null;
            Node grandParent = null;
            while (!cur.isLeaf) {
                Node left = getChild(cur.left);
                Node right = getChild(cur.right);
                if (left == null && right == null) {
                    break;
                }
                grandParent = parent;
                parent = cur;
                if (key < cur.key.get()) {
                    cur = left;
                } else {
                    cur = right;
                }
            }
            Integer curKey = cur.key.get();
            if (curKey == null || curKey != key) {
                return false;
            }
            if (parent == null) {
                if (cur.key.compareAndSet(key, null)) {
                    return true;
                }
                continue;
            }
            boolean isLeft = (parent.left.getReference() == cur);
            AtomicMarkableReference<Node> childRef = isLeft ? parent.left : parent.right;
            boolean marked = childRef.attemptMark(cur, true);
            if (marked) {
                // Node has been marked
                Node sibling = isLeft ? parent.right.getReference() : parent.left.getReference();
                if (grandParent == null) {
                    root.compareAndSet(parent, sibling);
                    return true;
                }
                boolean parentIsLeft = (grandParent.left.getReference() == parent);
                AtomicMarkableReference<Node> grandChildRef = parentIsLeft ? grandParent.left : grandParent.right;
                grandChildRef.compareAndSet(parent, sibling, false, false);
                return true;
            }
            if (childRef.isMarked()) {
                return false;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node cur = root.get();
        while (!cur.isLeaf) {
            Node left = getChild(cur.left);
            Node right = getChild(cur.right);
            if (left == null && right == null) {
                break;
            }
            if (key < cur.key.get()) {
                cur = left;
            } else {
                cur = right;
            }
        }
        return cur.key.get() != null && cur.key.get() == key;
    }
}