package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        int key;
        final AtomicMarkableReference<Node> left;
        final AtomicMarkableReference<Node> right;
        Node(int k) {
            key = k;
            left = new AtomicMarkableReference<>(null, false);
            right = new AtomicMarkableReference<>(null, false);
        }
    }

    private final AtomicMarkableReference<Node> root = new AtomicMarkableReference<>(null, false);

    private static Node getReference(AtomicMarkableReference<Node> ref, boolean[] marked) {
        return ref.getReference(marked);
    }

    private void helpRemove(AtomicMarkableReference<Node> parentRef, Node markedNode) {
        boolean[] marked = {true};
        Node expected = markedNode;
        Node left = getReference(markedNode.left, new boolean[1]);
        Node right = getReference(markedNode.right, new boolean[1]);
        Node replacement = (left != null) ? left : right;
        parentRef.compareAndSet(expected, replacement, true, false);
    }

    private boolean tryMarkNode(AtomicMarkableReference<Node> parentRef, Node node) {
        boolean[] marked = {false};
        Node expected = getReference(parentRef, marked);
        if (expected != node || marked[0]) {
            return false;
        }
        boolean success = parentRef.compareAndSet(expected, node, false, true);
        if (success) {
            // Node has been marked
        }
        return success;
    }

    private Node findNode(int key, AtomicMarkableReference<Node>[] parentHolder) {
        boolean[] marked = {false};
        AtomicMarkableReference<Node> currRef = root;
        Node curr = getReference(currRef, marked);
        parentHolder[0] =