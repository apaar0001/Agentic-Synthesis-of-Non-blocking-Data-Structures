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
        final Node parent;
        final Node curr;
        final AtomicMarkableReference<Node> currRef;

        Window(Node parent, Node curr, AtomicMarkableReference<Node> currRef) {
            this.parent = parent;
            this.curr = curr;
            this.currRef = currRef;
        }
    }

    private final Node root;

    public ConcurrentDataStructure() {
        this.root = new Node(Integer.MAX_VALUE);
    }

    private Window find(int key) {
        while (true) {
            Node parent = root;
            AtomicMarkableReference<Node> currRef = root.left;
            Node curr = currRef.getReference();

            if (curr == null) {
                return new Window(parent, null, currRef);
            }

            boolean retry = false;
            while (curr != null) {
                boolean[] leftMarked = new boolean[1];
                boolean[] rightMarked = new boolean[1];
                Node leftNode = curr.left.get(leftMarked);
                Node rightNode = curr.right.get(rightMarked);

                if (leftMarked[0] || rightMarked[0]) {
                    boolean[] currMarked = new boolean[1];
                    Node nextParentChild = currRef.get(currMarked);
                    if (currMarked[0] || nextParentChild != curr) {
                        retry = true;
                        break;
                    }

                    Node splice = null;
                    if (leftNode == null && rightNode == null) {
                        splice = null;
                    } else if (leftNode != null && rightNode == null) {
                        splice = leftNode;
                    } else if (leftNode == null) {
                        splice = rightNode;
                    }

                    if (leftNode == null || rightNode == null) {
                        if (currRef.compareAndSet(curr, splice, false, false)) {
                            curr = splice;
                        } else {
                            retry = true;
                            break;
                        }
                    } else {
                        Window successorWin = findSuccessor(curr);
                        if (successorWin == null) {
                            retry = true;
                            break;
                        }
                        Node succ = successorWin.curr;
                        Node succParent = successorWin.parent;
                        boolean[] succMarked = new boolean[1];
                        Node succRight = succ.right.get(succMarked);

                        if (succParent == curr) {
                            if (currRef.compareAndSet(curr, succ, false, false)) {
                                if (succ.left.compareAndSet(null, leftNode, false, false)) {
                                    curr = succ;
                                } else {
                                    retry = true;
                                    break;
                                }
                            } else {
                                retry = true;
                                break;
                            }
                        } else {
                            if (succParent.left.compareAndSet(succ, succRight, false, false)) {
                                if (currRef.compareAndSet(curr, succ, false, false)) {
                                    if (succ.left.compareAndSet(null, leftNode, false, false) &&
                                        succ.right.compareAndSet(null, rightNode, false, false)) {
                                        curr = succ;
                                    } else {
                                        retry = true;
                                        break;
                                    }
                                } else {
                                    retry = true;
                                    break;
                                }
                            } else {
                                retry = true;
                                break;
                            }
                        }
                    }
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

            if (!retry) {
                return new Window(parent, null, currRef);
            }
        }
    }

    private Window findSuccessor(Node node) {
        Node parent = node;
        AtomicMarkableReference<Node> currRef = node.right;
        Node curr = currRef.getReference();
        if (curr == null) return null;

        while (true) {
            boolean[] marked = new boolean[1];
            Node leftNode = curr.left.get(marked);
            if (marked[0]) return null;

            if (leftNode == null) {
                return new Window(parent, curr, currRef);
            }
            parent = curr;
            currRef = curr.left;
            curr = leftNode;
        }
    }

    @Override
    public boolean add(int key) {
        if (key == Integer.MAX_VALUE) {
            return false;
        }
        Node newNode = new Node(key);
        while (true) {
            Window win = find(key);
            if (win.curr != null) {
                return false;
            }
            if (win.currRef.compareAndSet(null, newNode, false, false)) {
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
            Window win = find(key);
            if (win.curr == null) {
                return false;
            }
            Node curr = win.curr;

            boolean[] leftMarked = new boolean[1];
            Node leftNode = curr.left.get(leftMarked);
            if (leftMarked[0]) continue;

            if (!curr.left.compareAndSet(leftNode, leftNode, false, true)) {
                continue;
            }
            // Node has been marked

            boolean[] rightMarked = new boolean[1];
            Node rightNode = curr.right.get(rightMarked);
            if (!rightMarked[0]) {
                curr.right.compareAndSet(rightNode, rightNode, false, true);
            }

            find(key);
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        if (key == Integer.MAX_VALUE) {
            return false;
        }
        Node curr = root.left.getReference();
        while (curr != null) {
            if (key == curr.key) {
                boolean[] leftMarked = new boolean[1];
                boolean[] rightMarked = new boolean[1];
                curr.left.get(leftMarked);
                curr.right.get(rightMarked);
                return !leftMarked[0] && !rightMarked[0];
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