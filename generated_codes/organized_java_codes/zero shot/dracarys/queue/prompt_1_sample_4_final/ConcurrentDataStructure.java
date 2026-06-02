package com.example.Sets;
import com.example.utils.SetADT;

public class ConcurrentDataStructure implements QueueADT {
    private int[] data;
    private int front;
    private int rear;
    private int size;
    private int capacity;

    public ConcurrentDataStructure() {
        capacity = 10;
        data = new int[capacity];
        front = 0;
        rear = -1;
        size = 0;
    }

    @Override
    public void enqueue(int val) {
        if (size == capacity) {
            System.out.println("Data structure is full");
            return;
        }
        rear = (rear + 1) % capacity;
        data[rear] = val;
        size++;
        System.out.println("Element added to data structure");
    }

    @Override
    public int dequeue() {
        if (size == 0) {
            System.out.println("Data structure is empty");
            return -1;
        }
        int val = data[front];
        front = (front + 1) % capacity;
        size--;
        return val;
    }

    @Override
    public boolean isEmpty() {
        return size == 0;
    }
}