-----

# Memory Access and Trace Analysis Tools for ChampSim 

for the version here https://github.com/svassi04/RRIP_work/tree/main

This document outlines new functionality added to a ChampSim binary and provides a set of scripts for analyzing memory access patterns and trace files.

## 1\. New Flag in ChampSim

A new flag, `-memory_addresses`, has been added to the ChampSim binary to capture instruction-level memory access information.

  - **Default Behavior:** The binary's default behavior remains unchanged.
  - **New Flag:** `-memory_addresses <addresses_outfile>`
      - This flag enables the new functionality, directing all memory access data to the specified output file.
      - **Output Format:**
          - Read Access: `ip: <instruction_address> source_memory: <memory_address>`
          - Write Access: `ip: <instruction_address> destination_memory: <memory_address>`

**Example Execution:**

```bash
./bin/perceptron-next_line-ip_stride-lru-1core -warmup_instructions 5000 -simulation_instructions 10000 -memory_addresses file.txt -traces /mnt/beegfs/svassi04/memc_test/memc_001.out.gz
```

**Example Output:**

```
ip: 7ffff69f818f source_memory: 7fffffffdd68
ip: 7ffff69f819d destination_memory: 7fffffffdc48
```

-----

## 2\. Trace Manipulation Scripts

The following scripts are designed to process and manipulate ChampSim traces.

### 2.1. Cutting Traces

This script splits a large trace file into smaller, fixed-size traces.

**Usage:**

```bash
python3 cut_champsim_trace.py mytrace.out.gz 150000000
```

### 2.2. Combining Traces

To combine multiple traces into a single file, you can decompress, append, and re-compress them.

**Usage:**

```bash
gunzip -c trace1.gz > trace1.out
gunzip -c trace2.gz > trace2.out
cat trace2.out >> trace1.out
gzip trace1.out
```

-----

## 3\. Memory Conflict Analysis

These scripts analyze memory conflicts, providing insights into read-write dependencies across multiple traces.

### 3.1. Preparing Address Data

First, process the output file from the `-memory_addresses` flag to prepare the data for analysis.

**Usage:**

```bash
awk '{print $3, $4}' <champsim_read_write_addresses> | sort | uniq -c > <champsim_read_write_uniq_addresses>
```

This command extracts unique addresses and their access types from the raw output.

### 3.2. Find Memory Conflicts Script

The `find_memory_conflicts` script is used to compare prepared address data from multiple traces.

**Usage:**

```bash
./find_memory_conflicts <champsim_read_write_addresses_1> <champsim_read_write_addresses_2> ...
```

**Options:**

  - `-s <number>`: Sets the cache block size in bits for block-level comparison.
      - `0`: For an **exact byte-level match**.
      - `6`: For a **64-byte block size**.
  - `-o <out_filename>`: Specifies the output file for the conflict results.
  - `-B`: Appends a statistical breakdown to the output file.

**Statistical Breakdown (`-B` flag):**

  - **MatchesWithSelf**: Conflicts where a read and write of the same address occur within a single trace.
  - **MatchesWithoutSelf**: Conflicts where a read in one trace and a write in another trace conflict.
  - **TotalSources**: The total number of unique `source_memory` accesses.
  - **TotalDestinations**: The total number of unique `destination_memory` accesses.
