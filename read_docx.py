#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
从 MT30.docx 中读取表格，提取数据集名称和对应的运行时间限制
"""

try:
    from docx import Document
except ImportError:
    print("请先安装 python-docx: pip install python-docx")
    exit(1)

import re
import json

def extract_time_limits(docx_path):
    """
    从docx文件中提取表格数据，返回 {数据集名称: 时间限制(秒)} 字典
    从 Beam-ACOTime (s) 列提取时间限制
    """
    doc = Document(docx_path)
    time_limits = {}
    
    print("正在读取文档表格...")
    
    # 遍历所有表格
    for table_idx, table in enumerate(doc.tables):
        print(f"\n表格 {table_idx + 1}:")
        print(f"  行数: {len(table.rows)}, 列数: {len(table.columns) if table.rows else 0}")
        
        # 先读取表头，找到"Beam-ACOTime"列的索引
        if not table.rows:
            continue

        header_row = table.rows[0]
        header_cells = [cell.text.strip() for cell in header_row.cells]
        print(f"  表头: {header_cells}")

        # 找到时间列的索引（Beam-ACOTime列）
        time_col_idx = -1
        dataset_col_idx = 0  # 假设第一列是数据集名称

        for idx, header in enumerate(header_cells):
            if 'Beam-ACO' in header and 'Time' in header:
                time_col_idx = idx
                break

        if time_col_idx == -1:
            print("  未找到Beam-ACOTime列，跳过此表格")
            continue

        print(f"  数据集名称列: {dataset_col_idx}, 时间限制列: {time_col_idx}")

        # 遍历数据行（跳过表头）
        for row_idx, row in enumerate(table.rows[1:], start=1):
            cells = [cell.text.strip() for cell in row.cells]

            if len(cells) > max(dataset_col_idx, time_col_idx):
                dataset_name = cells[dataset_col_idx]
                time_text = cells[time_col_idx]

                # 提取时间值（处理特殊情况如 "0" 或 "0(>0.01ms)"）
                match = re.search(r'(\d+(?:\.\d+)?)', time_text)
                if match and dataset_name:
                    time_value = float(match.group(1))

                    # 如果时间为0，设置一个最小值（比如0.01秒）
                    if time_value < 0.01:
                        time_value = 0.01

                    time_limits[dataset_name] = time_value
                    print(f"  行 {row_idx}: {dataset_name} -> {time_value}秒")

    return time_limits

def main():
    docx_path = "request/MT30.docx"
    
    time_limits = extract_time_limits(docx_path)
    
    print(f"\n\n共提取 {len(time_limits)} 个数据集的时间限制:")
    for dataset, time_limit in sorted(time_limits.items()):
        print(f"  {dataset}: {time_limit}秒")
    
    # 保存为JSON文件供C程序读取
    output_json = "dataset_time_limits.json"
    with open(output_json, 'w', encoding='utf-8') as f:
        json.dump(time_limits, f, indent=2, ensure_ascii=False)
    
    print(f"\n时间限制已保存到: {output_json}")
    
    # 也生成一个C头文件格式（可选）
    output_h = "dataset_time_limits.h"
    with open(output_h, 'w', encoding='utf-8') as f:
        f.write("// 自动生成的数据集时间限制配置\n")
        f.write("#ifndef DATASET_TIME_LIMITS_H\n")
        f.write("#define DATASET_TIME_LIMITS_H\n\n")
        f.write("#include <string.h>\n\n")
        f.write("// 根据数据集文件名获取时间限制（秒），返回-1表示未找到\n")
        f.write("static inline double get_time_limit_for_dataset(const char *filename) {\n")
        
        for dataset, time_limit in sorted(time_limits.items()):
            f.write(f'    if (strcmp(filename, "{dataset}") == 0) return {time_limit};\n')
        
        f.write("    return -1.0;  // 未找到对应的时间限制\n")
        f.write("}\n\n")
        f.write("#endif // DATASET_TIME_LIMITS_H\n")
    
    print(f"C头文件已生成: {output_h}")

if __name__ == "__main__":
    main()
