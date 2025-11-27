// filepath: d:\mypro\SMA\SMA_Back\sma.c
#include "sma.h"

/*
=================================================================================
    黏菌算法（Slime Mould Algorithm, SMA）求解TSPTW问题
=================================================================================

【核心编码方案】
  - 优先级编码（Priority-based Encoding）：
    * 每个个体 x[i] 是一个长度为 DIM 的实数数组（优先级键）
    * x[i][0] 对应仓库（depot），x[i][1..DIM-1] 对应客户节点
    * 解码：将客户键 x[i][1..DIM-1] 按值升序排序，得到访问顺序
    * 例如：keys = [_, 3.5, 1.2, 7.8] → 访问顺序为 0 -> 客户2 -> 客户1 -> 客户3 -> 0

【TSPTW问题特性】
  - 目标：最小化总行驶距离（闭合路线：0 -> 客户们 -> 0）
  - 约束：硬时间窗（到达早于 earliest 可等待，晚于 latest 直接不可行）
  - 难点：优先级键的微小变化会导致排序剧变，极易违反时间窗

【原始SMA的问题】
  - 盲目朝 bestPositions 靠拢 → 轻微扰动就违反时间窗 → 99% 更新产生不可行解
  - 算法困在"不可行解空间"打转，全局最优一直不更新
  - 只有末期随机重启才偶然找到新可行解（非正常优化行为）

【本实现的改进机制】
  1. 时间窗启发式初始化：前3个个体按 earliest/latest/mid 排序，提高初始可行解概率
  2. 可行解保持策略：每次更新后立即评估，若新解不可行但旧解可行则回退
  3. 保守步长：vb 分支步长缩小到 10%，vc 分支到 5%，避免过度扰动
  4. 提高探索下限：全局探索概率最低 10%（原 3%），增强跳出局部最优能力
  5. 停滞重启机制：连续无改进 patience 代后，重启底部 30% 个体
  6. 不可行解主动注入：新旧都不可行时，50% 概率注入 earliest 启发式
  7. 键交换微扰：对底部 40% 个体以 20% 概率做小幅键值交换，改变排序但保持值域

【结果】
  - 修复后：从第2个里程碑（~10% 迭代）就开始持续改进，整个过程平滑收敛
  - 修复前：前 80% 迭代完全卡死，只有末期才突然跳变
=================================================================================
*/

// 说明：
// 本文件实现黏菌算法（SMA）的主流程与若干工具函数。
// 核心思想：
//  - 使用“优先级编码”表示 TSPTW 解：对每个维度的实数键排序得到客户访问顺序（仓库固定0）。
//  - 适应度函数 TSPTW 使用硬时间窗（早到等待，迟到不可行），目标为总行驶距离。
//  - SMA 通过权重 W、vb/vc 两类更新与概率 p 来引导个体向更优位置移动。
//  - 若某代全体不可行，则对人群进行重采样并注入时间窗启发式，提高获得可行解的概率。

// 将区间 [minV, maxV] 线性映射到 [0, dim-1] 并做边界夹取（用于构造启发式优先级键）
static inline FIT_DATA_TYPE normalize_range(double v, double minV, double maxV, int dim)
{
    double denom = (maxV - minV);
    if (denom <= 1e-9) return
    0.0;
    double t = (v - minV) / denom;
    if (t < 0) t = 0; if (t > 1) t = 1;
    return (FIT_DATA_TYPE)(t * (dim - 1));
}

// 针对优先级键的微扰：在客户键 1..dim-1 范围内随机交换若干对键值，改变访问顺序（文件作用域定义）
static inline void smallKeySwaps(FIT_DATA_TYPE *keys, int dim, int swaps)
{
    if (!keys || dim <= 2 || swaps <= 0) return;
    for (int s = 0; s < swaps; ++s)
    {
        int u = 1 + rand() % (dim - 1);
        int v = 1 + rand() % (dim - 1);
        if (u == v) continue;
        FIT_DATA_TYPE tmp = keys[u];
        keys[u] = keys[v];
        keys[v] = tmp;
    }
}

