package com.example.Sets;
import com.example.utils.SetADT;

import com.example.utils.QueueADT;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements QueueADT {
    private static class Node {
        final int val;
        final AtomicReference<Node> next;
        
        Node(int val) {
            this.val = val;
            this.next = new AtomicReference<>(null);
        }
    }
    
    private final AtomicReference<Node> head;
    private final AtomicReference<Node> tail;
    
    public ConcurrentDataStructure() {
        Node sentinel = new Node(-1);
        head = new AtomicReference<>(sentinel);
        tail = new AtomicReference<>(sentinel);
    }
    
    @Override
    public void enqueue(int val) {
        Node newNode = new Node(val);
        while (true) {
            Node currentTail = tail.get();
            Node tailNext = currentTail.next.get();
            
            if (currentTail == tail.get()) {
                if (tailNext != null) {
                    tail.compareAndSet(currentTail, tailNext);
                } else {
                    if (currentTail.next.compareAndSet(null, newNode)) {
                        tail.compareAndSet(currentTail, newNode);
                        return;
                    }
                }
            }
        }
    }
    
    @Override
    public int dequeue() {
        while (true) {
            Node currentHead = head.get();
            Node currentTail = tail.get();
            Node headNext = currentHead.next.get();
            
            if (currentHead == head.get()) {
                if (currentHead == currentTail) {
                    if (headNext == null) {
                        return -1;
                    }
                    tail.compareAndSet(currentTail, headNext);
                } else {
                    if (head.compareAndSet(currentHead, headNext)) {
                        // Dequeue victim point
                        return headNext.val;
                    }
                }
            }
        }
    }
    
    @Override
    public boolean isEmpty() {
        return head.get().next.get() == null;
    }
}