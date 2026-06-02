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

    private final AtomicMarkableReference<Node> root;

    public ConcurrentDataStructure() {
        this.root = new AtomicMarkableReference<>(null, false);
    }

    @Override
    public boolean add(int key) {
        while (true) {
            AtomicMarkableReference<Node> currRef = root;
            Node curr = currRef.getReference();
            boolean currMarked = currRef.isMarked();

            if (currMarked) {
                continue;
            }
            if (curr == null) {
                Node newNode = new Node(key);
                if (root.compareAndSet(null, newNode, false, false)) {
                    return true;
                }
                continue;
            }

            AtomicMarkableReference<Node> parentRef = currRef;
            Node parent = curr;
            while (true) {
                Node node = parentRef.getReference();
                boolean nodeMarked = parentRef.isMarked();

                if (nodeMarked) {
                    break;
                }
                if (key == node.key) {
                    return false;
                }
                boolean goLeft = key < node.key;
                AtomicMarkableReference<Node> childRef = goLeft ? node.left : node.right;
                Node child = childRef.getReference();
                boolean childMarked = childRef.isMarked();

                if (child == null || childMarked) {
                    Node newNode = new Node(key);
                    if (childRef.compareAndSet(child, newNode, childMarked, false)) {
                        return true;
                    }
                    break;
                } else {
                    parentRef = childRef;
                    parent = node;
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            AtomicMarkableReference<Node> prevRef = root;
            Node prev = prevRef.getReference();
            boolean prevMarked = prevRef.isMarked();

            if (prevMarked) {
                continue;
            }
            if (prev == null) {
                return false;
            }

            boolean found = false;
            AtomicMarkableReference<Node> targetRef = null;
            Node target = null;

            while (true) {
                Node node = prevRef.getReference();
                boolean nodeMarked = prevRef.isMarked();

                if (nodeMarked) {
                    break;
                }
                if (key == node.key) {
                    found = true;
                    targetRef = prevRef;
                    target = node;
                    break;
                }
                boolean goLeft = key < node.key;
                AtomicMarkableReference<Node> nextRef = goLeft ? node.left : node.right;
                Node next = nextRef.getReference();
                boolean nextMarked = nextRef.isMarked();

                if (next == null || nextMarked) {
                    break;
                }
                prevRef = nextRef;
                prev = next;
            }

            if (!found) {
                return false;
            }

            if (targetRef.attemptMark(target, true)) {
                // Node has been marked
                Node left = target.left.getReference();
                Node right = target.right.getReference();
                Node replacement = (left != null) ? left : right;
                if (targetRef.compareAndSet(target, replacement, true, false)) {
                    return true;
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        AtomicMarkableReference<Node> currRef = root;
        while (true) {
            Node curr = currRef.getReference();
            boolean currMarked = currRef.isMarked();

            if (curr == null || currMarked) {
                return false;
            }
            if (key == curr.key) {
                return true;
            }
            boolean goLeft = key < curr.key;
            AtomicMarkableReference<Node> nextRef = goLeft ? curr.left : curr.right;
            currRef = nextRef;
        }
    }
}