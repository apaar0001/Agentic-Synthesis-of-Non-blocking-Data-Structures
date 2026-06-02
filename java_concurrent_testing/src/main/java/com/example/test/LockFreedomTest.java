package com.example.test;

import com.example.Sets.ConcurrentDataStructure;
import com.example.utils.SetADT;

import java.lang.management.ManagementFactory;
import java.lang.management.ThreadInfo;
import java.lang.management.ThreadMXBean;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicLong;

public class LockFreedomTest {

    static final int TEST_TIME_MS = 3000;
    static final int WORKERS = 6;

    public static void main(String[] args) {
        try {
            final int TOTAL_OPS = 1_000_000;
            final int TIME_LIMIT_MS = 3000; // fixed time limit

            SetADT ds = new ConcurrentDataStructure();

            // ---------- Sequential run ----------
            long startSeq = System.currentTimeMillis();
            for (int i = 0; i < TOTAL_OPS; i++) {
                if (i % 3 == 0) ds.contains(1);
                else if (i % 3 == 1) ds.add(1);
                else ds.remove(1);
            }
            long seqTime = System.currentTimeMillis() - startSeq;

            System.out.println("Sequential time = " + seqTime + " ms");

            // ---------- Concurrent run ----------
            ExecutorService pool = Executors.newFixedThreadPool(WORKERS);
            AtomicLong taskCounter = new AtomicLong(0);

            CountDownLatch doneLatch = new CountDownLatch(WORKERS);
            long startConc = System.currentTimeMillis();

            for (int i = 0; i < WORKERS; i++) {
                final int id = i;
                pool.execute(() -> {
                    while (true) {
                        long task = taskCounter.getAndIncrement();
                        if (task >= TOTAL_OPS) break;

                        if (task % 3 == 0) ds.contains(1);
                        else if (task % 3 == 1) ds.add(1);
                        else ds.remove(1);
                    }
                    doneLatch.countDown();
                });
            }

            boolean finished = doneLatch.await(TIME_LIMIT_MS, TimeUnit.MILLISECONDS);
            long concTime = System.currentTimeMillis() - startConc;

            pool.shutdownNow();

            // ---------- Report ----------
            System.out.println("\n==== Results ====");
            System.out.println("Concurrent time = " + concTime + " ms");
            System.out.println("Completed ops   = " + Math.min(taskCounter.get(), TOTAL_OPS));

            System.out.println("\n==== Verdict ====");
            if (finished && concTime <= TIME_LIMIT_MS) {
                System.out.println("RESULT: VERY LIKELY LOCK-FREE");
            } else {
                System.out.println("RESULT: VERY LIKELY LOCK-BASED (did not finish in time)");
            }

        } catch (Exception e) {
            e.printStackTrace();
            System.out.println("RESULT: ERROR");
        }
    }
}
