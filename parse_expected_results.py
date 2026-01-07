#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
从 dataset/results.csv 中提取预期的 Makespan 结果，生成 C 头文件
"""

import csv
import re

def parse_results_csv(csv_path):
    """
    从 CSV 文件中提取数据集名称和预期 Makespan
    """
    expected_results = {}

    with open(csv_path, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            dataset = row['Dataset']
            makespan = float(row['Makespan'])

            # 提取基础名称（去掉.txt后缀）
            basename = dataset.replace('.txt', '')
            expected_results[basename] = makespan

    return expected_results

def generate_header_file(expected_results, output_path):
    """
    生成 C 头文件
    """
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write("// 自动生成的预期结果配置文件\n")
        f.write("// 从 dataset/results.csv 提取\n")
        f.write("#ifndef EXPECTED_RESULTS_H\n")
        f.write("#define EXPECTED_RESULTS_H\n\n")
        f.write("#include <string.h>\n\n")
        f.write("// 根据数据集文件名获取预期的 Makespan，返回-1表示未找到\n")
        f.write("static inline double get_expected_makespan(const char *filename) {\n")

        for dataset, makespan in sorted(expected_results.items()):
            f.write(f'    if (strcmp(filename, "{dataset}") == 0) return {makespan};\n')

        f.write("    return -1.0;  // 未找到对应的预期结果\n")
        f.write("}\n\n")
        f.write("#endif // EXPECTED_RESULTS_H\n")

def main():
    csv_path = "dataset/results.csv"
    output_h = "expected_results.h"

    print(f"正在解析 {csv_path}...")
    expected_results = parse_results_csv(csv_path)

    print(f"\n共提取 {len(expected_results)} 个数据集的预期结果:")
    for dataset, makespan in sorted(expected_results.items()):
        print(f"  {dataset}: Makespan = {makespan}")

    generate_header_file(expected_results, output_h)
    print(f"\nC头文件已生成: {output_h}")

if __name__ == "__main__":
    main()

