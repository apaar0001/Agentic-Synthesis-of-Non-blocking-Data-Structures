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
        Node pred;
        Node curr;
        boolean isLeft;

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

    private Node getRef(AtomicMarkableReference<Node> amr) {
        boolean[] marked = {false};
        Node n = amr.get(marked);
        return marked[0] ? null : n;
    }

    private boolean isMarked(AtomicMarkableReference<Node> amr) {
        boolean[] marked = {false};
        amr.get(marked);
        return marked[0];
    }

    @Override
    public boolean add(int key) {
        while (true) {
            boolean[] marked = {false};
            Node rootNode = root.get(marked);
            if (marked[0]) continue;

            if (rootNode == null) {
                Node newNode = new Node(key);
                if (root.compareAndSet(null, newNode, false, false)) {
                    return true;
                }
                continue;
            }

            Result result = find(key);
            if (result == null) continue;

            if (result.found) return false;

            Node newNode = new Node(key);
            AtomicMarkableReference<Node> childRef = result.isLeft
                    ? result.parent.left
                    : result.parent.right;

            if (childRef.compareAndSet(null, newNode, false, false)) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Result result = find(key);
            if (result == null) continue;
            if (!result.found) return false;

            Node target = result.node;

            boolean[] lMarked = {false};
            boolean[] rMarked = {false};
            Node leftChild = target.left.get(lMarked);
            Node rightChild = target.right.get(rMarked);

            if (lMarked[0] || rMarked[0]) continue;

            int childCount = (leftChild != null ? 1 : 0) + (rightChild != null ? 1 : 0);

            if (childCount == 2) {
                // Find in-order successor
                Node succParent = target;
                AtomicMarkableReference<Node> succParentRef = target.right;
                Node succ = rightChild;

                boolean[] sMarked = {false};
                Node succLeft = succ.left.get(sMarked);
                while (!sMarked[0] && succLeft != null) {
                    succParent = succ;
                    succParentRef = succ.left;
                    succ = succLeft;
                    succLeft = succ.left.get(sMarked);
                }
                if (sMarked[0]) continue;

                // Logically mark the target node by marking one of its children references
                // We mark the node itself via a logical deletion flag by marking its right ref
                // Instead use a sentinel approach: mark target logically
                // Mark target by setting mark on left child ref
                boolean[] tlMarked = {false};
                Node tlChild = target.left.get(tlMarked);
                if (tlMarked[0]) continue;
                if (!target.left.compareAndSet(tlChild, tlChild, false, true)) continue;
                // Node has been marked

                // Now physically handle: replace target key with succ key not possible since key is final
                // Use structural approach: mark succ, graft succ's right child
                boolean[] srMarked = {false};
                Node succRight = succ.right.get(srMarked);
                if (srMarked[0]) {
                    target.left.compareAndSet(tlChild, tlChild, true, false);
                    continue;
                }
                if (!succ.right.compareAndSet(succRight, succRight, false, true)) {
                    target.left.compareAndSet(tlChild, tlChild, true, false);
                    continue;
                }

                Node replacement = new Node(succ.key);
                replacement.left.set(tlChild, false);
                boolean[] trMarked = {false};
                Node trChild = target.right.get(trMarked);
                if (trMarked[0]) {
                    target.left.compareAndSet(tlChild, tlChild, true, false);
                    continue;
                }
                replacement.right.set(succRight, false);

                if (result.parent == null) {
                    if (!root.compareAndSet(target, replacement, false, false)) {
                        target.left.compareAndSet(tlChild, tlChild, true, false);
                        continue;
                    }
                } else {
                    AtomicMarkableReference<Node> parentChildRef =
                            result.isLeft ? result.parent.left : result.parent.right;
                    if (!parentChildRef.compareAndSet(target, replacement, false, false)) {
                        target.left.compareAndSet(tlChild, tlChild, true, false);
                        continue;
                    }
                }

                // Unlink succ
                if (succParent == target) {
                    replacement.right.compareAndSet(succRight, succRight, false, false);
                } else {
                    succParentRef.compareAndSet(succ, succRight, false, false);
                }
                return true;

            } else {
                // 0 or 1 child: logically mark then physically unlink
                Node child = (leftChild != null) ? leftChild : rightChild;

                boolean[] tlMarked2 = {false};
                Node tl = target.left.get(tlMarked2);
                if (tlMarked2[0]) continue;
                if (!target.left.compareAndSet(tl, tl, false, true)) continue;
                // Node has been marked

                if (result.parent == null) {
                    root.compareAndSet(target, child, false, false);
                } else {
                    AtomicMarkableReference<Node> parentChildRef =
                            result.isLeft ? result.parent.left : result.parent.right;
                    parentChildRef.compareAndSet(target, child, false, false);
                }
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        boolean[] marked = {false};
        Node curr = root.get(marked);
        while (curr != null && !marked[0]) {
            if (key == curr.key) {
                boolean[] lm = {false};
                curr.left.get(lm);
                return !lm[0];
            } else if (key < curr.key) {
                curr = curr.left.get(marked);
            } else {
                curr = curr.right.get(marked);
            }
        }
        return false;
    }

    private static class Result {
        Node parent;
        Node node;
        boolean isLeft;
        boolean found;

        Result(Node parent, Node node, boolean isLeft, boolean found) {
            this.parent = parent;
            this.node = node;
            this.isLeft = isLeft;
            this.found = found;
        }
    }

    private Result find(int key) {
        boolean[] marked = {false};
        Node parent = null;
        boolean isLeft = false;
        Node curr = root.get(marked);
        if (marked[0]) return null;

        while (curr != null) {
            boolean[] lm = {false};
            curr.left.get(lm);
            if (lm[0]) {
                // curr is logically deleted, retry from root
                return null;
            }

            if (key == curr.key) {
                return new Result(parent, curr, isLeft, true);
            } else if (key < curr.key) {
                Node next = curr.left.get(marked);
                if (marked[0]) return null;
                parent = curr;
                isLeft = true;
                curr = next;
            } else {
                Node next = curr.right.get(marked);
                if (marked[0]) return null;
                parent = curr;
                isLeft = false;
                curr = next;
            }
        }
        return new Result(parent, null, isLeft, false);
    }
}