package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

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

    private final Node header; // dummy node with key Integer.MIN_VALUE

    public ConcurrentDataStructure() {
        header = new Node(Integer.MIN_VALUE);
        // header.left unused; header.right points to the actual tree root
        header.right.set(null, false);
    }

    @Override
    public boolean add(int key) {
        while (true) {
            ParentInfo info = findOrInsertParent(key);
            if (info.found) {
                Node found = info.node;
                if (!info.ref.isMarked()) {
                    // key already present
                    return false;
                } else {
                    // found node is logically deleted; try to physically replace it
                    Node newNode = new Node(key);
                    if (info.ref.compareAndSet(found, newNode, true, false)) {
                        return true;
                    }
                    // CAS failed, retry
                }
            } else {
                // insertion point
                Node newNode = new Node(key);
                if (info.ref.compareAndSet(null, newNode, false, false)) {
                    return true;
                }
                // CAS failed, retry
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            ParentInfo info = findParent(key);
            if (info.found && !info.ref.isMarked()) {
                // logically delete the node
                boolean marked = info.ref.attemptMark(info.node, true);
                if (marked) {
                    // Node has been marked
                    return true;
                }
                // CAS to mark failed, retry
            } else {
                // key not present or already deleted
                return false;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node parent = header;
        AtomicMarkableReference<Node> ref = header.right;
        while (true) {
            Node curr = ref.getReference();
            boolean marked = ref.isMarked();
            if (marked) {
                // help remove the marked node
                helpDelete(ref);
                // after helping, reload ref
                curr = ref.getReference();
                marked = ref.isMarked();
                if (curr == null) {
                    return false;
                }
                // stay at same parent and re-evaluate
                continue;
            }
            if (curr == null) {
                return false;
            }
            int cmp = Integer.compare(key, curr.key);
            if (cmp < 0) {
                parent = curr;
                ref = curr.left;
            } else if (cmp > 0) {
                parent = curr;
                ref = curr.right;
            } else {
                // key matches
                return !ref.isMarked();
            }
        }
    }

    private ParentInfo findParent(int key) {
        Node parent = header;
        AtomicMarkableReference<Node> ref = header.right;
        while (true) {
            Node curr = ref.getReference();
            boolean marked = ref.isMarked();
            if (marked) {
                helpDelete(ref);
                curr = ref.getReference();
                marked = ref.isMarked();
                if (curr == null) {
                    return new ParentInfo(parent, ref, null, false);
                }
                continue;
            }
            if (curr == null) {
                return new ParentInfo(parent, ref, null, false);
            }
            int cmp = Integer.compare(key, curr.key);
            if (cmp < 0) {
                parent = curr;
                ref = curr.left;
            } else if (cmp > 0) {
                parent = curr;
                ref = curr.right;
            } else {
                return new ParentInfo(parent, ref, curr, true);
            }
        }
    }

    private ParentInfo findOrInsertParent(int key) {
        Node parent = header;
        AtomicMarkableReference<Node> ref = header.right;
        while (true) {
            Node curr = ref.getReference();
            boolean marked = ref.isMarked();
            if (marked) {
                helpDelete(ref);
                curr = ref.getReference();
                marked = ref.isMarked();
                if (curr == null) {
                    return new ParentInfo(parent, ref, null, false);
                }
                continue;
            }
            if (curr == null) {
                return new ParentInfo(parent, ref, null, false);
            }
            int cmp = Integer.compare(key, curr.key);
            if (cmp < 0) {
                parent = curr;
                ref = curr.left;
            } else if (cmp > 0) {
                parent = curr;
                ref = curr.right;
            } else {
                return new ParentInfo(parent, ref, curr, true);
            }
        }
    }

    private void helpDelete(AtomicMarkableReference<Node> ref) {
        Node marked = ref.getReference();
        boolean markedBit = ref.isMarked();
        if (!markedBit || marked == null) {
            return;
        }
        Node replacement = (marked.left.getReference() != null) ? marked.left.getReference()
                : marked.right.getReference();
        ref.compareAndSet(marked, replacement, true, false);
    }

    private static final class ParentInfo {
        final Node parent;
        final AtomicMarkableReference<Node> ref;
        final Node node;
        final boolean found;

        ParentInfo(Node parent, AtomicMarkableReference<Node> ref, Node node, boolean found) {
            this.parent = parent;
            this.ref = ref;
            this.node = node;
            this.found = found;
        }
    }
}