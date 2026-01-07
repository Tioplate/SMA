#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
解析 beam_ACO.csv 文件，生成时间限制配置头文件
将 Beam-ACO 的时间值作为 SMA 算法的时间限制（秒）
"""

import csv
import os

def parse_beam_aco_csv(csv_path):
    """解析 beam_ACO.csv 文件，返回 {数据集名: 时间值(秒)} 字典"""
    dataset_times = {}

    with open(csv_path, 'r', encoding='utf-8-sig') as f:
        reader = csv.DictReader(f)
        for row in reader:
            dataset_name = row['Dataset'].replace('.txt', '')
            time_seconds = float(row['Beam-ACOTime(s)'])
            # 直接使用秒，不进行转换
            dataset_times[dataset_name] = time_seconds

    return dataset_times

def generate_header_file(dataset_times, output_path):
    """生成 C 头文件"""
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write("// 自动生成的数据集时间限制配置（基于 beam_ACO.csv 的时间值）\n")
        f.write("// 注意: 时间单位为秒\n")
        f.write("#ifndef DATASET_TIME_LIMITS_H\n")
        f.write("#define DATASET_TIME_LIMITS_H\n\n")
        f.write("#include <string.h>\n\n")
        f.write("// 根据数据集文件名获取时间限制（秒），返回-1表示未找到\n")
        f.write("static inline double get_time_limit_for_dataset(const char *filename) {\n")

        for dataset, time_s in sorted(dataset_times.items()):
            # 直接使用秒值
            f.write(f'    if (strcmp(filename, "{dataset}") == 0) return {time_s:.2f};\n')

        f.write("    return -1.0;  // 未找到对应的时间限制\n")
        f.write("}\n\n")
        f.write("#endif // DATASET_TIME_LIMITS_H\n")

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    csv_path = os.path.join(script_dir, 'dataset', 'beam_ACO.csv')
    output_path = os.path.join(script_dir, 'dataset_time_limits.h')

    print(f"正在解析: {csv_path}")
    dataset_times = parse_beam_aco_csv(csv_path)
    print(f"找到 {len(dataset_times)} 个数据集配置")

    print(f"正在生成: {output_path}")
    generate_header_file(dataset_times, output_path)
    print("完成！")

    # 打印前几个示例
    print("\n示例配置 (单位: 秒):")
    for i, (dataset, time_s) in enumerate(sorted(dataset_times.items())[:5]):
        print(f"  {dataset}: {time_s:.2f} 秒")

if __name__ == '__main__':
    main()
