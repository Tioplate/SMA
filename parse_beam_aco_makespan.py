#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Parse beam_ACO.csv file and generate expected results configuration header file
Use Beam-ACO Makespan values as expected results for early stopping
"""

import csv
import os

def parse_beam_aco_makespan(csv_path):
    """Parse beam_ACO.csv file, return {dataset name: makespan value} dictionary"""
    dataset_makespans = {}

    with open(csv_path, 'r', encoding='utf-8-sig') as f:
        reader = csv.DictReader(f)
        for row in reader:
            dataset_name = row['Dataset'].replace('.txt', '')
            makespan = float(row['Beam-ACO(Makespan)'])
            dataset_makespans[dataset_name] = makespan

    return dataset_makespans

def generate_header_file(dataset_makespans, output_path):
    """Generate C header file"""
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write("// Auto-generated expected results configuration file\n")
        f.write("// Extracted from beam_ACO.csv - Beam-ACO(Makespan) column\n")
        f.write("#ifndef EXPECTED_RESULTS_H\n")
        f.write("#define EXPECTED_RESULTS_H\n\n")
        f.write("#include <string.h>\n\n")
        f.write("// Get expected Makespan by dataset filename, return -1 if not found\n")
        f.write("static inline double get_expected_makespan(const char *filename) {\n")

        for dataset, makespan in sorted(dataset_makespans.items()):
            f.write(f'    if (strcmp(filename, "{dataset}") == 0) return {makespan:.2f};\n')

        f.write("    return -1.0;  // Expected result not found\n")
        f.write("}\n\n")
        f.write("#endif // EXPECTED_RESULTS_H\n")

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    csv_path = os.path.join(script_dir, 'dataset', 'beam_ACO.csv')
    output_path = os.path.join(script_dir, 'expected_results.h')

    print(f"Parsing: {csv_path}")
    dataset_makespans = parse_beam_aco_makespan(csv_path)
    print(f"Found {len(dataset_makespans)} dataset configurations")

    print(f"Generating: {output_path}")
    generate_header_file(dataset_makespans, output_path)
    print("Complete!")

    # Print first few examples
    print("\nExample configurations (Expected Makespan):")
    for i, (dataset, makespan) in enumerate(sorted(dataset_makespans.items())[:5]):
        print(f"  {dataset}: {makespan:.2f}")

if __name__ == '__main__':
    main()

