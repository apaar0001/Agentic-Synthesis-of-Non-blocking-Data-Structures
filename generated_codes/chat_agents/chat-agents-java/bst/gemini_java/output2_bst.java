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
        AtomicMarkableReference<Node> childRef;

        Window(Node parent, Node curr, AtomicMarkableReference<Node> childRef) {
            this.parent = parent;
            this.curr = curr;
            this.childRef = childRef;
        }
    }

    private Window find(int key) {
        boolean[] currMarked = new boolean[1];
        boolean[] childMarked = new boolean[1];

        retry:
        while (true) {
            Node parent = root;
            AtomicMarkableReference<Node> childRef = root.left;
            Node curr = childRef.getReference();

            while (curr != null) {
                Node succ = curr.left.get(currMarked);
                while (currMarked[0]) {
                    Node next = curr.right.getReference();
                    if (!childRef.compareAndSet(curr, next, false, false)) {
                        continue retry;
                    }
                    curr = next;
                    if (curr == null) break;
                    succ = curr.left.get(currMarked);
                }

                if (curr == null) break;

                succ = curr.right.get(currMarked);
                while (currMarked[0]) {
                    Node next = curr.left.getReference();
                    if (!childRef.compareAndSet(curr, next, false, false)) {
                        continue retry;
                    }
                    curr = next;
                    if (curr == null) break;
                    succ = curr.right.get(currMarked);
                }

                if (curr == null) break;

                if (key == curr.key) {
                    return new Window(parent, curr, childRef);
                }

                parent = curr;
                if (key < curr.key) {
                    childRef = curr.left;
                } else {
                    childRef = curr.right;
                }
                curr = childRef.getReference();
            }
            return new Window(parent, null, childRef);
        }
    }

    @Override
    public boolean add(int key) {
        if (key == Integer.MAX_VALUE) {
            return false;
        }
        while (true) {
            Window window = find(key);
            if (window.curr != null) {
                return false;
            }
            Node newNode = new Node(key);
            if (window.childRef.compareAndSet(null, newNode, false, false)) {
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
            Window window = find(key);
            if (window.curr == null) {
                return false;
            }
            Node curr = window.curr;
            if (curr.left.compareAndSet(null, curr, false, true)) {
                // Node has been marked
                curr.right.compareAndSet(null, null, false, true);
                window.childRef.compareAndSet(curr, null, false, false);
                return true;
            } else {
                boolean[] marked = new boolean[1];
                Node leftNode = curr.left.get(marked);
                if (marked[0]) {
                    if (curr.right.compareAndSet(null, null, false, true)) {
                        // Node has been marked
                        window.childRef.compareAndSet(curr, null, false, false);
                        return true;
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
        boolean[] marked = new boolean[1];
        Node curr = root.left.getReference();
        while (curr != null) {
            curr.left.get(marked);
            boolean leftMarked = marked[0];
            curr.right.get(marked);
            boolean rightMarked = marked[0];

            if (!leftMarked && !rightMarked && key == curr.key) {
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