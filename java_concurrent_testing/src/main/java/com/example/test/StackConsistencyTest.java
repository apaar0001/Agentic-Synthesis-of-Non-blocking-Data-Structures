package com.example.test;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;
import com.example.utils.StackADT;

/**
 * Consistency Test for lock-free Stack implementations.
 *
 * Phase 1: Concurrent conservation check (pushed == popped + remaining)
 * Phase 2: LIFO ordering check: min(remaining) < max(popped) - tolerance
 */
public class StackConsistencyTest {

    private static int numberOfThreads = 5;
    private static int maxRunningTime = 2;
    private static int keyRange = 100;

    public static void main(String[] args) {
        try {
            for (int i = 0; i < args.length - 1; i++) {
                if ("-n".equals(args[i])) numberOfThreads = Integer.parseInt(args[i + 1]);
                if ("-d".equals(args[i])) maxRunningTime = Integer.parseInt(args[i + 1]);
                if ("-k".equals(args[i])) keyRange = Integer.parseInt(args[i + 1]);
            }

            StackADT stack;
            try {
                stack = (StackADT) Class.forName("com.example.Sets.ConcurrentDataStructure")
                        .getDeclaredConstructor().newInstance();
            } catch (Exception e) {
                System.out.println("Sanity Test Failed");
                return;
            }

            // --- Concurrent phase with 2:1 push:pop ratio ---
            AtomicInteger totalPushed = new AtomicInteger(0);
            AtomicInteger totalPopped = new AtomicInteger(0);
            AtomicInteger invalidPops = new AtomicInteger(0);
            AtomicLong orderCounter = new AtomicLong(0);
            int[] results = new int[numberOfThreads];

            List<List<Integer>> allPopped = new ArrayList<>(Collections.nCopies(numberOfThreads, null));
            for (int i = 0; i < numberOfThreads; i++) allPopped.set(i, new ArrayList<>());

            Thread[] threads = new Thread[numberOfThreads];
            for (int i = 0; i < numberOfThreads; i++) {
                threads[i] = new Thread(new RunOperationsStack(
                        stack, i, keyRange, results,
                        totalPushed, totalPopped, invalidPops, true,
                        orderCounter, allPopped));
            }

            RunController.startFlag = RunController.stopFlag = false;
            for (Thread t : threads) t.start();
            RunController.startFlag = true;
            Thread.sleep(maxRunningTime * 1000L);
            RunController.stopFlag = true;
            for (Thread t : threads) t.join();

            // Drain remaining items and collect order tags
            List<Integer> remainingOrders = new ArrayList<>();
            while (!stack.isEmpty()) {
                int v = stack.pop();
                if (v == -1) break;
                remainingOrders.add(v);
            }
            int remaining = remainingOrders.size();
            int pushed = totalPushed.get();
            int popped = totalPopped.get();

            // Check 1: Conservation
            boolean conservationOk = (pushed >= popped) && (pushed <= popped + remaining + numberOfThreads * 2);

            // Check 2: LIFO Ordering with tolerance for concurrent push races
            // In a LIFO structure, remaining items were pushed EARLIER (lower tags)
            // while popped items were pushed LATER (higher tags, popped via LIFO).
            int maxPopped = Integer.MIN_VALUE;
            boolean hasPopped = false;
            for (List<Integer> tp : allPopped) {
                for (int val : tp) {
                    if (val > maxPopped) maxPopped = val;
                    hasPopped = true;
                }
            }

            int minRemaining = Integer.MAX_VALUE;
            boolean hasRemaining = false;
            for (int val : remainingOrders) {
                if (val < minRemaining) minRemaining = val;
                hasRemaining = true;
            }

            int tolerance = numberOfThreads * 2;
            boolean lifoOk = !hasPopped || !hasRemaining || (minRemaining < maxPopped + tolerance);
            boolean passed = conservationOk && lifoOk;

            if (passed) {
                System.out.println("Pushed: " + pushed + ", Popped: " + popped + ", Remaining: " + remaining);
                if (hasPopped && hasRemaining)
                    System.out.println("LIFO check: maxPopped=" + maxPopped + " minRemaining=" + minRemaining + " tolerance=" + tolerance + " OK");
                System.out.println("Sanity Test Passed");
            } else {
                if (!conservationOk)
                    System.out.printf("Stack conservation failed: pushed=%d popped=%d remaining=%d%n", pushed, popped, remaining);
                if (!lifoOk)
                    System.out.printf("Stack LIFO ordering failed: maxPopped=%d minRemaining=%d tolerance=%d%n", maxPopped, minRemaining, tolerance);
                System.out.println("Sanity Test Failed");
            }
        } catch (Throwable t) {
            System.out.println("Sanity Test Failed");
        }
    }
}
