# Hospital Patient Triage & Bed Allocator

## Overview
[cite_start]This project simulates an emergency room’s patient triage and bed allocation system[cite: 18]. [cite_start]The primary goal is to demonstrate core Operating System concepts, including concurrent process management, Inter-Process Communication (IPC), multithreading, synchronization, and contiguous memory allocation using a system-level C application[cite: 19].

## System Architecture
[cite_start]The system follows a Producer-Consumer architecture divided into three main components[cite: 21]:
* [cite_start]**Triage Script (`triage.sh`):** Acts as the external producer by validating user input and piping formatted patient data to the admissions manager[cite: 22].
* [cite_start]**Admissions Manager (`admissions.c`):** The central hub that uses threads to queue patients, runs bed allocation algorithms (Best-Fit), and spawns child processes for treatment[cite: 23].
* [cite_start]**Patient Simulator (`patient_simulator.c`):** The child processes that simulate treatment duration and use a named FIFO to notify the admissions manager upon discharge[cite: 24].

## Key Features

### Phase 1: Environment & Scripting
* Enforces strict input validation (e.g., names must be letters only, severity 1-10) before mapping severity to a priority level[cite: 27].
* [cite_start]Handles the creation and cleanup of POSIX IPC objects (FIFOs, Shared Memory, Semaphores) for clean startups and graceful shutdowns[cite: 28].

### Phase 2: Process Management & IPC
* [cite_start]Spawns a unique `patient_simulator` process for every admitted patient using `fork()` and `execv()`[cite: 38].
* [cite_start]Implements a `SIGCHLD` handler with `waitpid(-1, NULL, WNOHANG)` to reap processes immediately and prevent zombie memory leaks[cite: 39, 41].
* Utilizes ordinary pipes for external data flow and Named FIFOs (`/tmp/discharge_fifo`) for child-to-parent discharge notifications[cite: 44, 45].

### Phase 3: Threads & Synchronization
* [cite_start]Handles concurrent tasks within the admissions manager using POSIX threads (`pthreads`)[cite: 54].
* [cite_start]Protects the shared memory bed bitmap from data races using strict `pthread_mutex_t` locks[cite: 60].
* Uses a condition variable (`pthread_cond_t`) to efficiently sleep and wake the Scheduler when beds become available[cite: 61, 62].
* [cite_start]Enforces hard limits on ICU and Isolation capacities using counting semaphores (`sem_t`)[cite: 64].

### Phase 4: Memory Management
* Simulates the hospital ward as a contiguous array of memory units[cite: 75].
* [cite_start]Allocates beds using the Best-Fit algorithm (with First-Fit and Worst-Fit toggles available via command-line flags)[cite: 76, 77].
* [cite_start]Logically merges adjacent free blocks upon discharge (Coalescing)[cite: 78].
* Dynamically calculates external fragmentation and simulates internal fragmentation via Paging (2-unit page size)[cite: 79, 80].

## Prerequisites
* Linux Environment (Ubuntu / Linux Mint recommended)
* GCC Compiler
* Make

## Build & Run Instructions

**1. Clean Build Environment**
```bash
make clean && make all
