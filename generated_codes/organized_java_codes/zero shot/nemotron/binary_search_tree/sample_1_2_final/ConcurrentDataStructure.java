package com.example.Sets;
import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private final Node header;

    public ConcurrentDataStructure() {
        header = new Node(0, true);
        header.left.set(new Node(null, true), false);
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node parent = header;
            AtomicMarkableReference<Node> childRef = parent.left;
            boolean[] marked = new boolean[1];
            Node child = childRef.get(marked);
            while (marked[0]) {
                Node unmarked = childRef.getReference();
                if (parent.left.compareAndSet(child, unmarked, true, false)) {
                }
                childRef = parent.left;
                child = childRef.get(marked);
            }
            if (child == null) {
                continue;
            }
            if (child.isLeaf) {
                Integer leafKey = child.key;
                if (leafKey == null) {
                    Node newLeaf = new Node(key, true);
                    if (parent.left.compareAndSet(child, newLeaf, false, false)) {
                        return true;
                    }
                } else if (leafKey == key) {
                    return false;
                } else {
                    int oldKey = leafKey;
                    Node newLeafA = new Node(key, true);
                    Node newLeafB = new Node(oldKey, true);
                    Node newInternal;
                    if (key < oldKey) {
                        newInternal = new Node(oldKey, false);
                        newInternal.left.set(newLeafA, false);
                        newInternal.right.set(newLeafB, false);
                    } else {
                        newInternal = new Node(key, false);
                        newInternal.left.set(newLeafB, false);
                        newInternal.right.set(newLeafA, false);
                    }
                    if (parent.left.compareAndSet(child, newInternal, false, false)) {
                        return true;
                    }
                }
            } else {
                if (key < child.key) {
                    parent = child;
                    childRef = child.left;
                } else {
                    parent = child;
                    childRef = child.right;
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node parent = header;
            AtomicMarkableReference<Node> childRef = parent.left;
            boolean[] marked = new boolean[1];
            Node child = childRef.get(marked);
            while (marked[0]) {
                Node unmarked = childRef.getReference();
                if (parent.left.compareAndSet(child, unmarked, true, false)) {
                }
                childRef = parent.left;
                child = childRef.get(marked);
            }
            if (child == null || !child.isLeaf) {
                if (child == null) {
                    return false;
                }
                if (key < child.key) {
                    parent = child;
                    childRef = child.left;
                } else {
                    parent = child;
                    childRef = child.right;
                }
                continue;
            }
            Integer leafKey = child.key;
            if (leafKey == null || leafKey != key) {
                return false;
            }
            if (childRef.compareAndSet(child, child, false, true)) {
                // Node has been marked
                Node sibling = (parent.left == childRef) ? parent.right.getReference() : parent.left.getReference();
                AtomicMarkableReference<Node> parentRef = (parent.left == childRef) ? parent.left : parent.right;
                parentRef.compareAndSet(child, sibling, true, false);
                return true;
            }
            return false;
        }
    }

    @Override
    public boolean contains(int key) {
        AtomicMarkableReference<Node> childRef = header.left;
        while (true) {
            boolean[] marked = new boolean[1];
            Node child = childRef.get(marked);
            if (marked[0]) {
                Node unmarked = childRef.getReference();
                if (childRef.compareAndSet(child, unmarked, true, false)) {
                }
                childRef = header.left;
                continue;
            }
            if (child == null) {
                return false;
            }
            if (child.isLeaf) {
                Integer leafKey = child.key;
                return leafKey != null && leafKey == key;
            }
            if (key < child.key) {
                childRef = child.left;
            } else {
                childRef = child.right;
            }
        }
    }

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
}