package com.example.Sets;
import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements com.example.utils.SetADT {
    private static final int NULL_KEY = Integer.MIN_VALUE;

    private static class Node {
        final AtomicInteger key;
        final boolean isLeaf;
        final AtomicMarkableReference<Node> left;
        final AtomicMarkableReference<Node> right;

        Node(int key, boolean isLeaf) {
            this.key = new AtomicInteger(key);
            this.isLeaf = isLeaf;
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
        }
    }

    private final AtomicMarkableReference<Node> root;

    public ConcurrentDataStructure() {
        Node emptyLeaf = new Node(NULL_KEY, true);
        this.root = new AtomicMarkableReference<>(emptyLeaf, false);
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node[] res = find(key);
            Node parent = res[0];
            Node leaf = res[1];
            boolean isLeft = (boolean) res[2];

            if (leaf == null) {
                continue;
            }

            if (parent != null) {
                boolean[] marked = {false};
                Node ref = isLeft ? parent.left.get(marked) : parent.right.get(marked);
                if (marked[0] || ref != leaf) {
                    continue;
                }
            }

            int leafKey = leaf.key.get();
            if (leafKey == NULL_KEY) {
                if (leaf.key.compareAndSet(NULL_KEY, key)) {
                    return true;
                }
            } else if (leafKey == key) {
                return false;
            } else {
                int oldKey = leafKey;
                Node newLeaf = new Node(key, true);
                Node oldLeaf = new Node(oldKey, true);
                Node newInternal = new Node(0, false);
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
                    if (root.compareAndSet(leaf, newInternal, false, false)) {
                        return true;
                    }
                } else {
                    if (isLeft) {
                        if (parent.left.compareAndSet(leaf, newInternal, false, false)) {
                            return true;
                        }
                    } else {
                        if (parent.right.compareAndSet(leaf, newInternal, false, false)) {
                            return true;
                        }
                    }
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node[] res = find(key);
            Node parent = res[0];
            Node node = res[1];
            boolean isLeft = (boolean) res[2];

            if (node == null) {
                return false;
            }

            if (parent != null) {
                boolean[] marked = {false};
                Node ref = isLeft ? parent.left.get(marked) : parent.right.get(marked);
                if (marked[0] || ref != node) {
                    continue;
                }
            }

            int nodeKey = node.key.get();
            if (nodeKey == NULL_KEY || nodeKey != key) {
                return false;
            }

            if (parent == null) {
                boolean[] marked = {false};
                Node currentRef = root.get(marked);
                if (marked[0] || currentRef != node) {
                    continue;
                }
                if (root.attemptMark(node, true)) {
                    // Node has been marked
                    return true;
                }
            } else {
                boolean[] marked = {false};
                Node ref = isLeft ? parent.left.get(marked) : parent.right.get(marked);
                if (marked[0] || ref != node) {
                    continue;
                }
                if (isLeft ? parent.left.attemptMark(node, true) : parent.right.attemptMark(node, true)) {
                    // Node has been marked
                    return true;
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        while (true) {
            Node[] res = find(key);
            Node parent = res[0];
            Node node = res[1];
            boolean isLeft = (boolean) res[2];

            if (node == null) {
                return false;
            }

            if (parent != null) {
                boolean[] marked = {false};
                Node ref = isLeft ? parent.left.get(marked) : parent.right.get(marked);
                if (marked[0] || ref != node) {
                    continue;
                }
            }

            int nodeKey = node.key.get();
            if (nodeKey == NULL_KEY) {
                return false;
            }
            if (nodeKey == key) {
                return true;
            }
            return false;
        }
    }

    private Node[] find(int key) {
        while (true) {
            Node prev = null;
            Node curr = root.getReference();
            boolean isLeft = false;

            while (true) {
                if (prev != null) {
                    boolean[] marked = {false};
                    Node ref = isLeft ? prev.left.get(marked) : prev.right.get(marked);
                    if (marked[0]) {
                        helpRemoveIfMarked(prev, curr, isLeft);
                        prev = null;
                        curr = root.getReference();
                        continue;
                    }
                }

                if (curr.isLeaf) {
                    return new Node[]{prev, curr, isLeft};
                }

                boolean[] leftMarked = {false};
                Node leftChild = curr.left.get(leftMarked);
                boolean[] rightMarked = {false};
                Node rightChild = curr.right.get(rightMarked);

                if (leftMarked[0]) {
                    helpRemoveIfMarked(curr, leftChild, true);
                    prev = null;
                    curr = root.getReference();
                    break;
                }
                if (rightMarked[0]) {
                    helpRemoveIfMarked(curr, rightChild, false);
                    prev = null;
                    curr = root.getReference();
                    break;
                }

                if (key < curr.key.get()) {
                    prev = curr;
                    curr = leftChild;
                    isLeft = true;
                } else {
                    prev = curr;
                    curr = rightChild;
                    isLeft = false;
                }
            }
        }
    }

    private void helpRemoveIfMarked(Node parent, Node child, boolean isLeftChild) {
        boolean[] marked = {false};
        Node ref = isLeftChild ? parent.left.get(marked) : parent.right.get(marked);
        if (!marked[0] || ref != child) {
            return;
        }
        Node sibling = isLeftChild ? parent.right.getReference() : parent.left.getReference();
        boolean[] sibMarked = {false};
        Node sibRef = isLeftChild ? parent.right.get(sibMarked) : parent.left.get(sibMarked);
        if (sibMarked[0]) {
            return;
        }
        boolean success = isLeftChild ?
                parent.left.compareAndSet(child, sibling, true, false) :
                parent.right.compareAndSet(child, sibling, true, false);
    }
}