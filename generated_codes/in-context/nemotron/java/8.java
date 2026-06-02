package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        Integer key;
        boolean isLeaf;
        final AtomicMarkableReference<Node> left;
        final AtomicMarkableReference<Node> right;

        Node(Integer key, boolean isLeaf) {
            this.key = key;
            this.isLeaf = isLeaf;
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
        }
    }

    private static class SearchResult {
        Node parent;
        Node node;
        boolean isLeftChild;

        SearchResult(Node parent, Node node, boolean isLeftChild) {
            this.parent = parent;
            this.node = node;
            this.isLeftChild = isLeftChild;
        }
    }

    private final AtomicReference<Node> root;

    public ConcurrentDataStructure() {
        this.root = new AtomicReference<>(new Node(null, true));
    }

    private void helpUnmark(Node parent, boolean isLeft) {
        AtomicMarkableReference<Node> ref = isLeft ? parent.left : parent.right;
        Node[] markedHolder = new Node[1];
        boolean[] marked = new boolean[1];
        Node child = ref.get(marked);
        if (marked[0] && child != null) {
            Node sibling = isLeft ? parent.right.getReference() : parent.left.getReference();
            ref.compareAndSet(child, sibling, true, false);
        }
    }

    private SearchResult find(int key) {
        while (true) {
            Node pred = null;
            Node curr = root.get();
            boolean isLeft = false;
            while (!curr.isLeaf) {
                helpUnmark(pred, isLeft);
                Integer currKey = curr.key;
                int cmpKey = (currKey == null) ? Integer.MIN_VALUE : currKey;
                if (key < cmpKey) {
                    pred = curr;
                    isLeft = true;
                    curr = curr.left.getReference();
                } else {
                    pred = curr;
                    isLeft = false;
                    curr = curr.right.getReference();
                }
            }
            return new SearchResult(pred, curr, isLeft);
        }
    }

    @Override
    public boolean add(int key) {
        while (true) {
            SearchResult res = find(key);
            Node parent = res.parent;
            Node leaf = res.node;
            boolean isLeft = res.isLeftChild;

            if (leaf.key == null) {
                Node newLeaf = new Node(key, true);
                if (parent == null) {
                    if (root.compareAndSet(leaf, newLeaf)) {
                        return true;
                    }
                } else {
                    AtomicMarkableReference<Node> ref = isLeft ? parent.left : parent.right;
                    if (ref.compareAndSet(leaf, newLeaf, false, false)) {
                        return true;
                    }
                }
                continue;
            }

            if (leaf.key.equals(key)) {
                return false;
            }

            int oldKey = leaf.key;
            Node newLeaf = new Node(key, true);
            Node oldLeaf = new Node(oldKey, true);
            Node newInternal = new Node((key < oldKey) ? oldKey : key, false);

            if (key < oldKey) {
                newInternal.left.set(oldLeaf, false);
                newInternal.right.set(newLeaf, false);
                newInternal.key = oldKey;
            } else {
                newInternal.left.set(newLeaf, false);
                newInternal.right.set(oldLeaf, false);
                newInternal.key = key;
            }

            if (parent == null) {
                if (root.compareAndSet(leaf, newInternal)) {
                    return true;
                }
            } else {
                AtomicMarkableReference<Node> ref = isLeft ? parent.left : parent.right;
                if (ref.compareAndSet(leaf, newInternal, false, false)) {
                    return true;
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            SearchResult res = find(key);
            Node parent = res.parent;
            Node leaf = res.node;
            boolean isLeft = res.isLeftChild;

            if (leaf.key == null || !leaf.key.equals(key)) {
                return false;
            }

            if (parent == null) {
                Node emptyLeaf = new Node(null, true);
                if (root.compareAndSet(leaf, emptyLeaf)) {
                    // Node has been marked
                    return true;
                }
                continue;
            }

            Node sibling = isLeft ? parent.right.getReference() : parent.left.getReference();
            AtomicMarkableReference<Node> ref = isLeft ? parent.left : parent.right;
            boolean[] marked = {false};
            Node current = ref.get(marked);
            if (marked[0]) {
                return false;
            }
            if (ref.compareAndSet(current, current, false, true)) {
                // Node has been marked
                ref.compareAndSet(current, sibling, true, false);
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node curr = root.get();
        while (true) {
            if (curr.isLeaf) {
                Integer k = curr.key;
                return k != null && k.equals(key);
            }
            helpUnmark(curr, true);
            helpUnmark(curr, false);
            Integer currKey = curr.key;
            int cmpKey = (currKey == null) ? Integer.MIN_VALUE : currKey;
            if (key < cmpKey) {
                curr = curr.left.getReference();
            } else {
                curr = curr.right.getReference();
            }
        }
    }
}