package com.example.test;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;
import com.example.utils.StackADT;

/**
 * Worker thread for Stack consistency and non-blocking tests.
 * Each thread repeatedly pushes and pops values, tracking counts.
 *
 * Shared counters: totalPushed == totalPopped + (items remaining in stack)
 *
 * Order tags: each pushed value is a monotonically increasing order tag
 * from a shared AtomicLong counter. Popped values are collected per-thread
 * so the harness can verify LIFO ordering after the concurrent phase.
 */
public class RunOperationsStack implements Runnable {

    private final StackADT stack;
    private final int threadId;
    private final int keyRange;
    private final boolean testSanity;
    private final int[] results;
    private final AtomicInteger totalPushed;
    private final AtomicInteger totalPopped;
    private final AtomicLong orderCounter;
    private final List<List<Integer>> allPopped;

    /** Backward-compatible constructor (no ordering check). */
    public RunOperationsStack(
            StackADT stack, int threadId, int keyRange, int[] results,
            AtomicInteger totalPushed, AtomicInteger totalPopped,
            AtomicInteger unusedParam, boolean testSanity) throws IOException {
        this(stack, threadId, keyRange, results, totalPushed, totalPopped,
             unusedParam, testSanity, new AtomicLong(0), null);
    }

    public RunOperationsStack(
            StackADT stack, int threadId, int keyRange, int[] results,
            AtomicInteger totalPushed, AtomicInteger totalPopped,
            AtomicInteger unusedParam, boolean testSanity,
            AtomicLong orderCounter, List<List<Integer>> allPopped) throws IOException {
        this.stack = stack;
        this.threadId = threadId;
        this.keyRange = keyRange;
        this.results = results;
        this.totalPushed = totalPushed;
        this.totalPopped = totalPopped;
        this.testSanity = testSanity;
        this.orderCounter = orderCounter;
        this.allPopped = allPopped;
    }

    private void sanityRun() {
        while (!RunController.startFlag)
            ;

        int ops = 0;
        List<Integer> myPopped = new ArrayList<>();

        while (!RunController.stopFlag) {
            int val1 = (int) orderCounter.getAndIncrement();
            stack.push(val1);
            totalPushed.incrementAndGet();
            ops++;

            int val2 = (int) orderCounter.getAndIncrement();
            stack.push(val2);
            totalPushed.incrementAndGet();
            ops++;

            int got = stack.pop();
            if (got != -1) {
                totalPopped.incrementAndGet();
                myPopped.add(got);
                ops++;
            }
            RunController.globalOps.addAndGet(3);
        }
        results[threadId] = ops;
        if (allPopped != null) {
            allPopped.set(threadId, myPopped);
        }
    }

    private void benchMarkRun() {
        while (!RunController.startFlag)
            ;

        int ops = 0;
        while (!RunController.stopFlag) {
            stack.push((int) (Math.random() * keyRange));
            stack.pop();
            ops += 2;
            RunController.globalOps.addAndGet(2);
        }
        results[threadId] = ops;
    }

    @Override
    public void run() {
        if (testSanity) sanityRun();
        else benchMarkRun();
    }
}
