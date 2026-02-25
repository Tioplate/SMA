// SMA-Beam 混合算法实现
// 结合黏菌算法的全局搜索能力与束搜索的局部强化能力

#include "sma_beam.h"
#include <string.h>
#include <math.h>
#include <time.h>

// 创建默认 BeamConfig
BeamConfig createDefaultBeamConfig(int pop)
{
    BeamConfig config;
    config.beamWidth = (pop >= 50) ? (pop / 5) : 10;  // 20%的种群或至少10
    config.expansionFactor = 5;                        // 每个束个体产生5个邻域解
    config.hybridInterval = 20;                        // 每20代执行一次束搜索
    config.eliteRatio = 0.1;                          // 保留10%精英
    return config;
}

// 创建自适应 BeamConfig（根据问题规模动态调整）
BeamConfig createAdaptiveBeamConfig(int pop, int dimension)
{
    BeamConfig config;

    if (dimension <= 25)
    {
        // 小规模问题：标准配置
        config.beamWidth = (pop >= 50) ? (pop / 5) : 10;
        config.expansionFactor = 5;
        config.hybridInterval = 20;
        config.eliteRatio = 0.1;
        config.baseBeamWidth = config.beamWidth;
        config.minBeamWidth = config.beamWidth / 2;
        config.maxBeamWidth = config.beamWidth * 3;
    }
    else if (dimension <= 40)
    {
        // 中等规模：适度增强
        config.beamWidth = (pop >= 50) ? (pop / 4) : 15;  // 25%种群
        config.expansionFactor = 6;
        config.hybridInterval = 15;  // 更频繁的束搜索
        config.eliteRatio = 0.12;
        config.baseBeamWidth = config.beamWidth;
        config.minBeamWidth = config.beamWidth / 2;
        config.maxBeamWidth = config.beamWidth * 3;
    }
    else
    {
        // 大规模问题（>40）：强化配置
        config.beamWidth = (pop >= 50) ? (pop / 3) : 20;  // 33%种群
        config.expansionFactor = 8;  // 更多邻域探索
        config.hybridInterval = 10;  // 非常频繁的束搜索
        config.eliteRatio = 0.15;    // 保护更多精英
        config.baseBeamWidth = config.beamWidth;
        config.minBeamWidth = config.beamWidth / 2;
        config.maxBeamWidth = config.beamWidth * 2;

        printf("[Adaptive Config] Large-scale problem detected (dim=%d)\n", dimension);
        printf("  Enhanced: BeamWidth=%d (range: %d-%d), ExpansionFactor=%d, HybridInterval=%d\n",
               config.beamWidth, config.minBeamWidth, config.maxBeamWidth,
               config.expansionFactor, config.hybridInterval);
    }

    return config;
}

