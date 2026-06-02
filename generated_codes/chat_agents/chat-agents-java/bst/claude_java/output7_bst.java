package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        final int key;
        final AtomicMarkableReference<Node> left;
        final AtomicMarkableReference<Node> right;

        Node(int key) {
            this.key = key;
            this.left = new AtomicMarkableReference<>(null, false);
            this.right = new AtomicMarkableReference<>(null, false);
        }
    }

    private static class Window {
        final Node pred;
        final Node curr;
        final boolean isLeft;

        Window(Node pred, Node curr, boolean isLeft) {
            this.pred = pred;
            this.curr = curr;
            this.isLeft = isLeft;
        }
    }

    private final AtomicMarkableReference<Node> root;

    public ConcurrentDataStructure() {
        this.root = new AtomicMarkableReference<>(null, false);
    }

    @Override
    public boolean add(int key) {
        while (true) {
            if (root.getReference() == null) {
                Node newNode = new Node(key);
                if (root.compareAndSet(null, newNode, false, false)) {
                    return true;
                }
                continue;
            }
            Result result = find(key);
            if (result.found) {
                return false;
            }
            Node newNode = new Node(key);
            Node pred = result.pred;
            Node curr = result.curr;
            if (pred == null) {
                if (root.compareAndSet(curr, newNode, false, false)) {
                    return true;
                }
            } else if (result.isLeft) {
                if (pred.left.compareAndSet(curr, newNode, false, false)) {
                    return true;
                }
            } else {
                if (pred.right.compareAndSet(curr, newNode, false, false)) {
                    return true;
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Result result = find(key);
            if (!result.found) {
                return false;
            }
            Node target = result.curr;
            Node pred = result.pred;

            boolean[] leftChildMarked = {false};
            boolean[] rightChildMarked = {false};
            Node leftChild = target.left.get(leftChildMarked);
            Node rightChild = target.right.get(rightChildMarked);

            boolean hasLeft = leftChild != null && !leftChildMarked[0];
            boolean hasRight = rightChild != null && !rightChildMarked[0];

            if (!hasLeft && !hasRight) {
                if (target.left.compareAndSet(leftChild, leftChild, false, true)) {
                    // Node has been marked
                    if (pred == null) {
                        root.compareAndSet(target, null, false, false);
                    } else if (result.isLeft) {
                        pred.left.compareAndSet(target, null, false, false);
                    } else {
                        pred.right.compareAndSet(target, null, false, false);
                    }
                    return true;
                }
            } else if (hasLeft && !hasRight) {
                if (target.left.compareAndSet(leftChild, leftChild, false, true)) {
                    // Node has been marked
                    if (pred == null) {
                        root.compareAndSet(target, leftChild, false, false);
                    } else if (result.isLeft) {
                        pred.left.compareAndSet(target, leftChild, false, false);
                    } else {
                        pred.right.compareAndSet(target, leftChild, false, false);
                    }
                    return true;
                }
            } else if (!hasLeft) {
                if (target.right.compareAndSet(rightChild, rightChild, false, true)) {
                    // Node has been marked
                    if (pred == null) {
                        root.compareAndSet(target, rightChild, false, false);
                    } else if (result.isLeft) {
                        pred.left.compareAndSet(target, rightChild, false, false);
                    } else {
                        pred.right.compareAndSet(target, rightChild, false, false);
                    }
                    return true;
                }
            } else {
                SuccessorResult sr = findSuccessor(target);
                if (sr == null) continue;
                Node succParent = sr.parent;
                Node succ = sr.node;
                if (succ == null) continue;
                if (succ.left.compareAndSet(
                        succ.left.getReference(), succ.left.getReference(), false, true)) {
                    // Node has been marked
                    Node succRight = succ.right.getReference();
                    if (succParent == target) {
                        succParent.right.compareAndSet(succ, succRight, false, false);
                    } else {
                        succParent.left.compareAndSet(succ, succRight, false, false);
                    }
                    Node newNode = new Node(succ.key);
                    boolean[] lm = {false};
                    boolean[] rm = {false};
                    Node lc = target.left.get(lm);
                    Node rc = target.right.get(rm);
                    if (!lm[0]) newNode.left.set(lc, false);
                    if (!rm[0]) newNode.right.set(rc, false);
                    if (pred == null) {
                        root.compareAndSet(target, newNode, false, false);
                    } else if (result.isLeft) {
                        pred.left.compareAndSet(target, newNode, false, false);
                    } else {
                        pred.right.compareAndSet(target, newNode, false, false);
                    }
                    return true;
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        boolean[] marked = {false};
        Node curr = root.get(marked);
        while (curr != null) {
            if (marked[0]) {
                curr = root.getReference();
                marked[0] = false;
                continue;
            }
            if (key == curr.key) {
                boolean[] m = {false};
                curr.left.get(m);
                return !m[0];
            } else if (key < curr.key) {
                curr = curr.left.get(marked);
            } else {
                curr = curr.right.get(marked);
            }
        }
        return false;
    }

    private static class Result {
        final Node pred;
        final Node curr;
        final boolean found;
        final boolean isLeft;

        Result(Node pred, Node curr, boolean found, boolean isLeft) {
            this.pred = pred;
            this.curr = curr;
            this.found = found;
            this.isLeft = isLeft;
        }
    }

    private static class SuccessorResult {
        final Node parent;
        final Node node;

        SuccessorResult(Node parent, Node node) {
            this.parent = parent;
            this.node = node;
        }
    }

    private Result find(int key) {
        boolean[] marked = {false};
        Node pred = null;
        boolean isLeft = false;
        Node curr = root.get(marked);

        while (curr != null) {
            if (marked[0]) {
                curr = root.getReference();
                marked[0] = false;
                pred = null;
                continue;
            }
            if (key == curr.key) {
                boolean[] lm = {false};
                curr.left.get(lm);
                if (lm[0]) {
                    if (pred == null) {
                        root.compareAndSet(curr, null, false, false);
                        return find(key);
                    }
                    if (isLeft) {
                        pred.left.compareAndSet(curr, null, false, false);
                    } else {
                        pred.right.compareAndSet(curr, null, false, false);
                    }
                    return find(key);
                }
                return new Result(pred, curr, true, isLeft);
            } else if (key < curr.key) {
                Node next = curr.left.get(marked);
                if (!marked[0]) {
                    pred = curr;
                    isLeft = true;
                    curr = next;
                } else {
                    return find(key);
                }
            } else {
                Node next = curr.right.get(marked);
                if (!marked[0]) {
                    pred = curr;
                    isLeft = false;
                    curr = next;
                } else {
                    return find(key);
                }
            }
        }
        return new Result(pred, null, false, isLeft);
    }

    private SuccessorResult findSuccessor(Node node) {
        boolean[] rm = {false};
        Node right = node.right.get(rm);
        if (right == null || rm[0]) return null;

        boolean[] lm = {false};
        Node leftOfRight = right.left.get(lm);
        if (leftOfRight == null || lm[0]) {
            return new SuccessorResult(node, right);
        }

        Node parent = right;
        Node curr = leftOfRight;
        while (true) {
            boolean[] clm = {false};
            Node leftOfCurr = curr.left.get(clm);
            if (leftOfCurr == null || clm[0]) {
                return new SuccessorResult(parent, curr);
            }
            parent = curr;
            curr = leftOfCurr;
        }
    }
}