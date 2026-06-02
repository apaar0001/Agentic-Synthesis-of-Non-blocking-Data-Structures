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
        retry:
        while (true) {
            Node parent = root;
            AtomicMarkableReference<Node> currRef = root.left;
            Node curr = currRef.getReference();

            while (curr != null) {
                boolean[] leftMarked = new boolean[1];
                Node leftNode = curr.left.get(leftMarked);
                if (leftMarked[0]) {
                    if (!cleanUp(parent, currRef, curr)) {
                        continue retry;
                    }
                    curr = currRef.getReference();
                    continue;
                }

                boolean[] rightMarked = new boolean[1];
                Node rightNode = curr.right.get(rightMarked);
                if (rightMarked[0]) {
                    if (!cleanUp(parent, currRef, curr)) {
                        continue retry;
                    }
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

    private boolean cleanUp(Node parent, AtomicMarkableReference<Node> currRef, Node curr) {
        boolean[] leftMarked = new boolean[1];
        Node leftNode = curr.left.get(leftMarked);
        boolean[] rightMarked = new boolean[1];
        Node rightNode = curr.right.get(rightMarked);

        if (leftNode == null && rightNode == null) {
            return currRef.compareAndSet(curr, null, false, false);
        } else if (leftNode == null) {
            return currRef.compareAndSet(curr, rightNode, false, false);
        } else if (rightNode == null) {
            return currRef.compareAndSet(curr, leftNode, false, false);
        } else {
            Node succParent = curr;
            AtomicMarkableReference<Node> succRef = curr.right;
            Node succ = succRef.getReference();
            while (succ != null) {
                boolean[] sLeftMarked = new boolean[1];
                Node sLeft = succ.left.get(sLeftMarked);
                if (sLeftMarked[0]) {
                    cleanUp(succParent, succRef, succ);
                    return false;
                }
                if (sLeft == null) {
                    break;
                }
                succParent = succ;
                succRef = succ.left;
                succ = sLeft;
            }
            if (succ == null) return false;
            
            boolean[] sRightMarked = new boolean[1];
            Node sRight = succ.right.get(sRightMarked);
            if (sRightMarked[0]) {
                cleanUp(succParent, succRef, succ);
                return false;
            }

            int succKey = succ.key;
            if (succRef.compareAndSet(succ, succ, false, true)) {
                // Node has been marked
                cleanUp(succParent, succRef, succ);
                Node newNode = new Node(succKey);
                newNode.left.set(leftNode, false);
                newNode.right.set(rightNode, false);
                return currRef.compareAndSet(curr, newNode, false, false);
            }
            return false;
        }
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
            Node leftNode = curr.left.get(leftMarked);
            if (leftMarked[0]) continue;

            if (curr.left.compareAndSet(leftNode, leftNode, false, true)) {
                // Node has been marked
                cleanUp(w.parent, w.currRef, curr);
                return true;
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
            curr.left.get(leftMarked);
            boolean[] rightMarked = new boolean[1];
            curr.right.get(rightMarked);

            if (!leftMarked[0] && !rightMarked[0]) {
                if (key == curr.key) {
                    return true;
                }
                curr = (key < curr.key) ? curr.left.getReference() : curr.right.getReference();
            } else {
                Window w = find(key);
                return w.curr != null;
            }
        }
        return false;
    }
}