# hwbench — Hardware Benchmark

A command-line tool that measures what your Linux machine can actually do.
No installation, no dependencies, no root required. Drop the binary, run it, get answers.

## What It Measures

**CPU Throughput**
How fast the processor executes a serial chain of arithmetic — integer and floating point.
Because each operation depends on the result of the previous one, the CPU can't cheat by
running multiple operations at once. The number reflects real throughput per core.

**Memory Bandwidth**
How fast data moves between the processor and RAM. Two write paths are measured:

- *Cached write* — the normal path. Before writing a memory location, the CPU fetches
  the existing data first (a "Read-For-Ownership"). This hidden read costs bandwidth.
- *Non-temporal write* — the fast path. Bypasses the cache entirely, streams directly
  to RAM. No hidden read. This is the true write bandwidth of the hardware.
- *Sequential read* — how fast the CPU can read a large block of data from RAM.

**Cache Latency**
How long it takes to fetch a single piece of data from each level of the memory hierarchy.
Modern CPUs have small, fast caches close to the processor (L1, L2, L3) and slower main
RAM further away. The latency jumps between levels tell you exactly where the speed cliff is.

The test uses a random access pattern specifically designed to defeat the CPU's prefetcher —
the hardware mechanism that tries to predict what data you'll need next. Without that
defeat, you'd be measuring the prefetcher, not the cache. A short warmup pass runs before
timing begins to ensure the benchmark itself is not skewing the first result.

## Building

```bash
gcc -O2 -msse2 -o hwbench hwbench.c
```

Requires: gcc, standard C library, Linux. Nothing else.

## Running

```bash
./hwbench
```

Takes 10–30 seconds depending on hardware. Results print as each test completes —
if it appears to hang at the start, the CPU integer test is running (it's the longest).

## Reading the Output

```
CPU
  Integer throughput    :   2400 MOPS
  Float throughput      :   1200 MOPS
```
MOPS = millions of operations per second. Higher is better.

```
Memory Bandwidth
  Write (cached/RFO)    :   20.1 GB/s
  Write (non-temporal)  :   23.2 GB/s
  Sequential read       :   16.2 GB/s
```
GB/s = gigabytes per second. The gap between cached and non-temporal write is
the cost of the hidden Read-For-Ownership on every cache-missing store.

```
Cache Latency
  16 KB   (L1)          :    1.2 ns
  256 KB  (L2)          :    2.8 ns
  4 MB    (L3)          :   10.7 ns
  256 MB  (RAM)         :   98.6 ns
```
ns = nanoseconds per memory access. Lower is better. The jump from L3 to RAM
is where most performance-sensitive software tries not to go.

## What Normal Looks Like

On a modern desktop or laptop CPU:

| Measurement            | Typical range       |
|------------------------|---------------------|
| Integer throughput     | 500 – 4000 MOPS     |
| Float throughput       | 300 – 3000 MOPS     |
| Write (non-temporal)   | 15 – 50 GB/s        |
| Sequential read        | 15 – 50 GB/s        |
| L1 latency             | 1 – 2 ns            |
| L2 latency             | 3 – 6 ns            |
| L3 latency             | 8 – 20 ns           |
| RAM latency            | 60 – 120 ns         |

Numbers outside these ranges aren't necessarily wrong — older hardware, single-channel
memory, or a thermally throttled laptop will land lower. The value is in running it
on multiple machines and comparing.

## Why C

The write bandwidth test requires non-temporal store instructions (MOVNTDQ) to bypass
the cache and measure true memory throughput. The cache latency test requires a serial
dependent-load chain that the CPU cannot speculatively execute around. Both techniques
demand direct control over what the compiler and hardware actually do. C provides that
control. A higher-level language cannot make the same guarantees.
