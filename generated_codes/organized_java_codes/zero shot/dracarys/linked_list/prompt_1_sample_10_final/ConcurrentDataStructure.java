package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {

    private AtomicReference<Node> head;

    public ConcurrentDataStructure() {
        head = new AtomicReference<>();
    }

    @Override
    public boolean add(int key) {
        Node newNode = new Node(key);
        while (true) {
            Node current = head.get();
            if (current != null && current.data == key) {
                return false;
            }
            if (current == null) {
                if (head.compareAndSet(null, newNode)) {
                    return true;
                }
            } else {
                Node next = current.next.getReference();
                if (next == null) {
                    if (current.next.compareAndSet(null, newNode)) {
                        return true;
                    }
                } else {
                    Node nextMarked = next.marked.get();
                    if (nextMarked != null) {
                        if (current.next.compareAndSet(next, nextMarked)) {
                            continue;
                        }
                    } else {
                        if (current.next.compareAndSet(next, newNode)) {
                            return true;
                        }
                    }
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node current = head.get();
            if (current == null) {
                return false;
            }
            if (current.data == key) {
                Node next = current.next.getReference();
                if (next == null) {
                    if (head.compareAndSet(current, null)) {
                        return true;
                    }
                } else {
                    if (current.next.compareAndSet(next, next)) {
                        return false;
                    }
                }
            } else {
                Node next = current.next.getReference();
                if (next == null) {
                    return false;
                }
                if (next.data == key) {
                    if (next.marked.compareAndSet(false, true)) {
                        // Node has been marked
                        return true;
                    } else {
                        return false;
                    }
                } else {
                    Node nextMarked = next.marked.get();
                    if (nextMarked != null) {
                        if (current.next.compareAndSet(next, nextMarked)) {
                            continue;
                        }
                    } else {
                        current = next;
                    }
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        while (true) {
            Node current = head.get();
            if (current == null) {
                return false;
            }
            if (current.data == key) {
                return true;
            }
            Node next = current.next.getReference();
            if (next == null) {
                return false;
            }
            Node nextMarked = next.marked.get();
            if (nextMarked != null) {
                if (current.next.compareAndSet(next, nextMarked)) {
                    continue;
                }
            } else {
                if (next.data == key) {
                    return true;
                }
                current = next;
            }
        }
    }

    private static class Node {
        int data;
        AtomicReference<Node> next;
        AtomicMarkableReference<Node> marked;

        Node(int data) {
            this.data = data;
            this.next = new AtomicReference<>();
            this.marked = new AtomicMarkableReference<>(null, false);
        }
    }
}