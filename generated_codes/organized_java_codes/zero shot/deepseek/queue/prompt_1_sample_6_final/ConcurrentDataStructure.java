package com.example.Sets;
import com.example.utils.SetADT;

public class ConcurrentDataStructure implements QueueADT {
    private static class Node {
        int value;
        Node next;
        
        Node(int val) {
            this.value = val;
            this.next = null;
        }
    }
    
    private Node front;
    private Node rear;
    private int count;
    
    public ConcurrentDataStructure() {
        front = null;
        rear = null;
        count = 0;
    }
    
    @Override
    public void enqueue(int val) {
        Node newNode = new Node(val);
        
        if (rear == null) {
            front = newNode;
            rear = newNode;
        } else {
            rear.next = newNode;
            rear = newNode;
        }
        count++;
    }
    
    @Override
    public int dequeue() {
        if (front == null) {
            return -1;
        }
        
        int result = front.value;
        front = front.next;
        
        if (front == null) {
            rear = null;
        }
        
        count--;
        return result;
    }
    
    @Override
    public boolean isEmpty() {
        return front == null;
    }
}