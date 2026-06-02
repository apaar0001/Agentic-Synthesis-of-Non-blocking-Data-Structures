package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static class Element {
        final int value;
        final AtomicMarkableReference<Element> successor;

        Element(int value) {
            this.value = value;
            this.successor = new AtomicMarkableReference<>(null, false);
        }
    }

    private final AtomicMarkableReference<Element> first;

    public ConcurrentDataStructure() {
        first = new AtomicMarkableReference<>(null, false);
    }

    private void helpUnlink(Element prev, Element current) {
        Element succ = current.successor.getReference();
        if (prev == null) {
            first.compareAndSet(current, succ, false, false);
        } else {
            prev.successor.compareAndSet(current, succ, false, false);
        }
    }

    @Override
    public boolean add(int key) {
        Element newElement = new Element(key);
        while (true) {
            Element current = first.getReference();
            Element prev = null;
            Element next = null;
            boolean[] markHolder = new boolean[1];

            while (current != null) {
                next = current.successor.get(markHolder);
                if (markHolder[0]) {
                    helpUnlink(prev, current);
                    current = first.getReference();
                    prev = null;
                    continue;
                }
                if (current.value == key) {
                    return false;
                }
                prev = current;
                current = next;
            }

            if (prev == null) {
                if (first.compareAndSet(null, newElement, false, false)) {
                    return true;
                }
            } else {
                if (prev.successor.compareAndSet(null, newElement, false, false)) {
                    return true;
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Element current = first.getReference();
            Element prev = null;
            Element next = null;
            boolean[] markHolder = new boolean[1];

            while (current != null) {
                next = current.successor.get(markHolder);
                if (markHolder[0]) {
                    helpUnlink(prev, current);
                    current = first.getReference();
                    prev = null;
                    continue;
                }
                if (current.value == key) {
                    if (current.successor.compareAndSet(next, next, false, true)) {
                        // Node has been marked
                        helpUnlink(prev, current);
                        return true;
                    } else {
                        break;
                    }
                }
                prev = current;
                current = next;
            }
            return false;
        }
    }

    @Override
    public boolean contains(int key) {
        Element current = first.getReference();
        boolean[] markHolder = new boolean[1];

        while (current != null) {
            Element next = current.successor.get(markHolder);
            if (!markHolder[0] && current.value == key) {
                return true;
            }
            current = next;
        }
        return false;
    }
}