// 生成邻域解：基于优先级键的小幅扰动
static void generateNeighbor(const FIT_DATA_TYPE *source, FIT_DATA_TYPE *neighbor,
                             int dim, double perturbationStrength)
{
    for (int i = 0; i < dim; i++)
    {
        neighbor[i] = source[i];
    }

    // 根据扰动强度选择策略
    double rand_val = rand01();

    if (perturbationStrength < 0.15)
    {
        // 小扰动：局部微调（1-3个维度）
        int numPerturbations = 1 + rand() % 3;
        for (int p = 0; p < numPerturbations; p++)
        {
            int idx = 1 + rand() % (dim - 1);
            double delta = (rand01() * 2.0 - 1.0) * perturbationStrength * (dim - 1) * 0.5;
            neighbor[idx] += delta;

            if (neighbor[idx] < 0) neighbor[idx] = rand01() * 0.5;
            if (neighbor[idx] > dim - 1) neighbor[idx] = (dim - 1) - rand01() * 0.5;
        }
    }
    else if (perturbationStrength < 0.35)
    {
        // 中等扰动：区域性调整（2-5个维度）
        int numPerturbations = 2 + rand() % 4;
        for (int p = 0; p < numPerturbations; p++)
        {
            int idx = 1 + rand() % (dim - 1);
            double delta = (rand01() * 2.0 - 1.0) * perturbationStrength * (dim - 1);
            neighbor[idx] += delta;

            if (neighbor[idx] < 0) neighbor[idx] = rand01() * 0.5;
            if (neighbor[idx] > dim - 1) neighbor[idx] = (dim - 1) - rand01() * 0.5;
        }
    }
    else
    {
        // 大扰动：大范围重组（3-6个维度）
        int numPerturbations = 3 + rand() % 4;
        for (int p = 0; p < numPerturbations; p++)
        {
            int idx = 1 + rand() % (dim - 1);
            double delta = (rand01() * 2.0 - 1.0) * perturbationStrength * (dim - 1) * 1.5;
            neighbor[idx] += delta;

            if (neighbor[idx] < 0) neighbor[idx] = rand01() * 0.5;
            if (neighbor[idx] > dim - 1) neighbor[idx] = (dim - 1) - rand01() * 0.5;
        }
    }

    // 30% 概率进行键值交换（改变访问顺序）
    if (rand01() < 0.3)
    {
        int numSwaps = 1 + rand() % 2;  // 1-2次交换
        for (int s = 0; s < numSwaps; s++)
        {
            int i = 1 + rand() % (dim - 1);
            int j = 1 + rand() % (dim - 1);
            if (i != j)
            {
                FIT_DATA_TYPE tmp = neighbor[i];
                neighbor[i] = neighbor[j];
                neighbor[j] = tmp;
            }
        }
    }
}

// 2-opt 邻域操作：交换两个客户的访问顺序
static void generate2OptNeighbor(const FIT_DATA_TYPE *source, FIT_DATA_TYPE *neighbor, int dim)
{
    // 复制源解
    for (int i = 0; i < dim; i++)
    {
        neighbor[i] = source[i];
    }

    // 解码为访问顺序
    xData *order = (xData *)malloc((dim - 1) * sizeof(xData));
    for (int i = 1, k = 0; i < dim; i++, k++)
    {
        order[k].xIndex = i;
        order[k].data = neighbor[i];
    }
    sortX(order, dim - 1);

    // 随机选择两个位置进行2-opt交换
    if (dim > 3)
    {
        int pos1 = rand() % (dim - 1);
        int pos2 = rand() % (dim - 1);
        if (pos1 > pos2) { int tmp = pos1; pos1 = pos2; pos2 = tmp; }
        if (pos2 - pos1 >= 2)
        {
            // 反转 [pos1+1, pos2] 区间
            for (int i = pos1 + 1, j = pos2; i < j; i++, j--)
            {
                int tmpIdx = order[i].xIndex;
                order[i].xIndex = order[j].xIndex;
                order[j].xIndex = tmpIdx;
            }
        }
    }

    // 重新编码为优先级键
    for (int k = 0; k < dim - 1; k++)
    {
        int nodeIdx = order[k].xIndex;
        neighbor[nodeIdx] = (FIT_DATA_TYPE)k + 1e-6 * k;
    }

    free(order);
}

