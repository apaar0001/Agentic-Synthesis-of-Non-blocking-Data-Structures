package com.example.Sets;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicBoolean;
import com.example.utils.SetADT;

public class ConcurrentDataStructure implements SetADT {
    private final AtomicReference<Node> root;

    public ConcurrentDataStructure() {
        root.set(new Node(null, true));
    }

    @Override
    public boolean add(int key) {
        outer: while (true) {
            Node curr = root.get();
            Node parent = null;
            AtomicMarkableReference<Node> parentChildRef = null;
            boolean isLeft = false;
            while (!curr.isLeaf) {
                if (curr.marked.get()) {
                    helpRemove(parent, parentChildRef, curr);
                    continue outer;
                }
                parent = curr;
                if (key < curr.key) {
                    parentChildRef = curr.left;
                    curr = curr.left.getReference();
                    isLeft = true;
                } else {
                    parentChildRef = curr.right;
                    curr = curr.right.getReference();
                    isLeft = false;
                }
                if (curr == null) {
                    break;
                }
            }
            if (curr == null) {
                Node newLeaf = new Node(Integer.valueOf(key), true);
                if (parent == null) {
                    if (root.compareAndSet(null, newLeaf)) {
                        return true;
                    }
                } else {
                    if (parentChildRef.compareAndSet(null, newLeaf, false, false)) {
                        return true;
                    }
                }
                continue outer;
            }
            if (curr.marked.get()) {
                helpRemove(parent, parentChildRef, curr);
                continue outer;
            }
            Integer leafKey = curr.key;
            if (leafKey == null) {
                Node newLeaf = new Node(Integer.valueOf(key), true);
                if (parent == null) {
                    if (root.compareAndSet(curr, newLeaf)) {
                        return true;
                    }
                } else {
                    if (parentChildRef.compareAndSet(curr, newLeaf, false, false)) {
                        return true;
                    }
                }
                continue outer;
            }
            if (leafKey == key) {
                return false;
            }
            Integer oldKey = leafKey;
            Node newLeaf = new Node(Integer.valueOf(key), true);
            Node oldLeaf = new Node(oldKey, true);
            Node newInternal = new Node(null, false);
            if (key < oldKey) {
                newInternal.key = oldKey;
                newInternal.left.set(newLeaf, false);
                newInternal.right.set(oldLeaf, false);
            } else {
                newInternal.key = key;
                newInternal.left.set(oldLeaf, false);
                newInternal.right.set(newLeaf, false);
            }
            if (parent == null) {
                if (root.compareAndSet(curr, newInternal)) {
                    return true;
                }
            } else {
                if (parentChildRef.compareAndSet(curr, newInternal, false, false)) {
                    return true;
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        outer: while (true) {
            Node curr = root.get();
            Node parent = null;
            Node grandParent = null;
            AtomicMarkableReference<Node> parentChildRef = null;
            boolean isLeft = false;
            while (!curr.isLeaf) {
                if (curr.marked.get()) {
                    helpRemove(parent, parentChildRef, curr);
                    continue outer;
                }
                grandParent = parent;
                parent = curr;
                if (key < curr.key) {
                    parentChildRef = curr.left;
                    curr = curr.left.getReference();
                    isLeft = true;
                } else {
                    parentChildRef = curr.right;
                    curr = curr.right.getReference();
                    isLeft = false;
                }
                if (curr == null) {
                    return false;
                }
            }
            if (curr.marked.get()) {
                helpRemove(parent, parentChildRef, curr);
                continue outer;
            }
            Integer leafKey = curr.key;
            if (leafKey == null || !leafKey.equals(key)) {
                return false;
            }
            if (!curr.marked.compareAndSet(false, true)) {
                return false;
            }
            // Node has been marked
            if (parent == null) {
                Node newLeaf = new Node(null, true);
                if (root.compareAndSet(curr, newLeaf)) {
                    return true;
                }
                continue outer;
            }
            Node sibling = isLeft ? parent.right.getReference() : parent.left.getReference();
            if (parentChildRef.compareAndSet(curr, sibling, true, false)) {
                return true;
            }
            continue outer;
        }
    }

    @Override
    public boolean contains(int key) {
        Node curr = root.get();
        while (true) {
            if (curr == null) {
                return false;
            }
            if (curr.marked.get()) {
                curr = root.get();
                continue;
            }
            if (curr.isLeaf) {
                Integer k = curr.key;
                return k != null && k == key;
            }
            if (key < curr.key) {
                curr = curr.left.getReference();
            } else {
                curr = curr.right.getReference();
            }
        }
    }

    private void helpRemove(Node parent, AtomicMarkableReference<Node> childRef, Node markedNode) {
        if (parent == null) {
            Node newLeaf = new Node(null, true);
            root.compareAndSet(markedNode, newLeaf);
            return;
        }
        boolean isLeft = (parent.left.getReference() == markedNode);
        Node sibling = isLeft ? parent.right.getReference() : parent.left.getReference();
        AtomicMarkableReference<Node> ref = isLeft ? parent.left : parent.right;
        ref.compareAndSet(markedNode, sibling, true, false);
    }

    private static class Node {
        Integer key;
        final AtomicMarkableReference<Node> left;
        final AtomicMarkableReference<Node> right;
        final boolean isLeaf;
        final AtomicBoolean marked;
        Node(Integer key, boolean isLeaf) {
            this.key = key;
            this.isLeaf = isLeaf;
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
            this.marked = new AtomicBoolean(false);
        }
    }
}