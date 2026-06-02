package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
private static final class Node {
final int key;
final AtomicMarkableReference<Node> left;
final AtomicMarkableReference<Node> right;
final AtomicMarkableReference<Node> mark;


    Node(int key) {
        this.key = key;
        this.left = new AtomicMarkableReference<>(null, false);
        this.right = new AtomicMarkableReference<>(null, false);
        this.mark = new AtomicMarkableReference<>(this, false);
    }

    boolean isMarked() {
        return mark.isMarked();
    }
}

private static final class Window {
    final Node parent;
    final Node curr;

    Window(Node parent, Node curr) {
        this.parent = parent;
        this.curr = curr;
    }
}

private final AtomicMarkableReference<Node> root;

public ConcurrentDataStructure() {
    this.root = new AtomicMarkableReference<>(null, false);
}

@Override
public boolean add(int key) {
    while (true) {
        Window w = find(key);
        Node parent = w.parent;
        Node curr = w.curr;

        if (curr != null && curr.key == key && !curr.isMarked()) {
            return false;
        }

        Node node = new Node(key);

        if (parent == null) {
            if (root.compareAndSet(null, node, false, false)) {
                return true;
            }
            continue;
        }

        if (parent.isMarked()) {
            continue;
        }

        AtomicMarkableReference<Node> link = key < parent.key ? parent.left : parent.right;
        Node observed = link.getReference();

        if (observed == null && !link.isMarked()) {
            if (link.compareAndSet(null, node, false, false)) {
                return true;
            }
        }
    }
}

@Override
public boolean remove(int key) {
    while (true) {
        Window w = find(key);
        Node parent = w.parent;
        Node curr = w.curr;

        if (curr == null || curr.key != key || curr.isMarked()) {
            return false;
        }

        if (curr.left.getReference() != null && curr.right.getReference() != null) {
            Node succParent = curr;
            Node succ = curr.right.getReference();

            while (succ != null) {
                if (succ.isMarked()) {
                    helpUnlink(succParent, succ);
                    succ = succParent.right.getReference();
                    continue;
                }

                Node left = succ.left.getReference();
                if (left == null) {
                    break;
                }

                succParent = succ;
                succ = left;
            }

            if (succ == null || succ.isMarked()) {
                continue;
            }

            if (succ.mark.compareAndSet(succ, succ, false, true)) {
                // Node has been marked
                helpUnlink(succParent, succ);
                curr = succ;
            } else {
                continue;
            }
        }

        if (curr.mark.compareAndSet(curr, curr, false, true)) {
            // Node has been marked
            helpUnlink(parent, curr);
            return true;
        }

        if (curr.isMarked()) {
            helpUnlink(parent, curr);
            return false;
        }
    }
}

@Override
public boolean contains(int key) {
    Node curr = root.getReference();

    while (curr != null) {
        if (!curr.isMarked()) {
            if (key == curr.key) {
                return true;
            }
            curr = key < curr.key ? curr.left.getReference() : curr.right.getReference();
        } else {
            curr = key < curr.key ? curr.left.getReference() : curr.right.getReference();
        }
    }

    return false;
}

private Window find(int key) {
    retry:
    while (true) {
        Node parent = null;
        Node curr = root.getReference();

        while (curr != null) {
            if (curr.isMarked()) {
                if (!helpUnlink(parent, curr)) {
                    continue retry;
                }
                curr = parent == null
                        ? root.getReference()
                        : (key < parent.key ? parent.left.getReference() : parent.right.getReference());
                continue;
            }

            if (key == curr.key) {
                return new Window(parent, curr);
            }

            parent = curr;
            curr = key < curr.key ? curr.left.getReference() : curr.right.getReference();
        }

        return new Window(parent, null);
    }
}

private boolean helpUnlink(Node parent, Node curr) {
    if (curr == null || !curr.isMarked()) {
        return true;
    }

    Node left = curr.left.getReference();
    Node right = curr.right.getReference();

    if (left != null && right != null) {
        return true;
    }

    Node replacement = left != null ? left : right;

    if (parent == null) {
        return root.compareAndSet(curr, replacement, false, false) || root.getReference() != curr;
    }

    if (parent.isMarked()) {
        return false;
    }

    AtomicMarkableReference<Node> link = curr.key < parent.key ? parent.left : parent.right;
    return link.compareAndSet(curr, replacement, false, false) || link.getReference() != curr;
}


}
