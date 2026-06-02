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

    private final Node root;

    public ConcurrentDataStructure() {
        this.root = new Node(Integer.MAX_VALUE);
    }

    private static class Window {
        Node parent;
        Node curr;
        AtomicMarkableReference<Node> currRef;

        Window(Node parent, Node curr, AtomicMarkableReference<Node> currRef) {
            this.parent = parent;
            this.curr = curr;
            this.currRef = currRef;
        }
    }

    private Window find(int key) {
        Node parent;
        Node curr;
        AtomicMarkableReference<Node> currRef;

        retry:
        while (true) {
            parent = root;
            currRef = root.left;
            curr = currRef.getReference();

            while (curr != null) {
                boolean[] leftMarked = new boolean[1];
                boolean[] rightMarked = new boolean[1];
                Node leftNode = curr.left.get(leftMarked);
                Node rightNode = curr.right.get(rightMarked);

                if (leftMarked[0] || rightMarked[0]) {
                    if (!cleanUp(parent, curr)) {
                        continue retry;
                    }
                    currRef = (key < parent.key) ? parent.left : parent.right;
                    curr = currRef.getReference();
                    continue;
                }

                if (key == curr.key) {
                    return new Window(parent, curr, currRef);
                }

                parent = curr;
                if (key < curr.key) {
                    currRef = curr.left;
                } else {
                    currRef = curr.right;
                }
                curr = currRef.getReference();
            }
            return new Window(parent, null, currRef);
        }
    }

    private boolean cleanUp(Node parent, Node curr) {
        boolean[] leftMarked = new boolean[1];
        boolean[] rightMarked = new boolean[1];
        Node leftNode = curr.left.get(leftMarked);
        Node rightNode = curr.right.get(rightMarked);

        if (!leftMarked[0] && !rightMarked[0]) {
            return true;
        }

        if (leftMarked[0] && rightMarked[0]) {
            Node replacement = null;
            AtomicMarkableReference<Node> parentRef = (curr.key < parent.key) ? parent.left : parent.right;
            return parentRef.compareAndSet(curr, replacement, false, false);
        }

        Node child = leftMarked[0] ? rightNode : leftNode;
        AtomicMarkableReference<Node> parentRef = (curr.key < parent.key) ? parent.left : parent.right;
        return parentRef.compareAndSet(curr, child, false, false);
    }

    @Override
    public boolean add(int key) {
        if (key == Integer.MAX_VALUE) {
            return false;
        }
        while (true) {
            Window w = find(key);
            if (w.curr != null) {
                return false;
            }
            Node newNode = new Node(key);
            if (w.currRef.compareAndSet(null, newNode, false, false)) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        if (key == Integer.MAX_VALUE) {
            return false;
        }
        while (true) {
            Window w = find(key);
            if (w.curr == null) {
                return false;
            }
            Node curr = w.curr;

            boolean[] leftMarked = new boolean[1];
            boolean[] rightMarked = new boolean[1];
            Node leftNode = curr.left.get(leftMarked);
            Node rightNode = curr.right.get(rightMarked);

            if (leftMarked[0] || rightMarked[0]) {
                cleanUp(w.parent, curr);
                continue;
            }

            if (leftNode == null && rightNode == null) {
                if (curr.left.compareAndSet(null, null, false, true)) {
                    if (curr.right.compareAndSet(null, null, false, true)) {
                        // Node has been marked
                        cleanUp(w.parent, curr);
                        return true;
                    } else {
                        curr.left.attemptMark(null, false);
                    }
                }
            } else if (leftNode == null) {
                if (curr.left.compareAndSet(null, null, false, true)) {
                    // Node has been marked
                    cleanUp(w.parent, curr);
                    return true;
                }
            } else if (rightNode == null) {
                if (curr.right.compareAndSet(null, null, false, true)) {
                    // Node has been marked
                    cleanUp(w.parent, curr);
                    return true;
                }
            } else {
                Node succParent = curr;
                AtomicMarkableReference<Node> succRef = curr.right;
                Node succ = succRef.getReference();

                while (succ != null) {
                    boolean[] sLeftMarked = new boolean[1];
                    Node sLeft = succ.left.get(sLeftMarked);
                    if (sLeftMarked[0]) {
                        break;
                    }
                    if (sLeft == null) {
                        break;
                    }
                    succParent = succ;
                    succRef = succ.left;
                    succ = sLeft;
                }

                if (succ == null) {
                    continue;
                }

                boolean[] sRightMarked = new boolean[1];
                Node sRight = succ.right.get(sRightMarked);
                if (sRightMarked[0]) {
                    continue;
                }

                if (succ.left.compareAndSet(null, null, false, true)) {
                    // Node has been marked
                    int succKey = succ.key;
                    if (remove(succKey)) {
                        Node replacement = new Node(succKey);
                        replacement.left.set(curr.left.getReference(), false);
                        replacement.right.set(curr.right.getReference(), false);
                        AtomicMarkableReference<Node> parentRef = (curr.key < w.parent.key) ? w.parent.left : w.parent.right;
                        if (parentRef.compareAndSet(curr, replacement, false, false)) {
                            curr.left.attemptMark(curr.left.getReference(), true);
                            curr.right.attemptMark(curr.right.getReference(), true);
                            return true;
                        }
                    }
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        if (key == Integer.MAX_VALUE) {
            return false;
        }
        Node curr = root.left.getReference();
        while (curr != null) {
            boolean[] leftMarked = new boolean[1];
            boolean[] rightMarked = new boolean[1];
            curr.left.get(leftMarked);
            curr.right.get(rightMarked);

            if (!leftMarked[0] && !rightMarked[0] && curr.key == key) {
                return true;
            }
            if (key < curr.key) {
                curr = curr.left.getReference();
            } else {
                curr = curr.right.getReference();
            }
        }
        return false;
    }
}