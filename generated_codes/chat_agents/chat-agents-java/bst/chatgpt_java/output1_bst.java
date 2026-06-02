package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
private static final class Node {
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

@Override
public boolean add(int key) {
    while (true) {
        Node parent = root;
        Node curr = root.left.getReference();

        while (curr != null) {
            if (isMarked(curr)) {
                helpUnlink(parent, curr);
                break;
            }

            if (key == curr.key) {
                return false;
            }

            parent = curr;
            curr = key < curr.key ? curr.left.getReference() : curr.right.getReference();
        }

        if (curr != null) {
            continue;
        }

        Node node = new Node(key);
        AtomicMarkableReference<Node> childRef = key < parent.key ? parent.left : parent.right;

        if (childRef.compareAndSet(null, node, false, false)) {
            return true;
        }
    }
}

@Override
public boolean remove(int key) {
    while (true) {
        Node parent = root;
        Node curr = root.left.getReference();

        while (curr != null) {
            if (isMarked(curr)) {
                helpUnlink(parent, curr);
                break;
            }

            if (key == curr.key) {
                boolean[] mark = {false};
                curr.right.get(mark);

                if (mark[0]) {
                    return false;
                }

                Node right = curr.right.getReference();
                if (curr.right.compareAndSet(right, right, false, true)) {
                    // Node has been marked
                    curr.left.attemptMark(curr.left.getReference(), true);
                    helpUnlink(parent, curr);
                    return true;
                }

                break;
            }

            parent = curr;
            curr = key < curr.key ? curr.left.getReference() : curr.right.getReference();
        }

        if (curr == null) {
            return false;
        }
    }
}

@Override
public boolean contains(int key) {
    Node curr = root.left.getReference();

    while (curr != null) {
        if (key == curr.key) {
            return !isMarked(curr);
        }
        curr = key < curr.key ? curr.left.getReference() : curr.right.getReference();
    }

    return false;
}

private boolean isMarked(Node node) {
    return node.left.isMarked() || node.right.isMarked();
}

private void helpUnlink(Node parent, Node node) {
    if (parent == null || node == null) {
        return;
    }

    Node left = node.left.getReference();
    Node right = node.right.getReference();

    if (left != null && right != null) {
        return;
    }

    Node replacement = left != null ? left : right;
    AtomicMarkableReference<Node> parentRef =
            node.key < parent.key ? parent.left : parent.right;

    parentRef.compareAndSet(node, replacement, false, false);
}


}
