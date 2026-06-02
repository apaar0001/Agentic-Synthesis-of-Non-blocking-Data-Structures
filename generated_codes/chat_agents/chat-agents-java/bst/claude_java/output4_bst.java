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
        final AtomicMarkableReference<Node> predRef;
        final Node curr;

        Window(Node pred, AtomicMarkableReference<Node> predRef, Node curr) {
            this.pred = pred;
            this.predRef = predRef;
            this.curr = curr;
        }
    }

    private final AtomicMarkableReference<Node> root;

    public ConcurrentDataStructure() {
        this.root = new AtomicMarkableReference<>(null, false);
    }

    private Window find(int key) {
        retry:
        while (true) {
            Node pred = null;
            AtomicMarkableReference<Node> predRef = root;
            boolean[] predMark = {false};
            Node curr = predRef.get(predMark);

            if (curr == null) {
                return new Window(pred, predRef, null);
            }

            while (curr != null) {
                boolean[] currMark = {false};
                AtomicMarkableReference<Node> currRef;
                Node next;

                if (key < curr.key) {
                    currRef = curr.left;
                } else if (key > curr.key) {
                    currRef = curr.right;
                } else {
                    currRef = null;
                }

                if (currRef != null) {
                    next = currRef.get(currMark);
                } else {
                    return new Window(pred, predRef, curr);
                }

                if (currMark[0]) {
                    // curr's chosen child ref is marked; help remove and restart
                    continue retry;
                }

                // Check if curr itself is marked (logically deleted)
                boolean[] selfMark = {false};
                if (key < curr.key) {
                    curr.left.get(selfMark);
                } else {
                    curr.right.get(selfMark);
                }

                // Check if predRef points to curr and curr is not marked
                boolean[] predChildMark = {false};
                Node predChild = predRef.get(predChildMark);
                if (predChild != curr) {
                    continue retry;
                }

                pred = curr;
                predRef = currRef;
                curr = next;
            }

            return new Window(pred, predRef, null);
        }
    }

    private Window findExact(int key) {
        retry:
        while (true) {
            AtomicMarkableReference<Node> predRef = root;
            Node pred = null;
            boolean[] mark = {false};
            Node curr = predRef.get(mark);

            while (curr != null) {
                boolean[] currMark = {false};
                Node leftNode = curr.left.get(currMark);
                if (currMark[0]) {
                    continue retry;
                }
                Node rightNode = curr.right.get(currMark);
                if (currMark[0]) {
                    continue retry;
                }

                if (key == curr.key) {
                    return new Window(pred, predRef, curr);
                } else if (key < curr.key) {
                    pred = curr;
                    predRef = curr.left;
                    curr = leftNode;
                } else {
                    pred = curr;
                    predRef = curr.right;
                    curr = rightNode;
                }
            }

            return new Window(pred, predRef, null);
        }
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Window w = findExact(key);
            if (w.curr != null) {
                return false;
            }
            Node newNode = new Node(key);
            if (w.predRef.compareAndSet(null, newNode, false, false)) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Window w = findExact(key);
            if (w.curr == null) {
                return false;
            }
            Node curr = w.curr;
            boolean[] lm = {false};
            Node leftChild = curr.left.get(lm);
            boolean[] rm = {false};
            Node rightChild = curr.right.get(rm);

            if (leftChild == null && rightChild == null) {
                // Leaf node: logically mark by marking one child ref
                if (!curr.left.compareAndSet(null, null, false, true)) {
                    continue;
                }
                // Node has been marked
                w.predRef.compareAndSet(curr, null, false, false);
                return true;
            } else if (leftChild == null || rightChild == null) {
                Node onlyChild = (leftChild != null) ? leftChild : rightChild;
                AtomicMarkableReference<Node> onlyRef = (leftChild != null) ? curr.left : curr.right;
                if (!onlyRef.compareAndSet(onlyChild, onlyChild, false, true)) {
                    continue;
                }
                // Node has been marked
                w.predRef.compareAndSet(curr, onlyChild, false, false);
                return true;
            } else {
                // Two children: find in-order successor (leftmost in right subtree)
                Node successorParent = curr;
                AtomicMarkableReference<Node> successorParentRef = curr.right;
                boolean[] sm = {false};
                Node successor = curr.right.get(sm);
                if (sm[0]) continue;

                while (true) {
                    boolean[] slm = {false};
                    Node succLeft = successor.left.get(slm);
                    if (slm[0]) { continue; }
                    if (succLeft == null) break;
                    successorParent = successor;
                    successorParentRef = successor.left;
                    successor = succLeft;
                }

                boolean[] succRightMark = {false};
                Node succRight = successor.right.get(succRightMark);
                if (succRightMark[0]) continue;

                // Mark successor's right child ref to logically delete successor
                if (!successor.right.compareAndSet(succRight, succRight, false, true)) {
                    continue;
                }
                // Node has been marked

                Node replacement = new Node(successor.key);
                replacement.left.set(leftChild, false);
                replacement.right.set(rightChild, false);

                if (!w.predRef.compareAndSet(curr, replacement, false, false)) {
                    continue;
                }

                if (successorParent == curr) {
                    replacement.right.compareAndSet(rightChild, succRight, false, false);
                } else {
                    successorParentRef.compareAndSet(successor, succRight, false, false);
                }

                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        boolean[] mark = {false};
        Node curr = root.get(mark);
        while (curr != null) {
            boolean[] lm = {false};
            boolean[] rm = {false};
            Node left = curr.left.get(lm);
            Node right = curr.right.get(rm);

            if (key == curr.key) {
                return !lm[0] && !rm[0];
            } else if (key < curr.key) {
                if (lm[0]) return false;
                curr = left;
            } else {
                if (rm[0]) return false;
                curr = right;
            }
        }
        return false;
    }
}