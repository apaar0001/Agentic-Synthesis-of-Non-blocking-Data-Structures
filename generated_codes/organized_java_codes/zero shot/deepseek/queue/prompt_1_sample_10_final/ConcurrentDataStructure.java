package com.example.Sets;
import com.example.utils.SetADT;

public class ConcurrentDataStructure implements QueueADT {
    private static class Node {
        int value;
        Node next;
        
        Node(int val) {
            this.value = val;
        }
    }
    
    private Node head;
    private Node tail;
    private int count;
    
    public ConcurrentDataStructure() {
        head = null;
        tail = null;
        count = 0;
    }
    
    @Override
    public void enqueue(int val) {
        Node newNode = new Node(val);
        
        if (tail == null) {
            head = newNode;
            tail = newNode;
        } else {
            tail.next = newNode;
            tail = newNode;
        }
        
        count++;
    }
    
    @Override
    public int dequeue() {
        if (head == null) {
            return -1;
        }
        
        int result = head.value;
        head = head.next;
        
        if (head == null) {
            tail = null;
        }
        
        count--;
        return result;
    }
    
    @Override
    public boolean isEmpty() {
        return head == null;
    }
}