// 束搜索核心操作
void beamSearchPhase(FIT_DATA_TYPE **x, fitnessData *fit, FIT_DATA_TYPE *bestPositions,
                     int pop, int DIM, int speed, dataMatrix *data,
                     BeamConfig beamConfig, FIT_DATA_TYPE *globalBest)
{
    if (!x || !fit || !data || pop <= 0 || DIM <= 1) return;

    int beamWidth = beamConfig.beamWidth;
    if (beamWidth > pop) beamWidth = pop;
    if (beamWidth < 2) beamWidth = 2;

    // 第一步：从当前种群中选择束（top beamWidth 个可行解）
    FIT_DATA_TYPE **beamSolutions = (FIT_DATA_TYPE **)malloc(beamWidth * sizeof(FIT_DATA_TYPE *));
    int beamCount = 0;

    for (int i = 0; i < pop && beamCount < beamWidth; i++)
    {
        if (fit[i].fitness != FIT_DATA_TYPE_MAX)  // 只选可行解
        {
            beamSolutions[beamCount] = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
            memcpy(beamSolutions[beamCount], x[i], DIM * sizeof(FIT_DATA_TYPE));
            beamCount++;
        }
    }

    if (beamCount == 0)
    {
        free(beamSolutions);
        return;  // 没有可行解，跳过束搜索
    }

    // 第二步：从每个束解扩展邻域解
    int expansionFactor = beamConfig.expansionFactor;
    int totalCandidates = beamCount * expansionFactor;
    FIT_DATA_TYPE **candidates = (FIT_DATA_TYPE **)malloc(totalCandidates * sizeof(FIT_DATA_TYPE *));
    fitnessData *candidateFitness = (fitnessData *)malloc(totalCandidates * sizeof(fitnessData));
    int candidateIdx = 0;

    for (int b = 0; b < beamCount; b++)
    {
        for (int e = 0; e < expansionFactor && candidateIdx < totalCandidates; e++)
        {
            candidates[candidateIdx] = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));

            // 不同的邻域生成策略
            if (e % 3 == 0)
            {
                // 小幅随机扰动
                generateNeighbor(beamSolutions[b], candidates[candidateIdx], DIM, 0.1);
            }
            else if (e % 3 == 1)
            {
                // 中等扰动
                generateNeighbor(beamSolutions[b], candidates[candidateIdx], DIM, 0.3);
            }
            else
            {
                // 2-opt 邻域
                generate2OptNeighbor(beamSolutions[b], candidates[candidateIdx], DIM);
            }

             // 评估候选解
            candidateFitness[candidateIdx].popIndex = candidateIdx;
            candidateFitness[candidateIdx].fitness = TSPTW(candidates[candidateIdx], DIM, speed, data);
            candidateIdx++;
        }
    }

    // 第三步：从候选解中选择最优的 beamWidth 个解
    sortFitness(candidateFitness, candidateIdx);

    // 第四步：用优秀的候选解替换种群中的较差个体
    int replaceStart = pop - beamCount;  // 从后部开始替换
    if (replaceStart < (int)(pop * beamConfig.eliteRatio))
    {
        replaceStart = (int)(pop * beamConfig.eliteRatio);  // 保护精英
    }

    int replaceCount = 0;
    for (int i = 0; i < candidateIdx && replaceCount < beamCount && (replaceStart + replaceCount) < pop; i++)
    {
        if (candidateFitness[i].fitness != FIT_DATA_TYPE_MAX)
        {
            int targetIdx = replaceStart + replaceCount;
            memcpy(x[targetIdx], candidates[candidateFitness[i].popIndex], DIM * sizeof(FIT_DATA_TYPE));
            fit[targetIdx].fitness = candidateFitness[i].fitness;

            // 更新全局最优
            if (candidateFitness[i].fitness < *globalBest)
            {
                *globalBest = candidateFitness[i].fitness;
                memcpy(bestPositions, candidates[candidateFitness[i].popIndex], DIM * sizeof(FIT_DATA_TYPE));
            }

            replaceCount++;
        }
    }

    // 清理资源
    for (int i = 0; i < beamCount; i++)
    {
        free(beamSolutions[i]);
    }
    free(beamSolutions);

    for (int i = 0; i < candidateIdx; i++)
    {
        free(candidates[i]);
    }
    free(candidates);
    free(candidateFitness);
}

