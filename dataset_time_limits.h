// 自动生成的数据集时间限制配置（基于 beam_ACO.csv 的时间值）
// 注意: 时间单位为秒
#ifndef DATASET_TIME_LIMITS_H
#define DATASET_TIME_LIMITS_H

#include <string.h>

// 根据数据集文件名获取时间限制（秒），返回-1表示未找到
static inline double get_time_limit_for_dataset(const char *filename) {
    if (strcmp(filename, "rc_201.1") == 0) return 0.01;
    if (strcmp(filename, "rc_201.2") == 0) return 0.39;
    if (strcmp(filename, "rc_201.3") == 0) return 0.09;
    if (strcmp(filename, "rc_201.4") == 0) return 0.16;
    if (strcmp(filename, "rc_202.1") == 0) return 0.95;
    if (strcmp(filename, "rc_202.2") == 0) return 0.01;
    if (strcmp(filename, "rc_202.3") == 0) return 0.26;
    if (strcmp(filename, "rc_202.4") == 0) return 1.18;
    if (strcmp(filename, "rc_203.1") == 0) return 0.01;
    if (strcmp(filename, "rc_203.2") == 0) return 4.05;
    if (strcmp(filename, "rc_203.3") == 0) return 20.47;
    if (strcmp(filename, "rc_203.4") == 0) return 0.01;
    if (strcmp(filename, "rc_204.1") == 0) return 24.79;
    if (strcmp(filename, "rc_204.2") == 0) return 22.46;
    if (strcmp(filename, "rc_204.3") == 0) return 19.15;
    if (strcmp(filename, "rc_205.1") == 0) return 0.03;
    if (strcmp(filename, "rc_205.2") == 0) return 3.44;
    if (strcmp(filename, "rc_205.3") == 0) return 0.13;
    if (strcmp(filename, "rc_205.4") == 0) return 1.70;
    if (strcmp(filename, "rc_206.1") == 0) return 0.01;
    if (strcmp(filename, "rc_206.2") == 0) return 14.22;
    if (strcmp(filename, "rc_206.3") == 0) return 0.01;
    if (strcmp(filename, "rc_206.4") == 0) return 29.44;
    if (strcmp(filename, "rc_207.1") == 0) return 26.88;
    if (strcmp(filename, "rc_207.2") == 0) return 24.21;
    if (strcmp(filename, "rc_207.3") == 0) return 34.73;
    if (strcmp(filename, "rc_207.4") == 0) return 0.01;
    if (strcmp(filename, "rc_208.1") == 0) return 41.96;
    if (strcmp(filename, "rc_208.2") == 0) return 8.50;
    if (strcmp(filename, "rc_208.3") == 0) return 26.67;
    return -1.0;  // 未找到对应的时间限制
}

#endif // DATASET_TIME_LIMITS_H
