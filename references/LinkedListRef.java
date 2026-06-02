package com.example.Sets;

import com.example.utils.SetADT;

/**
 * Reference sequential implementation of a singly linked list (sorted order).
 * Used as CodeBLEU reference baseline for zip_shot semantic scoring.
 */
public class LinkedListRef implements SetADT {

    private static class Node {
        int key;
        Node next;

        Node(int key) {
            this.key = key;
            this.next = null;
        }
    }

    private final Node head; // sentinel head (key = Integer.MIN_VALUE)
    private final Node tail; // sentinel tail (key = Integer.MAX_VALUE)

    public LinkedListRef() {
        head = new Node(Integer.MIN_VALUE);
        tail = new Node(Integer.MAX_VALUE);
        head.next = tail;
    }

    @Override
    public boolean add(int key) {
        Node pred = head;
        Node curr = head.next;
        while (curr.key < key) {
            pred = curr;
            curr = curr.next;
        }
        if (curr.key == key) {
            return false;
        }
        Node node = new Node(key);
        node.next = curr;
        pred.next = node;
        return true;
    }

    @Override
    public boolean remove(int key) {
        Node pred = head;
        Node curr = head.next;
        while (curr.key < key) {
            pred = curr;
            curr = curr.next;
        }
        if (curr.key != key) {
            return false;
        }
        pred.next = curr.next;
        return true;
    }

    @Override
    public boolean contains(int key) {
        Node curr = head.next;
        while (curr.key < key) {
            curr = curr.next;
        }
        return curr.key == key;
    }
}
