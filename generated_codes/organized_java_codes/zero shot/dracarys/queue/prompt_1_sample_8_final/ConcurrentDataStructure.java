package com.example.Sets;
import com.example.utils.SetADT;

public class ConcurrentDataStructure implements QueueADT {
    private int[] data;
    private int front;
    private int rear;
    private int size;
    private static final int CAPACITY = 10;

    public ConcurrentDataStructure() {
        data = new int[CAPACITY];
        front = 0;
        rear = -1;
        size = 0;
    }

    @Override
    public void enqueue(int val) {
        if (size == CAPACITY) {
            System.out.println("Data structure is full");
            return;
        }
        rear = (rear + 1) % CAPACITY;
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
        front = (front + 1) % CAPACITY;
        size--;
        return val;
    }

    @Override
    public boolean isEmpty() {
        return size == 0;
    }
}