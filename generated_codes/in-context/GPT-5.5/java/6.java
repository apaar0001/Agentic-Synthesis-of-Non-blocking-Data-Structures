package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Lock-free BST-style integer set.
 *
 * Uses CAS for child insertion and logical deletion via AtomicBoolean.
 * add(): CAS child pointer or revive logically deleted node.
 * remove(): logical deletion; physically unlinks leaf / single-child nodes when safe.
 * contains(): wait-free traversal ignoring logically deleted nodes.
 */
public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        final int key;
        final AtomicReference<Node> left;
        final AtomicReference<Node> right;
        final AtomicBoolean deleted;

        Node(int key) {
            this.key = key;
            this.left = new AtomicReference<>(null);
            this.right = new AtomicReference<>(null);
            this.deleted = new AtomicBoolean(false);
        }
    }

    private final Node root;

    public ConcurrentDataStructure() {
        root = new Node(Integer.MIN_VALUE);
    }

    @Override
    public boolean add(int key) {
        if (key == Integer.MIN_VALUE)
            return false;

        while (true) {
            Node parent = root;
            Node curr = root.right.get();

            if (curr == null) {
                Node node = new Node(key);
                if (root.right.compareAndSet(null, node))
                    return true;
                continue;
            }

            while (true) {
                if (key == curr.key) {
                    if (!curr.deleted.get())
                        return false;
                    if (curr.deleted.compareAndSet(true, false))
                        return true;
                    break;
                }

                parent = curr;
                AtomicReference<Node> nextRef = key < curr.key ? curr.left : curr.right;
                Node next = nextRef.get();

                if (next == null) {
                    Node node = new Node(key);
                    if (nextRef.compareAndSet(null, node))
                        return true;
                    break;
                }

                curr = next;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        if (key == Integer.MIN_VALUE)
            return false;

        while (true) {
            SearchResult result = findWithParent(key);
            Node parent = result.parent;
            Node curr = result.curr;

            if (curr == null || curr.key != key || curr.deleted.get())
                return false;

            if (!curr.deleted.compareAndSet(false, true))
                continue;

            // Node has been marked
            tryUnlink(parent, curr);
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        if (key == Integer.MIN_VALUE)
            return false;

        Node curr = root.right.get();

        while (curr != null) {
            if (key == curr.key)
                return !curr.deleted.get();

            curr = key < curr.key ? curr.left.get() : curr.right.get();
        }

        return false;
    }

    private static class SearchResult {
        final Node parent;
        final Node curr;

        SearchResult(Node parent, Node curr) {
            this.parent = parent;
            this.curr = curr;
        }
    }

    private SearchResult findWithParent(int key) {
        Node parent = root;
        Node curr = root.right.get();

        while (curr != null) {
            if (key == curr.key)
                return new SearchResult(parent, curr);

            parent = curr;
            curr = key < curr.key ? curr.left.get() : curr.right.get();
        }

        return new SearchResult(parent, null);
    }

    private void tryUnlink(Node parent, Node curr) {
        Node left = curr.left.get();
        Node right = curr.right.get();

        if (left != null && right != null)
            return;

        Node child = left != null ? left : right;

        AtomicReference<Node> parentLink;
        if (parent.left.get() == curr) {
            parentLink = parent.left;
        } else if (parent.right.get() == curr) {
            parentLink = parent.right;
        } else {
            return;
        }

        parentLink.compareAndSet(curr, child);
    }
}