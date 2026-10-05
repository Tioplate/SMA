# SMA for TSP / TSPTW Research

This repository contains a C implementation of a **Slime Mould Algorithm (SMA)** prototype applied to routing optimization.

The public code currently implements a basic **Traveling Salesman Problem (TSP)** experiment: candidate solution vectors are evaluated by decoding their ordering into a tour and computing its total travel distance.

This project is also the foundation of my master's research at the **University of Tsukuba**, where I am extending the approach toward the **Traveling Salesman Problem with Time Windows (TSPTW)**.

## Current Implementation

The code in this repository currently includes:

- Population initialization for SMA
- Fitness-based population sorting
- SMA weight updates and position updates
- TSP fitness evaluation using a distance matrix
- Tour decoding by sorting solution values into visit order
- Iterative best-solution tracking
- GCC / Make based build configuration

The current entry point runs SMA with a population size of 30.

## Research Direction

My current master's research extends this line of work from TSP to **TSPTW**, where each customer has an allowed service time window and infeasible late arrivals must be avoided.

The broader research explores a hybrid heuristic that combines:

- **Slime Mould Algorithm (SMA)** as the population-based search framework
- **Beam Search** for generating and injecting promising candidate solutions
- **Time-window preprocessing** to reduce infeasible moves and narrow feasible time ranges
- **Greedy repair** for restoring infeasible candidate tours
- **Random-key representation** for encoding tours

> Note: the public repository is currently an earlier SMA/TSP prototype. Some of the TSPTW-specific extensions described above are part of the ongoing research code and are not yet reflected in this repository.

## Project Structure

```text
SMA/
├── Makefile
├── src/
│   ├── main.c
│   ├── sma.c
│   ├── sma.h
│   ├── fitness.c
│   ├── fitness.h
│   └── init.h
└── output/
```

### Main Components

- `src/main.c`  
  Program entry point and SMA parameter initialization.

- `src/sma.c`  
  Core Slime Mould Algorithm implementation, including population updates, fitness sorting, and best-solution tracking.

- `src/fitness.c`  
  Fitness functions, including TSP route decoding and total-distance evaluation.

- `src/init.h`  
  Experimental constants and data definitions used by the prototype.

## Build and Run

### Requirements

- GCC
- GNU Make

### Build

```bash
make
```

### Run

```bash
make run
```

The program prints the best fitness value found during each iteration and reports the final route and execution time.

### Clean

```bash
make clean
```

## Research Goal

The goal of this work is not to provide an exact solver with an optimality guarantee, but to investigate whether a population-based metaheuristic combined with problem-specific search and preprocessing can obtain **high-quality feasible solutions within short computation times** for constrained routing problems.

## Author

**Yuren Chen**  
Master's student, University of Tsukuba

Research interests: combinatorial optimization, metaheuristics, routing problems, and software engineering.
