package com.example.test;

import java.util.concurrent.atomic.AtomicInteger;
import com.example.utils.QueueADT;

/**
 * Non-Blocking (Lock-Freedom Victim) Test for lock-free Queue implementations.
 *
 * Mechanism:
 * The ConcurrentDataStructure is pre-instrumented by the Python runner with
 * victim-sleep logic anchored on the comment "// Dequeue victim point" inside
 * dequeue(). One thread is elected victim and stalls for 10 s inside dequeue();
 * the remaining threads must still make progress — proving lock-freedom.
 *
 * Progress is measured by RunControllerNonBlocking.globalOps. If ops increase
 * while the victim is stalled, lock-freedom is proven.
 *
 * Prints "Sanity Test Passed" or "Sanity Test Failed" (parsed by Python
 * runner).
 */
public class QueueNonBlockingTest {

    private static int numberOfThreads = 5;
    private static int maxRunningTime = 3;
    private static int keyRange = 100;
    private static QueueADT queue;

    public static void main(String[] args) {
        try {
            for (int i = 0; i < args.length - 1; i++) {
                if ("-n".equals(args[i]))
                    numberOfThreads = Integer.parseInt(args[i + 1]);
                if ("-d".equals(args[i]))
                    maxRunningTime = Integer.parseInt(args[i + 1]);
                if ("-k".equals(args[i]))
                    keyRange = Integer.parseInt(args[i + 1]);
            }

            // Instantiate the victim-instrumented ConcurrentDataStructure
            try {
                queue = (QueueADT) Class.forName("com.example.Sets.ConcurrentDataStructure").getDeclaredConstructor()
                        .newInstance();
            } catch (Exception e) {
                System.out.println("Sanity Test Failed");
                System.err.println(
                        "[QueueNonBlockingTest] Could not instantiate ConcurrentDataStructure: " + e.getMessage());
                return;
            }

            AtomicInteger totalEnqueued = new AtomicInteger(0);
            AtomicInteger totalDequeued = new AtomicInteger(0);
            AtomicInteger invalidDequeues = new AtomicInteger(0);
            int[] results = new int[numberOfThreads];

            Thread[] threads = new Thread[numberOfThreads];
            for (int i = 0; i < numberOfThreads; i++) {
                threads[i] = new Thread(new RunOperationsQueueNonBlocking(
                        queue, i, keyRange, results,
                        totalEnqueued, totalDequeued, invalidDequeues, true));
            }

            // Pre-populate so dequeue() has something to consume
            for (int i = 0; i < keyRange / 2; i++) {
                queue.enqueue(i);
                totalEnqueued.incrementAndGet();
            }

            RunControllerNonBlocking.startFlag = RunControllerNonBlocking.stopFlag = false;
            RunControllerNonBlocking.globalOps.set(0);

            for (Thread t : threads)
                t.start();
            RunControllerNonBlocking.startFlag = true;

            // Sample globalOps at t=1s and t=duration to confirm progress
            Thread.sleep(1000);
            long opsBefore = RunControllerNonBlocking.globalOps.get();
            Thread.sleep((maxRunningTime - 1) * 1000L);
            long opsAfter = RunControllerNonBlocking.globalOps.get();

            RunControllerNonBlocking.stopFlag = true;
            for (Thread t : threads)
                t.join();

            // Lock-freedom: other threads made progress while victim stalled
            boolean passed = (opsAfter - opsBefore) >= (numberOfThreads - 1) * 5;

            if (passed) {
                System.out.println("Sanity Test Passed");
            } else {
                System.out.printf("[QueueNonBlockingTest] Progress check failed: opsBefore=%d opsAfter=%d%n", opsBefore,
                        opsAfter);
                System.out.println("Sanity Test Failed");
            }
        } catch (Throwable t) {
            System.err.println("[QueueNonBlockingTest] FATAL: " + t.getMessage());
            t.printStackTrace(System.err);
            System.out.println("Sanity Test Failed");
        }
    }
}
