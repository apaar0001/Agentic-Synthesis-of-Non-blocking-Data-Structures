package com.example.test;

import java.util.concurrent.atomic.AtomicInteger;
import com.example.utils.StackADT;

/**
 * Non-Blocking (Lock-Freedom Victim) Test for lock-free Stack implementations.
 *
 * Mechanism:
 * The ConcurrentDataStructure is pre-instrumented with victim-sleep logic
 * anchored on the comment "// Pop victim point" inside pop(). One thread
 * stalls for 10 s inside pop(); the remaining threads must still make
 * measurable progress — proving lock-freedom.
 *
 * Prints "Sanity Test Passed" or "Sanity Test Failed" (parsed by Python
 * runner).
 */
public class StackNonBlockingTest {

    private static int numberOfThreads = 5;
    private static int maxRunningTime = 3;
    private static int keyRange = 100;
    private static StackADT stack;

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

            try {
                stack = (StackADT) Class.forName("com.example.Sets.ConcurrentDataStructure").getDeclaredConstructor()
                        .newInstance();
            } catch (Exception e) {
                System.out.println("Sanity Test Failed");
                System.err.println(
                        "[StackNonBlockingTest] Could not instantiate ConcurrentDataStructure: " + e.getMessage());
                return;
            }

            AtomicInteger totalPushed = new AtomicInteger(0);
            AtomicInteger totalPopped = new AtomicInteger(0);
            AtomicInteger invalidPops = new AtomicInteger(0);
            int[] results = new int[numberOfThreads];

            Thread[] threads = new Thread[numberOfThreads];
            for (int i = 0; i < numberOfThreads; i++) {
                threads[i] = new Thread(new RunOperationsStackNonBlocking(
                        stack, i, keyRange, results,
                        totalPushed, totalPopped, invalidPops, true));
            }

            // Pre-populate
            for (int i = 0; i < keyRange / 2; i++) {
                stack.push(i);
                totalPushed.incrementAndGet();
            }

            RunControllerNonBlocking.startFlag = RunControllerNonBlocking.stopFlag = false;
            RunControllerNonBlocking.globalOps.set(0);

            for (Thread t : threads)
                t.start();
            RunControllerNonBlocking.startFlag = true;

            Thread.sleep(1000);
            long opsBefore = RunControllerNonBlocking.globalOps.get();
            Thread.sleep((maxRunningTime - 1) * 1000L);
            long opsAfter = RunControllerNonBlocking.globalOps.get();

            RunControllerNonBlocking.stopFlag = true;
            for (Thread t : threads)
                t.join();

            boolean passed = (opsAfter - opsBefore) >= (numberOfThreads - 1) * 5;

            if (passed) {
                System.out.println("Sanity Test Passed");
            } else {
                System.out.printf("[StackNonBlockingTest] Progress check failed: opsBefore=%d opsAfter=%d%n", opsBefore,
                        opsAfter);
                System.out.println("Sanity Test Failed");
            }
        } catch (Throwable t) {
            System.err.println("[StackNonBlockingTest] FATAL: " + t.getMessage());
            t.printStackTrace(System.err);
            System.out.println("Sanity Test Failed");
        }
    }
}