// 计算束多样性（汉明距离的平均值）
static double calculateBeamDiversity(FIT_DATA_TYPE **beamSolutions, int beamCount, int dim)
{
    if (beamCount <= 1) return 0.0;

    double totalDistance = 0.0;
    int pairCount = 0;

    for (int i = 0; i < beamCount - 1; i++)
    {
        for (int j = i + 1; j < beamCount; j++)
        {
            // 解码并比较
            xData *order1 = (xData *)malloc((dim - 1) * sizeof(xData));
            xData *order2 = (xData *)malloc((dim - 1) * sizeof(xData));

            for (int k = 1, idx = 0; k < dim; k++, idx++)
            {
                order1[idx].xIndex = k;
                order1[idx].data = beamSolutions[i][k];
                order2[idx].xIndex = k;
                order2[idx].data = beamSolutions[j][k];
            }

            sortX(order1, dim - 1);
            sortX(order2, dim - 1);

            int diffCount = 0;
            for (int k = 0; k < dim - 1; k++)
            {
                if (order1[k].xIndex != order2[k].xIndex) diffCount++;
            }

            totalDistance += (double)diffCount / (double)(dim - 1);
            pairCount++;

            free(order1);
            free(order2);
        }
    }

    return (pairCount > 0) ? (totalDistance / pairCount) : 0.0;
}

