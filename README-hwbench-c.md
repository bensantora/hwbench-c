# hwbench — Hardware Benchmark

**A tool that tells you how fast your computer really is.**

## What's this tool do?

Imagine you have a super-fast race car but you've never timed it on a track. You know it's fast, but how fast exactly?

**hwbench** is like a stopwatch for your computer. It tests three things:

1. **How fast is your CPU?** (the brain)
2. **How fast can your computer move data around?** (the memory)
3. **How quick is each level of memory?** (the brain's notepads)

You run it, it measures, and it tells you real numbers. No guessing.

## Why should I care?

Knowing your computer's real speed is useful because:

- **Compare computers** — Is your home computer faster than the school laptop? Now you know for sure!
- **See what matters** — The tool shows why having more RAM or a faster CPU makes things quicker
- **Learn by doing** — Compiling and running this program teaches you real Linux commands
- **Debug slow computers** — If something feels slow, the numbers tell you if it's the CPU, memory, or something else

## What do the numbers mean?

Here's the simple version:

| Test | What it means | Good number |
|------|---------------|-------------|
| CPU Integer | How fast the brain does math | 500–4000 MOPS |
| CPU Float | How fast the brain does decimal math | 300–3000 MOPS |
| Memory Write | How fast data goes to RAM | 15–50 GB/s |
| Memory Read | How fast data comes from RAM | 15–50 GB/s |
| L1 Cache | Super-fast mini-memory | 1–2 nanoseconds |
| L2 Cache | Fast mini-memory | 3–6 nanoseconds |
| L3 Cache | Slower mini-memory | 8–20 nanoseconds |
| RAM | The main memory | 60–120 nanoseconds |

**Lower is better for time. Higher is better for speed.**

## How do I use it?

First, compile it (turn the code into a program):

```bash
gcc -O2 -msse2 -o hwbench hwbench.c
```

Then run it:

```bash
./hwbench
```

Wait about 10–30 seconds. It'll print results as it runs each test.

## What's really happening? (The cool part)

### CPU Test
The tool makes your CPU do math problems one after another — not at the same time. Why? Because this way it can't "cheat" by doing multiple things at once. What you get is the real, honest speed.

### Memory Test
Your computer has two ways to write data:
- **Normal way** — It first checks if the memory spot is "owned," then writes. Like checking if a desk is taken before putting your paper down.
- **Fast way** — Just writes directly. Like dropping your paper on an empty desk without checking.

The difference between those two numbers shows how much "checking" costs.

### Cache Test
Your CPU has tiny super-fast memory spots called "cache" (L1, L2, L3) and then the bigger, slower RAM. The tool tests each one by jumping around randomly, so the CPU can't guess what's next (that would be cheating!).

The numbers show the "speed cliff" — where going from fast memory to slow memory costs a LOT of time.

## Why was this written in C?

C is a programming language that lets you control the computer very precisely. The memory tests need special instructions that only C can use directly. It's like the difference between driving an automatic car vs. driving a stick shift — C gives you total control.

## Cool facts

- Runs in about 10–30 seconds
- No administrator/sudo needed
- Works on any Linux computer
- No extra libraries to install
- The binary is tiny (about 16KB!)

---

**Have fun! Run it on different computers and compare the numbers.** That's how you learn what makes a computer fast or slow.
