# hwbench — Code Review & Notes

## Overall

Solid, well-written C. Worth keeping. The benchmarks are technically correct in ways
that matter — this isn't throwaway code or a naive timing loop. Each test is designed
around a specific hardware behavior, and the design choices hold up to scrutiny.

## What's Good

**CPU tests**
The LCG chain for integer throughput is a real serial dependency — each iteration's
result feeds directly into the next. The compiler cannot hoist, reorder, or parallelize
it. The float FMA chain is built the same way. You're measuring what the CPU actually
does under a serial workload, not an ILP-inflated peak that never occurs in real code.

**Memory bandwidth — the non-temporal write path**
This is the most technically sophisticated part of the tool. Most simple benchmarks
measure only one write path and produce misleading results. hwbench distinguishes two:

- *Cached write (RFO path)* — the normal store. Before writing, the CPU fetches the
  existing cache line first (Read-For-Ownership). That hidden read costs real bandwidth
  and is why naive write benchmarks report numbers far below hardware spec.
- *Non-temporal write* — uses `MOVNTDQ` (SSE2) to bypass the cache entirely. No RFO,
  no eviction traffic. Data streams directly to RAM. This is the true write bandwidth
  of the hardware.

The gap between these two numbers is the RFO tax. Most tools don't show you that gap.
This one does, and it's the right way to think about write performance.

**Cache latency**
The random pointer-chase shuffle defeats the hardware prefetcher — the CPU mechanism
that tries to predict what memory you'll access next. Without defeating it, you'd be
measuring the prefetcher, not the cache. The dependent load chain (`idx = arr[idx]`)
ensures each access must complete before the next address is known. Serial, honest,
and correct.

## Minor Weaknesses

- Single-threaded only — doesn't measure multi-core aggregate bandwidth or parallel
  throughput. A future version could add a threaded memory bandwidth test.
- The Fisher-Yates shuffle uses `rand()`, which is only 15-bit on some platforms and
  could produce a biased shuffle there. `rand_r()` with a better seed or a simple
  xorshift would be more portable.
- SSE2 is hardcoded, so it won't compile on ARM without modification. The existing
  binaries suggest ARM was handled separately at build time.

## Why This Had to Be C

This is the part worth understanding.

The non-temporal store test requires `MOVNTDQ` — a specific x86 instruction that writes
directly to memory, bypassing the cache. C exposes this through `<emmintrin.h>` and
`_mm_stream_si128()`. You call it, it happens. The compiler does what you said.

Go and Rust do not expose this. Their memory models and abstractions don't reach down
to the instruction level in the same way. You could write inline assembly in Rust, but
you'd be fighting the language to do it. In C it's a single intrinsic call and the
result is exactly what you intended.

The cache latency test has the same constraint. The dependent load chain has to stay
serial — one load, then the next, then the next, each depending on the previous result.
In a higher-level language you're negotiating with a runtime, a garbage collector, or
an optimizer that may decide your loop looks like something it can improve. In C you
write the loop, the loop runs, and you trust what comes out.

This is the category of problem C owns: when correctness requires saying exactly what
the hardware should do and having that instruction actually execute. Fifty years old
and still the only honest tool for this job.

## Goal at the Time

The goal was to build something Go or Rust couldn't do quite as well — and that goal
was met. The non-temporal store path alone puts hwbench above most hobby benchmarks.
It doesn't just measure hardware, it teaches you something about how the memory
hierarchy actually works.