// 新增：从优先级键构造闭合路线（0 -> 客户排序 -> 0）用于打印/调试
// 返回一个长度为 (dim + 1) 的数组，元素为节点索引（以 FIT_DATA_TYPE 存放整数索引以方便打印）
static FIT_DATA_TYPE *buildRouteFromKeys(const FIT_DATA_TYPE *keys, int dim)
{
    if (dim <= 0) return NULL;
    // dim 包含仓库 0，客户为 1..dim-1
    FIT_DATA_TYPE *route = (FIT_DATA_TYPE *)malloc((dim + 1) * sizeof(FIT_DATA_TYPE));
    if (!route) return NULL;
    // 若 keys 为空，释放并返回 NULL
    if (!keys)
    {
        free(route);
        return NULL;
    }
    // 若没有客户，直接 0->0
    if (dim == 1)
    {
        route[0] = 0;
        route[1] = 0;
        return route;
    }
    // 准备排序用的临时数组，仅包含客户
    xData *order = (xData *)malloc((dim - 1) * sizeof(xData));
    if (!order)
    {
        free(route);
        return NULL;
    }
    for (int i = 1, k = 0; i < dim; i++, k++)
    {
        order[k].xIndex = i;
        order[k].data = keys[i];
    }
    sortX(order, dim - 1);

    // 填充闭合路线
    int p = 0;
    route[p++] = 0; // 起点仓库
    for (int i = 0; i < dim - 1; i++)
    {
        route[p++] = (FIT_DATA_TYPE)order[i].xIndex;
    }
    route[p++] = 0; // 终点回仓

    free(order);
    return route;
}

/*
    initialization：
    - 生成 pop 个个体，每个个体为长度 DIM 的实数数组（优先级键）。
    - 采用连续随机键，并加入很小的 j 级偏移，减少相等键导致的排序退化。
    - 注意：x[0] 对应仓库键，当前实现中仍会被赋值，但 TSPTW 排序时只使用 1..DIM-1。
*/
FIT_DATA_TYPE **initialization(int pop, int DIM)
{
    FIT_DATA_TYPE **x;
    x = (FIT_DATA_TYPE **)malloc(pop * sizeof(FIT_DATA_TYPE *));
    srand((unsigned)time(NULL)); // 简单随机种子，若需复现实验可改为固定种子
    for (int i = 0; i < pop; i++)
    {
        x[i] = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
        for (int j = 0; j < DIM; j++)
        {
            // 连续随机键，减少大量相等键导致的退化
            x[i][j] = (FIT_DATA_TYPE)rand01() * (DIM - 1) + 1e-6 * j;
        }
    }
    return x;
}
/*
    compare function of fitness, order by ascending
    - 适应度越小越好（总距离越小越优）。
*/
int compareFunction(const void *a, const void *b)
{
    return ((*(fitnessData *)a).fitness > (*(fitnessData *)b).fitness ? 1 : -1);
}

/* sortFitness：按适应度升序排序（原地） */
fitnessData *sortFitness(fitnessData *fit, int size)
{
    qsort(fit, size, sizeof(fit[0]), compareFunction);
    return fit;
}

/*
    sortIndex：
    - 按排序后的适应度顺序重排种群数组指针（原地操作，不释放传入的x）。
    - 使用临时数组暂存重排后的指针，再拷贝回原数组。
*/
FIT_DATA_TYPE **sortIndex(FIT_DATA_TYPE **x, fitnessData *fit, int pop)
{
    FIT_DATA_TYPE **xNew;
    xNew = (FIT_DATA_TYPE **)malloc(pop * sizeof(FIT_DATA_TYPE *));
    for (int i = 0; i < pop; i++)
    {
        xNew[i] = x[fit[i].popIndex];
    }
    // 拷贝回原数组
    for (int i = 0; i < pop; i++)
    {
        x[i] = xNew[i];
    }
    free(xNew); // 仅释放临时数组，不释放x
    return x;   // 返回原指针
}

