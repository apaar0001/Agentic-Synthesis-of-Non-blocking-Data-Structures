package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        final AtomicReference<Integer> key;
        final AtomicMarkableReference<Node> left;
        final AtomicMarkableReference<Node> right;
        volatile boolean isLeaf;

        Node(Integer key, boolean isLeaf) {
            this.key = new AtomicReference<>(key);
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
            this.isLeaf = isLeaf;
        }

        Node() {
            this.key = new AtomicReference<>(null);
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
            this.isLeaf = true;
        }
    }

    private static final Node EMPTY_LEAF = new Node();

    private final AtomicReference<Node> root;

    public ConcurrentDataStructure() {
        this.root = new AtomicReference<>(EMPTY_LEAF);
    }

    private boolean tryMarkForDeletion(Node node) {
        return node.key.compareAndSet(node.key.get(), null);
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node[] path = findOrInsertPath(key, false);
            Node parent = path[0];
            Node child = path[1];
            Boolean goLeft = (Boolean) path[2];

            if (child.key.get() != null) {
                if (child.key.get() == key) {
                    return false;
                }
                continue;
            }

            Node newLeaf = new Node(key, true);
            if (goLeft) {
                if (parent.left.compareAndSet(child, newLeaf, false, false)) {
                    return true;
                }
            } else {
                if (parent.right.compareAndSet(child, newLeaf, false, false)) {
                    return true;
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node[] path = findPath(key);
            Node parent = path[0];
            Node node = path[1];
            Boolean goLeft = (Boolean) path[2];

            if (node.key.get() == null || !node.key.get().equals(key)) {
                return false;
            }

            if (!tryMarkForDeletion(node)) {
                continue;
            }
            // Node has been marked

            Node sibling = goLeft ? parent.right.getReference() : parent.left.getReference();
            if (goLeft) {
                parent.left.compareAndSet(node, sibling, false, false);
            } else {
                parent.right.compareAndSet(node, sibling, false, false);
            }
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        Node curr = root.getReference();
        while (!curr.isLeaf) {
            Integer k = curr.key.get();
            if (k == null) {
                return false;
            }
            if (key < k) {
                curr = curr.left.getReference();
            } else {
                curr = curr.right.getReference();
            }
        }
        Integer k = curr.key.get();
        return k != null && k.equals(key);
    }

    private Node[] findPath(int key) {
        Node parent = root.getReference();
        Node curr = parent;
        Boolean goLeft = null;
        while (true) {
            if (curr.isLeaf) {
                return new Node[]{parent, curr, goLeft};
            }
            Integer k = curr.key.get();
            if (k == null) {
                return new Node[]{parent, curr, goLeft};
            }
            boolean markedLeft = curr.left.isMarked();
            boolean markedRight = curr.right.isMarked();
            Node leftRef = curr.left.getReference();
            Node rightRef = curr.right.getReference();
            while (markedLeft) {
                Node next = leftRef;
                if (curr.left.compareAndSet(next, next, false, false)) {
                } else {
                    break;
                }
                leftRef = curr.left.getReference();
                markedLeft = curr.left.isMarked();
            }
            while (markedRight) {
                Node next = rightRef;
                if (curr.right.compareAndSet(next, next, false, false)) {
                } else {
                    break;
                }
                rightRef = curr.right.getReference();
                markedRight = curr.right.isMarked();
            }
            if (key < k) {
                parent = curr;
                curr = leftRef;
                goLeft = true;
            } else if (key > k) {
                parent = curr;
                curr = rightRef;
                goLeft = false;
            } else {
                return new Node[]{parent, curr, goLeft};
            }
        }
    }

    private Node[] findOrInsertPath(int key, boolean forInsert) {
        Node parent = root.getReference();
        Node curr = parent;
        Boolean goLeft = null;
        while (true) {
            if (curr.isLeaf) {
                Integer k = curr.key.get();
                if (k == null) {
                    return new Node[]{parent, curr, goLeft};
                }
                if (k.equals(key)) {
                    return new Node[]{parent, curr, goLeft};
                }
                if (key < k) {
                    parent = curr;
                    curr = curr.left.getReference();
                    goLeft = true;
                } else {
                    parent = curr;
                    curr = curr.right.getReference();
                    goLeft = false;
                }
                continue;
            }
            Integer k = curr.key.get();
            if (k == null) {
                return new Node[]{parent, curr, goLeft};
            }
            boolean markedLeft = curr.left.isMarked();
            boolean markedRight = curr.right.isMarked();
            Node leftRef = curr.left.getReference();
            Node rightRef = curr.right.getReference();
            while (markedLeft) {
                Node next = leftRef;
                if (curr.left.compareAndSet(next, next, false, false)) {
                } else {
                    break;
                }
                leftRef = curr.left.getReference();
                markedLeft = curr.left.isMarked();
            }
            while (markedRight) {
                Node next = rightRef;
                if (curr.right.compareAndSet(next, next, false, false)) {
                } else {
                    break;
                }
                rightRef = curr.right.getReference();
                markedRight = curr.right.isMarked();
            }
            if (key < k) {
                parent = curr;
                curr = leftRef;
                goLeft = true;
            } else if (key > k) {
                parent = curr;
                curr = rightRef;
                goLeft = false;
            } else {
                return new Node[]{parent, curr, goLeft};
            }
        }
    }
}