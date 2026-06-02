package com.example.test;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;
import com.example.utils.QueueADT;

/**
 * Worker thread for Queue non-blocking (lock-freedom) tests.
 * Identical to RunOperationsQueue but coordinates via RunControllerNonBlocking
 * so the NonBlockingTest harness can control start/stop independently.
 */
public class RunOperationsQueueNonBlocking implements Runnable {

    private final QueueADT queue;
    private final int threadId;
    private final int keyRange;
    private final boolean testSanity;
    private final int[] results;
    private final AtomicInteger totalEnqueued;
    private final AtomicInteger totalDequeued;
    private final AtomicLong orderCounter;
    private final List<List<Integer>> allDequeued;

    public RunOperationsQueueNonBlocking(
            QueueADT queue, int threadId, int keyRange, int[] results,
            AtomicInteger totalEnqueued, AtomicInteger totalDequeued,
            AtomicInteger unusedParam, boolean testSanity) throws IOException {
        this(queue, threadId, keyRange, results, totalEnqueued, totalDequeued,
             unusedParam, testSanity, new AtomicLong(0), null);
    }

    public RunOperationsQueueNonBlocking(
            QueueADT queue, int threadId, int keyRange, int[] results,
            AtomicInteger totalEnqueued, AtomicInteger totalDequeued,
            AtomicInteger unusedParam, boolean testSanity,
            AtomicLong orderCounter, List<List<Integer>> allDequeued) throws IOException {
        this.queue = queue;
        this.threadId = threadId;
        this.keyRange = keyRange;
        this.results = results;
        this.totalEnqueued = totalEnqueued;
        this.totalDequeued = totalDequeued;
        this.testSanity = testSanity;
        this.orderCounter = orderCounter;
        this.allDequeued = allDequeued;
    }

    private void sanityRun() {
        while (!RunControllerNonBlocking.startFlag) ;
        int ops = 0;
        List<Integer> myDequeued = new ArrayList<>();
        while (!RunControllerNonBlocking.stopFlag) {
            int val1 = (int) orderCounter.getAndIncrement();
            queue.enqueue(val1);
            totalEnqueued.incrementAndGet();
            ops++;

            int val2 = (int) orderCounter.getAndIncrement();
            queue.enqueue(val2);
            totalEnqueued.incrementAndGet();
            ops++;

            int got = queue.dequeue();
            if (got != -1) {
                totalDequeued.incrementAndGet();
                myDequeued.add(got);
                ops++;
            }
            RunControllerNonBlocking.globalOps.addAndGet(3);
        }
        results[threadId] = ops;
        if (allDequeued != null) { allDequeued.set(threadId, myDequeued); }
    }

    private void benchMarkRun() {
        while (!RunControllerNonBlocking.startFlag) ;
        int ops = 0;
        while (!RunControllerNonBlocking.stopFlag) {
            queue.enqueue((int) (Math.random() * keyRange));
            queue.dequeue();
            ops += 2;
            RunControllerNonBlocking.globalOps.addAndGet(2);
        }
        results[threadId] = ops;
    }

    @Override
    public void run() {
        if (testSanity) sanityRun();
        else benchMarkRun();
    }
}
