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
            boolean[] predMarked = {false};
            Node curr = predRef.get(predMarked);

            while (curr != null) {
                boolean[] currMarked = {false};
                AtomicMarkableReference<Node> currRef = (key < curr.key) ? curr.left : (key > curr.key) ? curr.right : null;

                Node next;
                if (currRef != null) {
                    next = currRef.get(currMarked);
                } else {
                    currMarked[0] = false;
                    next = null;
                    currRef = (key <= curr.key) ? curr.left : curr.right;
                }

                if (predMarked[0]) {
                    continue retry;
                }

                if (currMarked[0]) {
                    Node snip = curr;
                    if (!predRef.compareAndSet(snip, next, false, false)) {
                        continue retry;
                    }
                    curr = next;
                    if (curr != null) {
                        currRef = (key < curr.key) ? curr.left : (key > curr.key) ? curr.right : null;
                        if (currRef != null) {
                            currMarked[0] = false;
                        }
                    }
                    continue;
                }

                if (curr.key == key) {
                    return new Window(pred, predRef, curr);
                }

                pred = curr;
                predRef = (key < curr.key) ? curr.left : curr.right;
                predMarked[0] = false;
                curr = predRef.get(predMarked);
            }

            return new Window(pred, predRef, null);
        }
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Window w = find(key);
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
            Window w = find(key);
            if (w.curr == null) {
                return false;
            }
            Node curr = w.curr;
            boolean[] lMarked = {false};
            boolean[] rMarked = {false};
            Node left = curr.left.get(lMarked);
            Node right = curr.right.get(rMarked);

            if (lMarked[0] || rMarked[0]) {
                continue;
            }

            // Case: leaf node
            if (left == null && right == null) {
                if (!curr.left.compareAndSet(null, null, false, true)) {
                    continue;
                }
                // Node has been marked
                if (!curr.right.compareAndSet(null, null, false, true)) {
                    curr.left.compareAndSet(null, null, true, false);
                    continue;
                }
                if (w.predRef.compareAndSet(curr, null, false, false)) {
                    return true;
                }
                curr.left.compareAndSet(null, null, true, false);
                curr.right.compareAndSet(null, null, true, false);
                continue;
            }

            // Case: one child
            if (left == null || right == null) {
                Node child = (left != null) ? left : right;
                AtomicMarkableReference<Node> childRef = (left != null) ? curr.left : curr.right;
                AtomicMarkableReference<Node> emptyRef = (left != null) ? curr.right : curr.left;

                if (!emptyRef.compareAndSet(null, null, false, true)) {
                    continue;
                }
                // Node has been marked
                if (!childRef.compareAndSet(child, child, false, true)) {
                    emptyRef.compareAndSet(null, null, true, false);
                    continue;
                }
                if (w.predRef.compareAndSet(curr, child, false, false)) {
                    return true;
                }
                emptyRef.compareAndSet(null, null, true, false);
                childRef.compareAndSet(child, child, true, false);
                continue;
            }

            // Case: two children - find in-order successor
            Node succParent = curr;
            AtomicMarkableReference<Node> succParentRef = curr.right;
            boolean[] succParentMarked = {false};
            Node succ = curr.right.get(succParentMarked);
            if (succParentMarked[0]) continue;

            boolean[] succMarked = {false};
            Node succLeft = succ.left.get(succMarked);
            while (succLeft != null && !succMarked[0]) {
                succParent = succ;
                succParentRef = succ.left;
                succ = succLeft;
                succLeft = succ.left.get(succMarked);
            }
            if (succMarked[0]) continue;

            boolean[] succRightMarked = {false};
            Node succRight = succ.right.get(succRightMarked);
            if (succRightMarked[0]) continue;

            Node replacement = new Node(succ.key);

            boolean[] currLM = {false};
            boolean[] currRM = {false};
            Node currLeft = curr.left.get(currLM);
            Node currRight = curr.right.get(currRM);
            if (currLM[0] || currRM[0]) continue;

            replacement.left.compareAndSet(null, currLeft, false, false);
            if (succ == currRight) {
                replacement.right.compareAndSet(null, succRight, false, false);
            } else {
                replacement.right.compareAndSet(null, currRight, false, false);
            }

            if (!curr.left.compareAndSet(currLeft, currLeft, false, true)) {
                continue;
            }
            // Node has been marked
            if (!curr.right.compareAndSet(currRight, currRight, false, true)) {
                curr.left.compareAndSet(currLeft, currLeft, true, false);
                continue;
            }

            if (!w.predRef.compareAndSet(curr, replacement, false, false)) {
                curr.left.compareAndSet(currLeft, currLeft, true, false);
                curr.right.compareAndSet(currRight, currRight, true, false);
                continue;
            }

            // Unlink successor
            if (succ != currRight) {
                succParentRef.compareAndSet(succ, succRight, false, false);
            }

            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        boolean[] marked = {false};
        Node curr = root.get(marked);
        while (curr != null) {
            if (key == curr.key) {
                boolean[] m = {false};
                curr.left.get(m);
                if (m[0]) return false;
                curr.right.get(m);
                if (m[0]) return false;
                return true;
            }
            AtomicMarkableReference<Node> nextRef = (key < curr.key) ? curr.left : curr.right;
            curr = nextRef.get(marked);
            if (marked[0]) {
                return find(key).curr != null;
            }
        }
        return false;
    }
}