// SMA-Beam 混合算法主函数（基于时间限制且可提前停止）
SMABeamResult* SMA_Beam_TimeLimited_WithEarlyStop(
    int pop, int DIM, const FIT_DATA_TYPE *lb, const FIT_DATA_TYPE *ub,
    const char *dataPath, int speed, double timeLimitSeconds,
    double expectedMakespan, BeamConfig beamConfig)
{
    // 初始化
    FIT_DATA_TYPE **x = initialization(pop, DIM);
    fitnessData *fit = (fitnessData *)malloc(pop * sizeof(fitnessData));
    FIT_DATA_TYPE *bestPositions = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
    FIT_DATA_TYPE *bestPositionsStart = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
    FIT_DATA_TYPE destinationFitness = FIT_DATA_TYPE_MAX;
    FIT_DATA_TYPE **W = (FIT_DATA_TYPE **)malloc(pop * sizeof(FIT_DATA_TYPE *));

    for (int i = 0; i < pop; i++)
    {
        W[i] = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
    }

    dataMatrix *data = readMatrix((char*)dataPath);

    // Beam Search 统计
    int beamSearchExecutions = 0;
    int solutionsFromBeam = 0;
    double totalBeamDiversity = 0.0;

    clock_t startClock = clock();
    double timeLimitMs = timeLimitSeconds * 1000.0;

    // 时间窗启发式初始化（与原SMA相同）
    if (data && data->size >= DIM)
    {
        double minEar = 1e18, maxEar = -1e18;
        double minLat = 1e18, maxLat = data->tw[1].latest;
        for (int j = 1; j < DIM; j++)
        {
            if (data->tw[j].earliest < minEar) minEar = data->tw[j].earliest;
            if (data->tw[j].earliest > maxEar) maxEar = data->tw[j].earliest;
            if (data->tw[j].latest < minLat) minLat = data->tw[j].latest;
            if (data->tw[j].latest > maxLat) maxLat = data->tw[j].latest;
        }

        // 前3个个体使用启发式
        for (int j = 0; j < DIM; j++) x[0][j] = 0;
        for (int j = 1; j < DIM; j++)
            x[0][j] = ((maxEar - minEar) > 1e-9) ?
                      ((data->tw[j].earliest - minEar) / (maxEar - minEar) * (DIM - 1) + 1e-6 * j) :
                      (rand01() * (DIM - 1) + 1e-6 * j);

        if (pop > 1)
        {
            for (int j = 0; j < DIM; j++) x[1][j] = 0;
            for (int j = 1; j < DIM; j++)
                x[1][j] = ((maxLat - minLat) > 1e-9) ?
                          ((data->tw[j].latest - minLat) / (maxLat - minLat) * (DIM - 1) + 1e-6 * j) :
                          (rand01() * (DIM - 1) + 1e-6 * j);
        }

        if (pop > 2)
        {
            for (int j = 0; j < DIM; j++) x[2][j] = 0;
            for (int j = 1; j < DIM; j++)
            {
                double mid = 0.5 * (data->tw[j].earliest + data->tw[j].latest);
                x[2][j] = ((maxEar - minEar) > 1e-9) ?
                          ((mid - minEar) / (maxEar - minEar) * (DIM - 1) + 1e-6 * j) :
                          (rand01() * (DIM - 1) + 1e-6 * j);
            }
        }
    }

    // 初始评估（带强制修复）
    for (int i = 0; i < pop; i++)
    {
        fit[i].popIndex = i;
        fit[i].fitness = TSPTW(x[i], DIM, speed, data);

        // 如果严重不可行，立即修复
        if (fit[i].fitness > 10000.0 && data != NULL)
        {
            repairSolutionGreedy(x[i], DIM, data, speed);
            fit[i].fitness = TSPTW(x[i], DIM, speed, data);
        }

        if (fit[i].fitness < destinationFitness)
        {
            destinationFitness = fit[i].fitness;
            for (int j = 0; j < DIM; j++)
            {
                bestPositions[j] = x[fit[i].popIndex][j];
                bestPositionsStart[j] = x[fit[i].popIndex][j];
            }
        }
    }

    int t = 1;
    int earlyStopTriggered = 0;
    int lastImprovementIter = 0;
    const int patience = 50;

    // 新增：停滞检测和自适应参数
    int stagnationCounter = 0;
    const int STAGNATION_THRESHOLD = 500;  // 500次迭代未改进视为停滞
    int currentBeamWidth = beamConfig.beamWidth;

    printf("SMA-Beam Hybrid Algorithm Started (Time Limit: %.2f seconds)\n", timeLimitSeconds);
    printf("Beam Width: %d, Expansion Factor: %d, Hybrid Interval: %d\n",
           beamConfig.beamWidth, beamConfig.expansionFactor, beamConfig.hybridInterval);
    printf("Adaptive Beam Width Range: [%d, %d]\n", beamConfig.minBeamWidth, beamConfig.maxBeamWidth);

    // 动态收敛曲线（预估最多记录100000个点，足够60秒运行）
    int maxIterations = 100000;
    FIT_DATA_TYPE *convergenceCurve = (FIT_DATA_TYPE *)malloc(maxIterations * sizeof(FIT_DATA_TYPE));

    // 主循环：基于时间限制
    while (1)
    {
        clock_t currentClock = clock();
        double elapsedMs = (double)(currentClock - startClock) * 1000.0 / CLOCKS_PER_SEC;

        if (elapsedMs >= timeLimitMs)
        {
            printf("Time limit reached at iteration %d\n", t);
            break;
        }

        // Early Stop机制已取消，算法将运行到时间限制为止
        // if (expectedMakespan > 0 && destinationFitness <= expectedMakespan)
        // {
        //     printf("Expected result achieved at iteration %d: %.2f <= %.2f\n",
        //            t, destinationFitness, expectedMakespan);
        //     earlyStopTriggered = 1;
        //     break;
        // }

        // 排序种群
        fit = sortFitness(fit, pop);
        x = sortIndex(x, fit, pop);
        for (int i = 0; i < pop; i++)
        {
            fit[i].popIndex = i;
        }

        FIT_DATA_TYPE bestFitness = fit[0].fitness;
        FIT_DATA_TYPE worstFitness = fit[pop - 1].fitness;

        // === 自适应Beam宽度调整 ===
        // 检测是否有改进
        if (t > 1 && destinationFitness >= convergenceCurve[t - 2] - 1e-6)
        {
            stagnationCounter++;
        }
        else
        {
            stagnationCounter = 0;
        }

        // 根据停滞情况动态调整beam宽度
        if (stagnationCounter > 0 && stagnationCounter % 100 == 0)
        {
            // 长时间停滞，增加beam宽度以增强探索
            if (currentBeamWidth < beamConfig.maxBeamWidth)
            {
                int increment = (beamConfig.maxBeamWidth - beamConfig.baseBeamWidth) / 5;
                if (increment < 2) increment = 2;
                currentBeamWidth += increment;
                if (currentBeamWidth > beamConfig.maxBeamWidth)
                    currentBeamWidth = beamConfig.maxBeamWidth;

                printf("[Iter %d] Stagnation detected (%d iters), increasing beam width to %d\n",
                       t, stagnationCounter, currentBeamWidth);
            }
        }
        else if (stagnationCounter == 0 && t > 100)
        {
            // 持续改进，逐渐减小beam宽度以提高效率
            if (currentBeamWidth > beamConfig.minBeamWidth)
            {
                int decrement = 1;
                if (currentBeamWidth - decrement >= beamConfig.minBeamWidth)
                {
                    currentBeamWidth -= decrement;
                }
            }
        }

        // 更新beamConfig的实际宽度
        BeamConfig adaptiveConfig = beamConfig;
        adaptiveConfig.beamWidth = currentBeamWidth;

        // === 束搜索阶段 ===
        if (t % beamConfig.hybridInterval == 0)
        {
            FIT_DATA_TYPE oldBest = destinationFitness;
            beamSearchPhase(x, fit, bestPositions, pop, DIM, speed, data,
                           adaptiveConfig, &destinationFitness);
            beamSearchExecutions++;

            if (destinationFitness < oldBest)
            {
                solutionsFromBeam++;
                lastImprovementIter = t;
                stagnationCounter = 0;  // 重置停滞计数器
                printf("[Iter %d] Beam Search improved: %.2f -> %.2f (BeamWidth=%d)\n",
                       t, oldBest, destinationFitness, currentBeamWidth);
            }
        }

        // === SMA 更新阶段 ===
        int skipUpdate = 0;
        if (bestFitness == FIT_DATA_TYPE_MAX)
        {
            // 无可行解时重采样
            for (int i = 0; i < pop; i++)
            {
                for (int j = 0; j < DIM; j++)
                {
                    x[i][j] = rand01() * (DIM - 1) + 1e-6 * j;
                }
            }
            skipUpdate = 1;
        }

        if (!skipUpdate)
        {
            FIT_DATA_TYPE S = (worstFitness - bestFitness) + 1e-8;
            if (S < 1e-12) S = 1e-12;

            // 计算权重
            for (int i = 0; i < pop; i++)
            {
                for (int j = 0; j < DIM; j++)
                {
                    if (fit[i].fitness == FIT_DATA_TYPE_MAX)
                    {
                        W[i][j] = 0.0;
                        continue;
                    }
                    FIT_DATA_TYPE numer = (fit[i].fitness - bestFitness);
                    if (numer < 0) numer = 0;
                    FIT_DATA_TYPE frac = numer / S;
                    if (i < pop / 2)
                    {
                        W[i][j] = 1 + rand01() * log10(frac + 1.0);
                    }
                    else
                    {
                        W[i][j] = 1 - rand01() * log10(frac + 1.0);
                    }
                }
            }

            FIT_DATA_TYPE tt = -((FIT_DATA_TYPE)t / (FIT_DATA_TYPE)maxIterations) + 1;
            FIT_DATA_TYPE a = (tt > -1 && tt < 1) ? atanh(tt) : 1;
            FIT_DATA_TYPE b = 1 - (FIT_DATA_TYPE)t / (FIT_DATA_TYPE)maxIterations;
            if (b < 1e-3) b = 1e-3;

            double zNow = 0.03 + 0.07 * (1.0 - (double)t / maxIterations);  // 递减的探索概率

            // 位置更新
            for (int i = 0; i < pop; i++)
            {
                FIT_DATA_TYPE *xOld = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
                memcpy(xOld, x[i], DIM * sizeof(FIT_DATA_TYPE));
                FIT_DATA_TYPE oldFitness = fit[i].fitness;

                if (rand01() < zNow)
                {
                    // 全局探索
                    for (int j = 0; j < DIM; j++)
                    {
                        x[i][j] = rand01() * (DIM - 1) + 1e-6 * j;
                    }
                }
                else
                {
                    // 局部开发
                    FIT_DATA_TYPE p = tanh(fabs(fit[i].fitness - destinationFitness));
                    for (int j = 0; j < DIM; j++)
                    {
                        FIT_DATA_TYPE r = rand01();
                        int A = rand() % pop;
                        int B = rand() % pop;
                        FIT_DATA_TYPE vb = 2 * a * rand01() - a;
                        FIT_DATA_TYPE vc = 2 * b * rand01() - b;

                        if (r < p)
                        {
                            FIT_DATA_TYPE step = vb * (W[i][j] * x[A][j] - x[B][j]) * 0.15;
                            x[i][j] = bestPositions[j] + step;
                        }
                        else
                        {
                            x[i][j] = x[i][j] + vc * x[i][j] * 0.08;
                        }

                        // 边界处理
                        if (x[i][j] < 0) x[i][j] = rand01() * 0.5;
                        if (x[i][j] > DIM - 1) x[i][j] = (DIM - 1) - rand01() * 0.5;
                    }
                }

                // 评估并保持可行性
                FIT_DATA_TYPE newFitness = TSPTW(x[i], DIM, speed, data);
                if (newFitness == FIT_DATA_TYPE_MAX && oldFitness != FIT_DATA_TYPE_MAX)
                {
                    memcpy(x[i], xOld, DIM * sizeof(FIT_DATA_TYPE));
                }

                free(xOld);
            }
        }

        // 重新评估
        for (int i = 0; i < pop; i++)
        {
            fit[i].fitness = TSPTW(x[i], DIM, speed, data);
            if (fit[i].fitness < destinationFitness)
            {
                destinationFitness = fit[i].fitness;
                memcpy(bestPositions, x[i], DIM * sizeof(FIT_DATA_TYPE));
                lastImprovementIter = t;
            }
        }

        // 每1000次迭代，强制修复最差的30%个体（防止陷入不可行区域）
        if (t % 1000 == 0 && data != NULL)
        {
            int repairCount = pop * 3 / 10; // 修复30%
            for (int i = pop - repairCount; i < pop; i++)
            {
                if (fit[i].fitness > 10000.0)
                {
                    repairSolutionGreedy(x[i], DIM, data, speed);
                    fit[i].fitness = TSPTW(x[i], DIM, speed, data);
                }
            }
        }

        // 记录收敛曲线
        if (t - 1 < maxIterations)
        {
            convergenceCurve[t - 1] = destinationFitness;
        }

        // 停滞重启
        if (t - lastImprovementIter >= patience)
        {
            int restartCount = pop / 3;
            for (int i = pop - restartCount; i < pop; i++)
            {
                for (int j = 0; j < DIM; j++)
                {
                    x[i][j] = rand01() * (DIM - 1) + 1e-6 * j;
                }
            }
            lastImprovementIter = t;
        }

        // 定期输出进度
        if (t % 50 == 0 || t == 1)
        {
            double currentElapsed = (double)(clock() - startClock) * 1000.0 / CLOCKS_PER_SEC;
            printf("[Iter %d] Best: %.2f, Time: %.2f ms (%.1f%% of limit)\n",
                   t, destinationFitness, currentElapsed,
                   (currentElapsed / timeLimitMs) * 100.0);
        }

        t++;
        if (t > maxIterations) break;  // 安全限制
    }

    clock_t endClock = clock();
    double elapsed_ms = (double)(endClock - startClock) * 1000.0 / CLOCKS_PER_SEC;

    // 计算最终距离和makespan
    FIT_DATA_TYPE finalActualDistance = 0.0;
    FIT_DATA_TYPE finalMakespan = 0.0;

    if (destinationFitness != FIT_DATA_TYPE_MAX && data != NULL)
    {
        xData *order = (xData *)malloc((DIM - 1) * sizeof(xData));
        for (int i = 1, k = 0; i < DIM; i++, k++)
        {
            order[k].xIndex = i;
            order[k].data = bestPositions[i];
        }
        sortX(order, DIM - 1);

        // 计算距离
        if (DIM > 1)
        {
            finalActualDistance += data->dist[0][order[0].xIndex];
        }
        for (int i = 0; i < DIM - 2; i++)
        {
            finalActualDistance += data->dist[order[i].xIndex][order[i + 1].xIndex];
        }
        if (DIM > 1)
        {
            finalActualDistance += data->dist[order[DIM - 2].xIndex][0];
        }

        // 计算makespan
        FIT_DATA_TYPE currentTime = data->tw[0].earliest;
        FIT_DATA_TYPE speedVal = (speed <= 0) ? 1.0 : (FIT_DATA_TYPE)speed;

        for (int i = 0; i < DIM - 1; i++)
        {
            int from = (i == 0) ? 0 : order[i - 1].xIndex;
            int to = order[i].xIndex;
            currentTime += data->dist[from][to] / speedVal;
            if (currentTime < data->tw[to].earliest)
                currentTime = data->tw[to].earliest;
        }

        // 返回仓库
        int lastCustomer = order[DIM - 2].xIndex;
        currentTime += data->dist[lastCustomer][0] / speedVal;
        finalMakespan = currentTime;

        free(order);
    }

    double avgBeamDiversity = (beamSearchExecutions > 0) ?
                              (totalBeamDiversity / beamSearchExecutions) : 0.0;

    printf("\n=== SMA-Beam Algorithm Completed ===\n");
    printf("Total Iterations: %d\n", t - 1);
    printf("Best Fitness: %.2f\n", destinationFitness);
    printf("Final Distance: %.2f\n", finalActualDistance);
    printf("Final Makespan: %.2f\n", finalMakespan);
    printf("Elapsed Time: %.2f ms (%.2f seconds)\n", elapsed_ms, elapsed_ms / 1000.0);
    printf("Beam Search Executions: %d\n", beamSearchExecutions);
    printf("Solutions from Beam: %d\n", solutionsFromBeam);
    printf("Early Stop: %s\n", earlyStopTriggered ? "Yes" : "No");

    // 封装结果
    SMABeamResult *result = (SMABeamResult *)malloc(sizeof(SMABeamResult));
    result->pop = pop;
    result->dimension = DIM;
    result->iterationATime = t - 1;
    result->destinationFitness = destinationFitness;
    result->bestPositions = bestPositions;
    result->bestPositionsStart = bestPositionsStart;
    result->convergenceCurve = convergenceCurve;
    result->finalDistance = finalActualDistance;
    result->finalMakespan = finalMakespan;
    result->elapsedTimeMs = elapsed_ms;
    result->earlyStopTriggered = earlyStopTriggered;
    result->beamSearchExecutions = beamSearchExecutions;
    result->solutionsFromBeam = solutionsFromBeam;
    result->avgBeamDiversity = avgBeamDiversity;

    // 清理
    for (int i = 0; i < pop; i++)
    {
        free(x[i]);
        free(W[i]);
    }
    free(x);
    free(fit);
    free(W);
    freeDataMatrix(data);

    return result;
}

// 简化版本：基于迭代次数
SMABeamResult* SMA_Beam(int pop, int DIM, const FIT_DATA_TYPE *lb, const FIT_DATA_TYPE *ub,
                        const char *dataPath, int speed, BeamConfig beamConfig)
{
    // 转换为基于时间的版本，给一个大的时间限制
    return SMA_Beam_TimeLimited_WithEarlyStop(pop, DIM, lb, ub, dataPath, speed,
                                              3600.0, -1.0, beamConfig);
}

// 简化版本：基于时间限制
SMABeamResult* SMA_Beam_TimeLimited(int pop, int DIM, const FIT_DATA_TYPE *lb, const FIT_DATA_TYPE *ub,
                                    const char *dataPath, int speed, double timeLimitSeconds,
                                    BeamConfig beamConfig)
{
    return SMA_Beam_TimeLimited_WithEarlyStop(pop, DIM, lb, ub, dataPath, speed,
                                              timeLimitSeconds, -1.0, beamConfig);
}
