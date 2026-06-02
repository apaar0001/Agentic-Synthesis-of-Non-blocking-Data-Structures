package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static class Element {
        final int value;
        Element successor;
        final AtomicMarkableReference<Element> mark;

        Element(int value) {
            this.value = value;
            this.successor = null;
            this.mark = new AtomicMarkableReference<>(this, false);
        }
    }

    private final Element head;

    public ConcurrentDataStructure() {
        head = new Element(Integer.MIN_VALUE);
        head.successor = new Element(Integer.MAX_VALUE);
    }

    private boolean find(Element start, int key, Element[] preds, Element[] currs) {
        Element pred = start;
        Element curr = pred.successor;
        
        while (true) {
            if (curr == null) {
                return false;
            }
            
            boolean[] marked = new boolean[1];
            Element succ = curr.mark.get(marked);
            
            while (marked[0]) {
                if (!pred.mark.compareAndSet(curr, succ, false, false)) {
                    return false;
                }
                curr = succ;
                if (curr == null) {
                    return false;
                }
                succ = curr.mark.get(marked);
            }
            
            if (curr.value >= key) {
                preds[0] = pred;
                currs[0] = curr;
                return curr.value == key;
            }
            
            pred = curr;
            curr = succ;
        }
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Element[] preds = new Element[1];
            Element[] currs = new Element[1];
            
            if (find(head, key, preds, currs)) {
                return false;
            }
            
            Element pred = preds[0];
            Element curr = currs[0];
            Element newNode = new Element(key);
            newNode.successor = curr;
            
            if (pred.mark.compareAndSet(curr, newNode, false, false)) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Element[] preds = new Element[1];
            Element[] currs = new Element[1];
            
            if (!find(head, key, preds, currs)) {
                return false;
            }
            
            Element pred = preds[0];
            Element curr = currs[0];
            Element succ = curr.successor;
            
            if (!curr.mark.compareAndSet(curr, curr, false, true)) {
                continue;
            }
            // Node has been marked
            pred.mark.compareAndSet(curr, succ, false, false);
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        Element curr = head.successor;
        
        while (curr != null && curr.value < key) {
            boolean[] marked = new boolean[1];
            curr.mark.get(marked);
            if (!marked[0] && curr.value == key) {
                return true;
            }
            curr = curr.successor;
        }
        
        return curr != null && curr.value == key && !curr.mark.isMarked();
    }
}