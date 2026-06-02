package com.example.Sets;
import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements com.example.utils.SetADT {
    private static class Node {
        int key;
        AtomicMarkableReference<Node> next;
        Node(int key) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, false);
        }
    }

    private final AtomicReference<Node> head = new AtomicReference<>(null);

    public ConcurrentDataStructure() {
        Node min = new Node(Integer.MIN_VALUE);
        Node max = new Node(Integer.MAX_VALUE);
        min.next = new AtomicMarkableReference<>(max, false);
        head.set(min);
    }

    private Node[] find(int key) {
        Node pred = head.get();
        Node curr;
        boolean[] marked = {false};
        while (true) {
            curr = pred.next.getReference();
            boolean currMarked = pred.next.get(marked);
            while (currMarked) {
                Node succ = curr.next.getReference();
                if (pred.next.compareAndSet(curr, succ, true, false)) {
                    curr = pred.next.getReference();
                    if (curr == null) break;
                    currMarked = pred.next.get(marked);
                } else {
                    break;
                }
            }
            if (curr == null) return new Node[]{pred, null};
            Node succ = curr.next.getReference();
            boolean succMarked = curr.next.get(marked);
            if (succMarked) {
                if (pred.next.compareAndSet(curr, succ, false, false)) {
                    continue;
                } else {
                    continue;
                }
            }
            if (curr.key >= key) {
                return new Node[]{pred, curr};
            }
            pred = curr;
        }
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node[] predCurr = find(key);
            Node pred = predCurr[0];
            Node curr = predCurr[1];
            if (curr != null && curr.key == key) {
                return false;
            }
            Node node = new Node(key);
            node.next = new AtomicMarkableReference<>(curr, false);
            if (pred.next.compareAndSet(curr, node, false, false)) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node[] predCurr = find(key);
            Node pred = predCurr[0];
            Node curr = predCurr[1];
            if (curr == null || curr.key != key) {
                return false;
            }
            Node succ = curr.next.getReference();
            if (curr.next.attemptMark(succ, true)) {
                // Node has been marked
                pred.next.compareAndSet(curr, succ, true, false);
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node curr = head.get();
        boolean[] marked = {false};
        while (true) {
            Node next = curr.next.getReference();
            boolean currMarked = curr.next.get(marked);
            if (currMarked) {
                curr = next;
                continue;
            }
            if (curr.key >= key) {
                break;
            }
            curr = next;
        }
        return curr.key == key;
    }
}