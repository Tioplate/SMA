# SMA-Based Optimization for TSPTW

C implementation for my master's research at the **University of Tsukuba** on heuristic optimization for the **Traveling Salesman Problem with Time Windows (TSPTW)**.

The project explores a population-based search framework built on the **Slime Mould Algorithm (SMA)** and extends it with problem-specific preprocessing, repair, local search, and a **Beam Search hybrid** to obtain good feasible solutions under limited computation time.

> This is experimental research code. The method is heuristic and does not guarantee an optimal solution.

## Problem

In TSPTW, a tour starts from a depot, visits every customer once, and returns to the depot while respecting a time window for each node.

The implementation accounts for:

- travel time between nodes;
- waiting when a vehicle arrives before a time window opens;
- penalties and feasibility checks for late arrivals;
- the return deadline at the depot.

The search objective is to obtain high-quality feasible tours quickly rather than to provide an exact optimality guarantee.

## Core Approach

### 1. Random-key route representation

Each solution is represented by real-valued priority keys. Customer keys are sorted to decode the visiting order, allowing the continuous SMA update rule to be applied to a permutation problem.

### 2. Slime Mould Algorithm

The SMA implementation performs population-based global search with several TSPTW-specific mechanisms, including:

- time-window-guided initialization using earliest, latest, and midpoint values;
- feasibility-aware position updates and rollback;
- adaptive exploration and stagnation handling;
- partial population restart and large mutation;
- route-diversity measurement;
- local improvement with **2-opt** and **Or-opt**;
- path relinking and additional perturbation strategies.

### 3. Time-window preprocessing

Before search, the implementation tightens feasible time ranges through iterative constraint propagation.

The four tightening rules propagate information through predecessor and successor relationships to update earliest and latest feasible service times until no further improvement is found.

A second network-filtering stage removes obviously infeasible transitions and uses short-path feasibility checks and precedence propagation to reduce the effective search space.

### 4. Greedy repair

When a generated route is strongly infeasible, a greedy repair procedure attempts to reconstruct a more feasible visiting order instead of discarding the individual immediately.

### 5. SMA + Beam Search hybrid

The hybrid implementation periodically applies Beam Search to promising members of the SMA population.

The Beam phase:

1. selects high-quality feasible individuals from the population;
2. generates candidate routes using random-key perturbations and 2-opt neighborhoods;
3. evaluates the candidates under the TSPTW objective;
4. keeps strong candidates and injects them into the weaker part of the SMA population while protecting elite solutions.

Beam width is adjusted according to problem size and search stagnation, allowing the method to balance exploration cost and local intensification.

## Time-Limited Search

Both the SMA and SMA-Beam implementations support time-limited execution.

The hybrid version can also stop early when a feasible route reaches a specified target makespan. Final solutions are validated against the original, unpruned problem data before the early-stop condition is accepted.

## Main C Files

| File | Purpose |
| --- | --- |
| `main.c` | Entry point for the SMA solver and time-limited execution |
| `sma.c` / `sma.h` | Core SMA implementation and TSPTW-specific search mechanisms |
| `sma_beam.c` / `sma_beam.h` | SMA-Beam hybrid and adaptive Beam Search |
| `fitness.c` / `fitness.h` | TSPTW evaluation, time-window tightening, network filtering, repair, and local search utilities |
| `readMatrix.c` / `readMatrix.h` | Benchmark-instance loading |
| `test_sma_beam.c` | Experimental driver for the SMA-Beam implementation |
| `validate_route.c` | Route feasibility validation utility |

## Build

The project uses **C11** and **CMake**.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The main targets are:

- `SMA_Back` — base SMA executable;
- `test_sma_beam` — SMA-Beam experimental executable;
- `sma_core` — reusable static library for the base SMA implementation;
- `sma_beam_lib` — reusable static library for the hybrid implementation.

## Research Focus

The current research focuses on the interaction between:

- swarm-intelligence-based global exploration;
- constraint-aware preprocessing;
- greedy feasibility repair;
- neighborhood-based local improvement;
- Beam Search intensification;
- short runtime budgets for difficult TSPTW instances.

The implementation is being used to study the trade-off between **solution quality and computation time** on benchmark TSPTW instances.

## Author

**Yuren Chen**  
Master's student, University of Tsukuba
