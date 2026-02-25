// SMA-Beam 混合算法头文件
// 结合黏菌算法(SMA)与束搜索(Beam Search)策略
#pragma once
#include "fitness.h"
#include "sma.h"

// Beam Search 参数结构
typedef struct
{
    int beamWidth;           // 束宽：保留的优秀个体数量
    int expansionFactor;     // 扩展因子：每个束个体产生的邻域解数量
    int hybridInterval;      // 混合间隔：多少代执行一次束搜索
    double eliteRatio;       // 精英比例：直接保留的最优个体比例
    // 新增：自适应参数
    int baseBeamWidth;       // 基础束宽（用于自适应调整）
    int minBeamWidth;        // 最小束宽
    int maxBeamWidth;        // 最大束宽
} BeamConfig;

// SMA-Beam 结果结构（继承SMAResult）
typedef struct
{
    int pop;
    int dimension;
    int iterationATime;
    FIT_DATA_TYPE destinationFitness;
    FIT_DATA_TYPE *convergenceCurve;
    FIT_DATA_TYPE *bestPositions;
    FIT_DATA_TYPE *bestPositionsStart;
    FIT_DATA_TYPE finalDistance;
    FIT_DATA_TYPE finalMakespan;
    FIT_DATA_TYPE elapsedTimeMs;
    int earlyStopTriggered;
    // 新增：Beam Search 统计信息
    int beamSearchExecutions;      // 执行束搜索的次数
    int solutionsFromBeam;         // 来自束搜索的改进解数量
    double avgBeamDiversity;       // 平均束多样性
} SMABeamResult;

// 主要函数：SMA-Beam 混合算法（基于迭代次数）
SMABeamResult* SMA_Beam(
    int pop,
    int DIM,
    const FIT_DATA_TYPE *lb,
    const FIT_DATA_TYPE *ub,
    const char *dataPath,
    int speed,
    BeamConfig beamConfig
);

// 主要函数：SMA-Beam 混合算法（基于时间限制）
SMABeamResult* SMA_Beam_TimeLimited(
    int pop,
    int DIM,
    const FIT_DATA_TYPE *lb,
    const FIT_DATA_TYPE *ub,
    const char *dataPath,
    int speed,
    double timeLimitSeconds,
    BeamConfig beamConfig
);

// 主要函数：SMA-Beam 混合算法（基于时间限制且可提前停止）
SMABeamResult* SMA_Beam_TimeLimited_WithEarlyStop(
    int pop,
    int DIM,
    const FIT_DATA_TYPE *lb,
    const FIT_DATA_TYPE *ub,
    const char *dataPath,
    int speed,
    double timeLimitSeconds,
    double expectedMakespan,
    BeamConfig beamConfig
);

// 辅助函数：创建默认 BeamConfig
BeamConfig createDefaultBeamConfig(int pop);

// 辅助函数：创建自适应 BeamConfig（根据问题规模动态调整）
BeamConfig createAdaptiveBeamConfig(int pop, int dimension);

// 辅助函数：束搜索核心操作（从当前种群中选择束并扩展）
void beamSearchPhase(
    FIT_DATA_TYPE **x,
    fitnessData *fit,
    FIT_DATA_TYPE *bestPositions,
    int pop,
    int DIM,
    int speed,
    dataMatrix *data,
    BeamConfig beamConfig,
    FIT_DATA_TYPE *globalBest
);
