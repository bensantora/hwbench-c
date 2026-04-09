# hwbench-c
Linux hardware benchmark tool written in C — measures CPU, memory bandwidth, and cache latency with low-level accuracy.

## Features

- **CPU benchmarks**: 32-bit and 64-bit integer, single and double precision floating-point
- **Memory bandwidth**: cached writes (RFO path), non-temporal writes (cache bypass), sequential reads
- **Cache latency**: L1, L2, L3, and RAM via random pointer chase
- **CPU affinity**: pins to CPU 0 for consistent results
- **Configurable**: adjustable memory buffer size

## Compile

```bash
gcc -O2 -msse2 -o hwbench hwbench.c
```

## Usage

```bash
./hwbench              # run with default 256MB buffer
./hwbench --size 512   # use 512MB buffer
./hwbench --help       # show help
```

## Output Example

```
hwbench — Hardware Benchmark
────────────────────────────────────────────
Memory buffer: 256 MB

CPU
  Integer (32-bit)       :  1106 MOPS  [#####                   ]
  Integer (64-bit)       :  1110 MOPS  [#####                   ]
  Float (single)         :   556 MOPS  [##                      ]
  Double precision       :   450 MOPS  [##                      ]

Memory Bandwidth
  Write (cached/RFO)    :  26.3 GB/s  [############            ]
  Write (non-temporal)  :  28.3 GB/s  [#############           ]
  Sequential read       :  17.0 GB/s  [########                ]

Cache Latency
  16 KB   (L1)          :   1.2 ns   [                        ]
  256 KB  (L2)          :   3.0 ns   [                        ]
  4 MB    (L3)          :  11.1 ns   [#                       ]
  256 MB  (RAM)         :  96.5 ns   [###########             ]
```