/*
    SMA：算法主循环
    输入：
      - pop：种群规模
      - DIM：维度/节点数
      - lb/ub：变量上下界（对“优先级编码”仅用于产生扰动与重采样，不做硬裁剪）
      - dataPath：数据文件路径，用于读取距离矩阵与时间窗
      - speed：行驶速度（影响可行性判定，非目标值）
    输出：
      - SMAResult 包含最优适应度、对应位置（排序前键）与收敛曲线

    主要步骤：
      1) 初始化种群；
      2) 注入时间窗启发式种子（earliest / latest / mid），提高可行解概率；
      3) 评估适应度，记录当前全局最优；
      4) 迭代 T 代：
         - 对适应度排序并重排种群；
         - 若本代最优为不可行（DBL_MAX），对人群重采样并再注入启发式；
         - 否则计算权重 W，使用公式2/3 更新（vb/vc）；
         - 重新评估适应度并更新全局最优；
      5) 输出结果并释放内存。
*/
SMAResult* SMA(int pop, int DIM, const FIT_DATA_TYPE *lb, const FIT_DATA_TYPE *ub, const char *dataPath, int speed)
{
    FIT_DATA_TYPE **x = initialization(pop, DIM);              // pop initialization
    fitnessData *fit;                                     // 种群个体适应度数组
    FIT_DATA_TYPE *bestPositions;                         //(当前)最优解下X取值的数组（优先级键）
    FIT_DATA_TYPE *bestPositionsStart;                     // 最初始最优解下X取值的数组（优先级键）
    FIT_DATA_TYPE destinationFitness = FIT_DATA_TYPE_MAX; // 当前最佳适应度
    FIT_DATA_TYPE *convergenceCurve;                      // 收敛曲线
    FIT_DATA_TYPE **W;                                    // 黏菌权重矩阵
    FIT_DATA_TYPE bestFitness;                            // 当代最佳适应度
    // 将秒级计时改为毫秒级：使用 clock() 记录 CPU 时间
    clock_t startClock, endClock;                         // 计时（CPU clock）
    dataMatrix *data = readMatrix((char*)dataPath);              // 读取数据

    fit = (fitnessData *)malloc(pop * sizeof(fitnessData));
    convergenceCurve = (FIT_DATA_TYPE *)malloc((T) * sizeof(FIT_DATA_TYPE));
    W = (FIT_DATA_TYPE **)malloc(pop * sizeof(FIT_DATA_TYPE *));
    bestPositions = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
    bestPositionsStart = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
    for (int i = 0; i < pop; i++)
    {
        W[i] = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
    }

    // 以时间窗启发式对前几个个体进行种子初始化，提升可行解概率
    if (data && data->size >= DIM)
    {
        // 统计早窗/晚窗范围（跳过仓库0）
        double minEar = 1e18, maxEar = -1e18;
        double minLat = 1e18, maxLat = -1e18;
        for (int j = 1; j < DIM; j++)
        {
            if (data->tw[j].earliest < minEar) minEar = data->tw[j].earliest;
            if (data->tw[j].earliest > maxEar) maxEar = data->tw[j].earliest;
            if (data->tw[j].latest < minLat) minLat = data->tw[j].latest;
            if (data->tw[j].latest > maxLat) maxLat = data->tw[j].latest;
        }
        // 个体0：按 earliest 升序映射为优先级键
        for (int j = 0; j < DIM; j++) x[0][j] = 0;
        for (int j = 1; j < DIM; j++)
            x[0][j] = normalize_range(data->tw[j].earliest, minEar, maxEar, DIM) + 1e-6 * j;
        // 个体1：按 latest 升序
        if (pop > 1)
        {
            for (int j = 0; j < DIM; j++) x[1][j] = 0;
            for (int j = 1; j < DIM; j++)
                x[1][j] = normalize_range(data->tw[j].latest, minLat, maxLat, DIM) + 1e-6 * j;
        }
        // 个体2：按 (earliest+latest)/2 升序
        if (pop > 2)
        {
            for (int j = 0; j < DIM; j++) x[2][j] = 0;
            for (int j = 1; j < DIM; j++)
            {
                double mid = 0.5 * (data->tw[j].earliest + data->tw[j].latest);
                x[2][j] = normalize_range(mid, minEar, maxEar, DIM) + 1e-6 * j;
            }
        }
    }

    // 初始评估
    for (int i = 0; i < pop; i++)
    {
        fit[i].popIndex = i;
        fit[i].fitness = TSPTW(x[i], DIM, speed, data);
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
    startClock = clock();

    // 预设打印里程碑：等间隔打印10次最佳fitness（包含初始迭代1与最后迭代T）
    const int milestoneCount = 10;
    int milestones[milestoneCount];
    milestones[0] = 1;            // 第一次迭代
    milestones[milestoneCount - 1] = T; // 最后一次迭代
    // 中间里程碑均匀分布在 (1, T) 之间
    for (int mi = 1; mi < milestoneCount - 1; ++mi)
    {
        double ratio = (double)mi / (double)(milestoneCount - 1); // 1/(n-1), 2/(n-1), ...
        int val = (int)round(1 + ratio * (T - 1)); // 映射到[1,T]
        if (val <= milestones[mi - 1]) val = milestones[mi - 1] + 1; // 保证严格递增
        if (val > T - 1) val = T - 1; // 避免与最后T冲突
        milestones[mi] = val;
    }
    // 若因为校正出现倒序或越界，再做一次线性拉伸（简单保障）
    for (int mi = 1; mi < milestoneCount; ++mi)
    {
        if (milestones[mi] <= milestones[mi - 1])
        {
            milestones[mi] = milestones[mi - 1] + 1;
            if (milestones[mi] > T) milestones[mi] = T; // 最后可能与T重合，无妨
        }
    }
    int nextMilestoneIndex = 0;

    // 停滞检测与临时探索增强参数
    int lastImprovementIter = 0;                 // 最近一次全局最优提升的迭代号
    int patience = (T >= 50) ? (T / 10) : 5;     // 停滞阈值：默认10% T 或至少5代
    if (patience < 3) patience = 3;
    int boostCounter = 0;                        // 提升探索概率的剩余轮数
    const double zBase = Z;                      // 基础探索概率


    while (t <= T)
    {
        // 对适应度值排序并同步重排个体
        fit = sortFitness(fit, pop);
        x = sortIndex(x, fit, pop);
        // 排序后对齐索引：本代后续均以 i 为该个体的新索引
        for (int i = 0; i < pop; ++i) {
            fit[i].popIndex = i;
        }
        bestFitness = fit[0].fitness;
        FIT_DATA_TYPE worstFitness = fit[pop - 1].fitness;

        int skipUpdate = 0;
        if (bestFitness == FIT_DATA_TYPE_MAX)
        {
            // 本代无可行解：一半随机重采样 + 一半时间窗启发式
            for (int i = 0; i < pop; i++)
            {
                for (int j = 0; j < DIM; j++)
                {
                    x[i][j] = (FIT_DATA_TYPE)rand01() * (DIM - 1) + 1e-6 * j;
                }
            }
            if (data && data->size >= DIM)
            {
                double minEar = 1e18, maxEar = -1e18;
                for (int j = 1; j < DIM; j++)
                {
                    if (data->tw[j].earliest < minEar) minEar = data->tw[j].earliest;
                    if (data->tw[j].earliest > maxEar) maxEar = data->tw[j].earliest;
                }
                for (int i = 0; i < pop/2; i++)
                {
                    x[i][0] = 0;
                    for (int j = 1; j < DIM; j++)
                        x[i][j] = normalize_range(data->tw[j].earliest, minEar, maxEar, DIM) + 1e-6 * j;
                }
            }
            skipUpdate = 1; // 本轮不做基于 W 的位置更新，直接进入下一次评估
        }

        if (!skipUpdate)
        {
            // S = (worst - best) + 极小量，保证为正，缩放稳定在[0,1]
            FIT_DATA_TYPE S = (worstFitness - bestFitness) + 1e-8;
            if (S < 1e-12) S = 1e-12; // 额外保护
            // 更新黏菌权重：注意按排序后的 i 写入，避免索引错位
            for (int i = 0; i < pop; i++)
            {
                for (int j = 0; j < DIM; j++)
                {
                    if (fit[i].fitness == FIT_DATA_TYPE_MAX)
                    {
                        W[i][j] = 0.0; // 不可行个体不给权重
                        continue;
                    }
                    FIT_DATA_TYPE numer = (fit[i].fitness - bestFitness);
                    if (numer < 0) numer = 0; // 稳健保护
                    FIT_DATA_TYPE frac = numer / S; // ∈[0, +∞)，通常在[0,1]
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
            // 求 vb、vc 中参数 a, b（注意使用浮点除法），并给 b 下界，避免 b->0 导致解塌缩
            FIT_DATA_TYPE tt = -((FIT_DATA_TYPE)t / (FIT_DATA_TYPE)T) + 1;
            FIT_DATA_TYPE a, b;
            if (tt > -1 && tt < 1)
            {
                a = atanh(tt);
            }
            else
            {
                a = 1; // 极端情况下给一个合理常数
            }
            b = 1 - (FIT_DATA_TYPE)t / (FIT_DATA_TYPE)T;
            if (b < 1e-3) b = 1e-3; // 防止后期过度收缩

            // 当前迭代的探索概率（如触发boost则临时增大）
            double zNow = zBase;
            if (boostCounter > 0)
            {
                double zBoost = zBase * 5.0;
                if (zBoost > 0.3) zBoost = 0.3; // 上限，避免完全随机
                zNow = zBoost;
                boostCounter--;
            }

            // 位置更新：公式2/3，改为"可行解保持"策略
            for (int i = 0; i < pop; i++)
            {
                // 保存更新前的位置（用于回退）
                FIT_DATA_TYPE *xOld = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
                for (int j = 0; j < DIM; j++) xOld[j] = x[i][j];
                FIT_DATA_TYPE oldFitness = fit[i].fitness;

                // 公式2：以概率 zNow 进行全局随机探索（提高探索概率）
                double explorationProb = (zNow > 0.1) ? zNow : 0.1; // 最低10%探索
                if (rand01() < explorationProb)
                {
                    for (int j = 0; j < DIM; j++)
                    {
                        FIT_DATA_TYPE newv = (FIT_DATA_TYPE)rand01() * (DIM - 1) + 1e-6 * j;
                        newv += ((FIT_DATA_TYPE)rand01() * 2.0 - 1.0) * 1e-3;
                        if (newv < 0) newv = 0; else if (newv > (DIM - 1)) newv = (DIM - 1);
                        x[i][j] = newv;
                    }
                }
                // 公式3：开发式搜索，但步长更保守
                else
                {
                    FIT_DATA_TYPE p = tanh(fabs(fit[i].fitness - destinationFitness));
                    FIT_DATA_TYPE *vb = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
                    FIT_DATA_TYPE *vc = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
                    for (int j = 0; j < DIM; j++)
                    {
                        FIT_DATA_TYPE r = rand01();
                        int A, B;
                        if (rand01() < 0.7) {
                            A = rand() % (pop / 2 ? pop / 2 : 1);
                            B = (pop / 2) + (rand() % (pop - (pop / 2) ? (pop - (pop / 2)) : 1));
                        } else {
                            A = rand() % pop;
                            B = rand() % pop;
                        }
                        if (A == B) B = (B + 1) % pop;
                        vb[j] = 2 * a * rand01() - a;
                        vc[j] = 2 * b * rand01() - b;
                        if (r < p)
                        {
                            // 缩小步长到10%，避免过度扰动导致不可行
                            FIT_DATA_TYPE step = vb[j] * (W[i][j] * x[A][j] - x[B][j]) * 0.1;
                            FIT_DATA_TYPE newv = bestPositions[j] + step;
                            newv += ((FIT_DATA_TYPE)rand01() * 2.0 - 1.0) * 1e-3;
                            if (newv < 0) newv = 0; else if (newv > (DIM - 1)) newv = (DIM - 1);
                            x[i][j] = newv;
                        }
                        else
                        {
                            // 更保守的相对扰动：步长限制在5%
                            FIT_DATA_TYPE newv = x[i][j] + vc[j] * x[i][j] * 0.05;
                            newv += ((FIT_DATA_TYPE)rand01() * 2.0 - 1.0) * 1e-3;
                            if (newv < 0) newv = 0; else if (newv > (DIM - 1)) newv = (DIM - 1);
                            x[i][j] = newv;
                        }
                    }
                    free(vb);
                    free(vc);
                }

                // 立即评估新位置的可行性
                FIT_DATA_TYPE newFitness = TSPTW(x[i], DIM, speed, data);
                // 若新位置不可行且旧位置可行，则回退（保持可行解）
                if (newFitness == FIT_DATA_TYPE_MAX && oldFitness != FIT_DATA_TYPE_MAX)
                {
                    for (int j = 0; j < DIM; j++) x[i][j] = xOld[j];
                }
                // 若新旧都不可行，则50%概率注入时间窗启发式
                else if (newFitness == FIT_DATA_TYPE_MAX && oldFitness == FIT_DATA_TYPE_MAX && rand01() < 0.5)
                {
                    if (data && data->size >= DIM)
                    {
                        double minEar = 1e18, maxEar = -1e18;
                        for (int jj = 1; jj < DIM; jj++)
                        {
                            if (data->tw[jj].earliest < minEar) minEar = data->tw[jj].earliest;
                            if (data->tw[jj].earliest > maxEar) maxEar = data->tw[jj].earliest;
                        }
                        x[i][0] = 0;
                        for (int jj = 1; jj < DIM; jj++)
                            x[i][jj] = normalize_range(data->tw[jj].earliest, minEar, maxEar, DIM) + 1e-6 * jj;
                    }
                }

                free(xOld);
            }
        }

        // 重新评估本代所有个体的适应度并更新全局最优
        int feasibleCount = 0; // 本代可行解数量（仅内部统计）
        for (int i = 0; i < pop; i++)
        {
            // 注意：不再重置popIndex，它应保持为排序后该位置对应的原始索引
            // fit[i].popIndex 在sortIndex后已经正确设置，这里直接用i访问排序后的x[i]
            fit[i].fitness = TSPTW(x[i], DIM, speed, data);
            if (fit[i].fitness != FIT_DATA_TYPE_MAX) feasibleCount++;
            if (fit[i].fitness < destinationFitness)
            {
                destinationFitness = fit[i].fitness;
                for (int j = 0; j < DIM; j++)
                {
                    bestPositions[j] = x[i][j];  // 直接用排序后的x[i]，因为i=0就是当前最优
                }
                lastImprovementIter = t; // 记录改进发生在第 t 代
                // printf("Improved at iter %d: best fitness = %.6f (feasible %d/%d)\n", t, destinationFitness, feasibleCount, pop);
            }
        }
        convergenceCurve[t - 1] = destinationFitness;
        // 只保留里程碑打印，关闭每代可行数量打印
        // if (t != lastImprovementIter) {
        //     printf("Iter %d: feasible %d/%d, best = %.6f\n", t, feasibleCount, pop, destinationFitness);
        // }

        // 若进入停滞期：重启底部个体并临时提升探索概率若干轮
        if (t - lastImprovementIter >= patience)
        {
            int startIdx = (int)(pop * 0.7); // 底部30%
            if (startIdx < 1) startIdx = 1;  // 至少保留最优个体不动
            for (int i = startIdx; i < pop; ++i)
            {
                for (int j = 0; j < DIM; ++j)
                {
                    x[i][j] = (FIT_DATA_TYPE)rand01() * (DIM - 1) + 1e-6 * j;
                }
            }
            // 同时注入一小部分按 earliest 的启发式（若数据可用）
            if (data && data->size >= DIM)
            {
                double minEar = 1e18, maxEar = -1e18;
                for (int j = 1; j < DIM; j++)
                {
                    if (data->tw[j].earliest < minEar) minEar = data->tw[j].earliest;
                    if (data->tw[j].earliest > maxEar) maxEar = data->tw[j].earliest;
                }
                for (int i = startIdx; i < pop; i += 2) // 间隔注入，避免同质化
                {
                    x[i][0] = 0;
                    for (int j = 1; j < DIM; j++)
                        x[i][j] = normalize_range(data->tw[j].earliest, minEar, maxEar, DIM) + 1e-6 * j;
                }
            }
            boostCounter = (T >= 50) ? (T / 20) : 3; // 提升探索概率 5%T 轮或至少3轮
            lastImprovementIter = t; // 重置计时，避免连续触发
        }

        // 在每代末尾对底部人群追加小概率的“键交换”微扰（改变排序但保持值域），增强早期逃逸
        int perturbStart = (int)(pop * 0.6);
        if (perturbStart < 1) perturbStart = 1;
        for (int i = perturbStart; i < pop; ++i)
        {
            if (rand01() < 0.2) // 20% 概率
            {
                smallKeySwaps(x[i], DIM, 2);
            }
        }

        // 迭代里程碑打印：恰当迭代点打印当前最佳fitness
        if (nextMilestoneIndex < milestoneCount && t == milestones[nextMilestoneIndex])
        {
            printf("Milestone %d/%d at iteration %d, best fitness = %.6f\n", nextMilestoneIndex + 1, milestoneCount, t, destinationFitness);
            nextMilestoneIndex++;
        }
        // 原有按频率打印注释保留
        // if (t % 1 == 0) { /* printf("At iteration %d, the best fitness is %lf.\n", t, destinationFitness); */ }
        t += 1;
    }

    endClock = clock();
    double elapsed_ms = (double)(endClock - startClock) * 1000.0 / (double)CLOCKS_PER_SEC;

    // 计算最优解的实际距离（不含超时惩罚）
    FIT_DATA_TYPE finalActualDistance = 0.0;
    {
        // 构造客户排序数组，仅包含 1..DIM-1 节点
        xData *order = (xData *)malloc((DIM - 1) * sizeof(xData));
        int k = 0;
        for (int i = 1; i < DIM; i++)
        {
            order[k].xIndex = i;
            order[k].data = bestPositions[i];
            k++;
        }
        order = sortX(order, DIM - 1);

        // 计算总距离
        // 仓库 -> 第一个客户
        if (DIM > 1)
        {
            int first = order[0].xIndex;
            finalActualDistance += data->dist[0][first];
        }
        // 客户之间
        for (int i = 0; i < DIM - 2; i++)
        {
            int from = order[i].xIndex;
            int to = order[i + 1].xIndex;
            finalActualDistance += data->dist[from][to];
        }
        // 最后一个客户 -> 仓库
        if (DIM > 1)
        {
            int last = order[DIM - 2].xIndex;
            finalActualDistance += data->dist[last][0];
        }

        free(order);
    }

    // NOTE: 不再原地把 bestPositions（优先级键）覆盖为索引，保留 keys 以便外部使用。
    // 使用 buildRouteFromKeys 构造闭合路线用于打印（0 开始、0 结束）
    FIT_DATA_TYPE *route = buildRouteFromKeys(bestPositions, DIM);
    printf("Iteration time: %d.\n", T);
    printf("Done, the best fitness is %lf, time %.3f ms.\n", destinationFitness, elapsed_ms);
    printf("Final actual distance (without penalty): %.0f\n", finalActualDistance);
    if (route)
    {
        printf("Route (0->...->0): [");
        for (int i = 0; i < DIM; i++)
        {
            // route 长度为 DIM+1，前 DIM 元素为 0.. last customer
            printf("%d, ", (int)route[i]);
        }
        printf("%d]\n", (int)route[DIM]);
        free(route);
    }
    // 依然保留并打印原始 bestPositions（优先级键）以便分析
    printf("Best x set: [");
    for (int i = 0; i < DIM - 1; i++)
    {
        printf("%lf, ", bestPositions[i]);
    }
    printf("%lf]\n", bestPositions[DIM - 1]);

    // 资源释放与结果封装
    for (int i = 0; i < pop; i++)
    {
        free(x[i]);
        free(W[i]);
    }
    SMAResult *result = (SMAResult *)malloc(sizeof(SMAResult));
    result->pop = pop;
    result->dimension = DIM;
    result->iterationATime = T;
    result->destinationFitness = destinationFitness;
    result->bestPositions = bestPositions;
    result->bestPositionsStart = bestPositionsStart;
    result->convergenceCurve = convergenceCurve;
    free(x);
    free(fit);
    // bestPositions / convergenceCurve 交由 result 管理
    freeDataMatrix(data);
    free(W);
    return result;
}

/* rand01：返回 [0,1) 的随机小数 */
FIT_DATA_TYPE rand01()
{
    return rand() / (FIT_DATA_TYPE)(RAND_MAX);
}

/*
    sortPostionIndex：
    - 将“优先级键数组”转换为“访问顺序索引数组”，便于打印与验证。
    - 注意：此处是原地回写，返回的 xi 内容被替换为索引序（0..dim-1 的排列）。
*/
FIT_DATA_TYPE *sortPostionIndex(FIT_DATA_TYPE *xi, int dim)
{
    if (dim <= 0)
    {
        return NULL;
    }
    xData *order = (xData *)malloc(dim * sizeof(xData));
    for (int i = 0; i < dim; i++)
    {
        order[i].xIndex = i;
        order[i].data = xi[i];
    }
    order = sortX(order, dim);
    for (int i = 0; i < dim; i++)
    {
        xi[i] = order[i].xIndex;
    }
    free(order);
    return xi;
}