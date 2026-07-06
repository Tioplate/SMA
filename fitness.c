#include "fitness.h"

#include <string.h>

/*
    Module: Fitness Calculation (TSP/TSPTW)
    - This file implements the example function F1, as well as the TSPTW fitness with hard time windows.
    - The position encoding uses "priority encoding": sorting the real keys of each dimension to determine the visitation order.
    - In TSPTW:
        * The 0th node is regarded as the depot (fixed start and end point);
        * Only the customer nodes (1..dim-1) are sorted by key value to determine the visitation order;
        * The time windows are hard constraints:
            - Arriving earlier than the earliest time allows waiting;
            - Arriving later than the latest time is directly judged as infeasible (returns FIT_DATA_TYPE_MAX);
        * The objective function is the "total travel distance" (excluding waiting time), with the return distance included in the objective but usually not checking the return window;
        * Speed is used to convert distance to travel time for feasibility judgment of the time window.
*/

/*
    F1 function: Example fitness, fitness == sum(x^2)
    Only for testing and comparison.
*/
FIT_DATA_TYPE F1(FIT_DATA_TYPE *x, int dim)
{
    FIT_DATA_TYPE fitness = 0;
    for (int i = 0; i < dim; i++)
    {
        fitness += x[i] * x[i];
    }
    return fitness;
}

/*
    Sorting comparison function: Ascending order by data
*/
int compareFunctionX(const void *a, const void *b)
{
    FIT_DATA_TYPE da = ((const xData *)a)->data;
    FIT_DATA_TYPE db = ((const xData *)b)->data;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

/*
    sortX: Sorts the xData array in ascending order by data
*/
xData *sortX(xData *order, int dim)
{
    qsort(order, dim, sizeof(xData), compareFunctionX);
    return order;
}

/*
    adjustPostion: Example function
*/
FIT_DATA_TYPE **adjustPostion(FIT_DATA_TYPE **x, int pop, int dim)
{
    for (int i = 0; i < pop; i++)
    {
        xData *order = (xData *)malloc(dim * sizeof(xData));
        for (int j = 0; j < dim; j++)
        {
            order[j].xIndex = j;
            order[j].data = x[i][j];
        }
        order = sortX(order, dim);
        free(order);
    }
    return x;
}

/*
    Comparison function: Ascending order by earliest
*/
typedef struct {
    int customerIdx;
    double earliest;
} CustomerTW;

static int cmpCustomerTW(const void *a, const void *b)
{
    double da = ((const CustomerTW *)a)->earliest;
    double db = ((const CustomerTW *)b)->earliest;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

/*
    动态贪心修复函数：综合考虑空间距离、等待时间和时间窗违约
*/
void repairSolutionGreedy(FIT_DATA_TYPE *x, int dim, dataMatrix *routeData, FIT_DATA_TYPE speed)
{
    if (!routeData || dim <= 1) return;

    int *visited = (int *)calloc(dim, sizeof(int));
    visited[0] = 1; // 0 是车库起点，已访问
    int current_node = 0;
    double current_time = routeData->tw[0].earliest;
    double spd = (speed <= 0) ? 1.0 : speed;

    int *order = (int *)malloc(dim * sizeof(int));
    order[0] = 0;

    for (int step = 1; step < dim; step++)
    {
        int best_next = -1;
        double best_cost = 1e18; // 寻找最小代价

        for (int j = 1; j < dim; j++)
        {
            if (visited[j]) continue;

            double arr_time = current_time + routeData->dist[current_node][j] / spd;
            double wait_time = 0;

            // 如果早到，允许等待
            if (arr_time < routeData->tw[j].earliest) {
                wait_time = routeData->tw[j].earliest - arr_time;
                arr_time = routeData->tw[j].earliest;
            }

            // 计算时间窗违规程度
            double time_violation = 0;
            if (arr_time > routeData->tw[j].latest) {
                time_violation = arr_time - routeData->tw[j].latest;
            }

            // 代价函数：违规绝对优先被惩罚，其次考虑行驶距离和等待时间
            double cost = time_violation * 100000.0 + routeData->dist[current_node][j] + wait_time * 0.1;

            // 引入 0.8 ~ 1.2 的随机扰动因子，保留种群多样性
            double rand_factor = 0.8 + 0.4 * ((double)rand() / RAND_MAX);
            cost *= rand_factor;

            if (cost < best_cost) {
                best_cost = cost;
                best_next = j;
            }
        }

        if (best_next == -1) break; // 理论上不会发生

        order[step] = best_next;
        visited[best_next] = 1;

        // 更新当前时间与当前节点
        double arr_time = current_time + routeData->dist[current_node][best_next] / spd;
        if (arr_time < routeData->tw[best_next].earliest) arr_time = routeData->tw[best_next].earliest;
        current_time = arr_time;
        current_node = best_next;
    }

    // 将生成的合法（或最接近合法）的顺序转码为优先级键值返回给 SMA
    x[0] = 0;
    for (int i = 1; i < dim; i++)
    {
        int custIdx = order[i];
        x[custIdx] = (FIT_DATA_TYPE)i + 1e-6 * i;
    }

    free(visited);
    free(order);
}

/*
    TSPTW with repair version
*/
FIT_DATA_TYPE TSPTW_WithRepair(FIT_DATA_TYPE *x, int dim, FIT_DATA_TYPE speed, dataMatrix *routeData)
{
    FIT_DATA_TYPE fitness = TSPTW(x, dim, speed, routeData);
    if (fitness > 10000.0 && routeData != NULL)
    {
        repairSolutionGreedy(x, dim, routeData, speed);
        fitness = TSPTW(x, dim, speed, routeData);
    }
    return fitness;
}

/*
    TSPTW (soft time window penalty)
*/
FIT_DATA_TYPE TSPTW(FIT_DATA_TYPE *x, int dim, FIT_DATA_TYPE speed, dataMatrix *routeData)
{
    if (dim <= 1)
        return 0;

    int customerCount = dim - 1;
    xData *order = (xData *)malloc(customerCount * sizeof(xData));

    for (int i = 1; i < dim; i++)
    {
        order[i - 1].xIndex = i;
        order[i - 1].data   = x[i];
    }
    sortX(order, customerCount);

    FIT_DATA_TYPE currentTime   = 0.0;
    FIT_DATA_TYPE totalDistance = 0.0;
    FIT_DATA_TYPE totalOvertime = 0.0;
    FIT_DATA_TYPE startTime, endTime, travelTime, distance;

    // Depot departure
    startTime = routeData->tw[0].earliest;
    endTime   = routeData->tw[0].latest;
    if (currentTime < startTime) currentTime = startTime;
    if (currentTime > endTime)   totalOvertime += (currentTime - endTime);

    // Depot -> first customer
    {
        int first = order[0].xIndex;
        distance   = routeData->dist[0][first];
        travelTime = distance / speed;
        currentTime   += travelTime;
        totalDistance += distance;
        // Penalize invalid edge
        if (routeData->E && !routeData->E[0][first])
            totalOvertime += 1e8;
        startTime = routeData->tw[first].earliest;
        endTime   = routeData->tw[first].latest;
        if (currentTime > endTime)   totalOvertime += (currentTime - endTime);
        if (currentTime < startTime) currentTime = startTime;
    }

    // Between customers
    for (int i = 0; i < customerCount - 1; i++)
    {
        int from = order[i].xIndex;
        int to   = order[i + 1].xIndex;
        distance   = routeData->dist[from][to];
        travelTime = distance / speed;
        // Penalize invalid edge
        if (routeData->E && !routeData->E[from][to])
            totalOvertime += 1e8;
        startTime = routeData->tw[to].earliest;
        endTime   = routeData->tw[to].latest;
        currentTime   += travelTime;
        totalDistance += distance;
        if (currentTime > endTime)   totalOvertime += (currentTime - endTime);
        if (currentTime < startTime) currentTime = startTime;
    }

    // Last customer -> depot
    {
        int last = order[customerCount - 1].xIndex;
        distance   = routeData->dist[last][0];
        travelTime = distance / speed;
        currentTime   += travelTime;
        totalDistance += distance;
        // Penalize invalid edge
        if (routeData->E && !routeData->E[last][0])
            totalOvertime += 1e8;
        endTime = routeData->tw[0].latest;
        if (currentTime > endTime) totalOvertime += (currentTime - endTime);
    }

    free(order);
    return currentTime + 10000.0 * totalOvertime;
}

/*
    Four-step time window tightening rules (constraint propagation preprocessing)
    By repeatedly applying the following four rules, tighten the time window [e_k, l_k] of each node,
    until no changes occur (fixed point), thus pruning invalid edges.

    Rule 1 (Forward push of e_k):
        e_k = max(e_k, min_{(i,k)∈E} (e_i + dist[i][k]/speed))
    Rule 2 (Backward push of e_k):
        e_k = max(e_k, min_{(k,i)∈E} (e_i - dist[k][i]/speed))
    Rule 3 (Backward pull of l_k):
        l_k = min(l_k, max_{(k,i)∈E} (l_i - dist[k][i]/speed))
    Rule 4 (Forward pull of l_k):
        l_k = min(l_k, max_{(i,k)∈E} (l_i + dist[i][k]/speed))  (Note: using predecessor's l_i)

    An edge (i,k) belongs to the valid edge set E if and only if:
        e_i + dist[i][k]/speed <= l_k  （Can arrive at k before the deadline from i）
*/
void tightenTimeWindows(dataMatrix *data, double speed)
{
    if (!data || data->size < 2 || speed <= 0) return;

    int n = data->size; // 节点总数（包含仓库 0）
    int changed = 1;
    int maxPasses = 100; // 防止死循环

    while (changed && maxPasses-- > 0)
    {
        changed = 0;

        // 注意：严格跳过 k = 0 (车库)。车库的时间窗代表全局起止客观约束，不能被动态压缩！
        for (int k = 1; k < n; k++)
        {
            double orig_ek = data->tw[k].earliest;
            double orig_lk = data->tw[k].latest;

            double ek = orig_ek;
            double lk = orig_lk;

            // === 第一步：基于前驱推迟最早时间 (Forward push of e_k) ===
            {
                double minArrival = 1e18;
                int hasPred = 0;
                for (int i = 0; i < n; i++)
                {
                    if (i == k || !data->E[i][k]) continue;
                    double arrival = data->tw[i].earliest + data->dist[i][k] / speed;
                    hasPred = 1;
                    if (arrival < minArrival) minArrival = arrival;
                }
                if (hasPred && minArrival > ek) ek = minArrival;
            }

            // === 第二步：基于后继推迟最早时间 (Backward push of e_k) ===
            {
                double minDep = 1e18;
                int hasSucc = 0;
                for (int i = 0; i < n; i++)
                {
                    if (i == k || !data->E[k][i]) continue;
                    hasSucc = 1;
                    double depTime = data->tw[i].earliest - data->dist[k][i] / speed;
                    if (depTime < minDep) minDep = depTime;
                }
                if (hasSucc && minDep > ek) ek = minDep;
            }

            // 安全裁剪，写入回 e_k
            if (ek > lk) ek = lk;
            data->tw[k].earliest = ek;

            // === 第三步：基于后继提前最晚时间 (Backward pull of l_k) ===
            {
                double maxLatDep = -1e18;
                int hasSucc = 0;
                for (int i = 0; i < n; i++)
                {
                    if (i == k || !data->E[k][i]) continue;
                    hasSucc = 1;
                    double latDep = data->tw[i].latest - data->dist[k][i] / speed;
                    if (latDep > maxLatDep) maxLatDep = latDep;
                }
                if (hasSucc && maxLatDep < lk) lk = maxLatDep;
            }

            // === 第四步：基于前驱提前最晚时间 (Forward pull of l_k) ===
            {
                double maxLatArr = -1e18;
                int hasPred = 0;
                for (int i = 0; i < n; i++)
                {
                    if (i == k || !data->E[i][k]) continue;
                    hasPred = 1;
                    double latArr = data->tw[i].latest + data->dist[i][k] / speed;
                    if (latArr > maxLatArr) maxLatArr = latArr;
                }
                if (hasPred && maxLatArr < lk) lk = maxLatArr;
            }

            // 安全裁剪：l_k 至少大于等于 e_k
            if (lk < ek) lk = ek;
            data->tw[k].latest = lk;

            // 仅在真实时间窗边界发生有效改变时，才触发下一轮紧缩
            if ((ek - orig_ek) > 1e-6 || (orig_lk - lk) > 1e-6)
            {
                changed = 1;
            }
        }
    }

    printf("[TW Tightening] Done (%d passes used).\n", 100 - maxPasses);
}

/*
    Three-step network pruning rules (filterNetwork)
    Called after tightenTimeWindows, to prune and update the valid edge set E and precedence constraint set R.
*/
void filterNetwork(dataMatrix *data, double speed)
{
    if (!data || data->size < 2 || speed <= 0 || !data->R || !data->E) return;

    int n = data->size;
    double **dist = data->dist;
    TimeWindow *tw = data->tw;
    int **R = data->R;
    int **E = data->E;

    // =========================================================
    // 第一步：直接超时断边
    // =========================================================
    int removed1 = 0;
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            if (i == j || !E[i][j]) continue;

            if (tw[i].earliest + dist[i][j] / speed > tw[j].latest)
            {
                E[i][j] = 0;
                // 注意：只收集纯客户点之前的绝对顺序 R
                if (i > 0 && j > 0) {
                    R[j][i] = 1;
                }
                removed1++;
            }
        }
    }
    printf("[Filter Step1] Removed %d timeout edges\n", removed1);

    // =========================================================
    // 第二步：三节点子路径校验 (Subpath checking)
    // 根据最短时间理论限定，去除由于要求 E[j][k] 闭包连通性导致的误伤雪崩
    // =========================================================
    int removed2 = 0;
    int added2 = 0;
    if (n > 3)
    {
        for (int i = 0; i < n; i++)
        {
            for (int j = 0; j < n; j++)
            {
                if (i == j || !E[i][j]) continue;

                // k 必须是实际客户点，不可以是 0 号点车库
                for (int k = 1; k < n; k++)
                {
                    if (k == i || k == j) continue;

                    // 检查理论下限约束：(i,j,k) 是否可行
                    int path_ijk = 0;
                    double tJ = tw[i].earliest + dist[i][j] / speed;
                    if (tJ < tw[j].earliest) tJ = tw[j].earliest;
                    if (tJ <= tw[j].latest)
                    {
                        double tK = tJ + dist[j][k] / speed;
                        if (tK <= tw[k].latest) path_ijk = 1;
                    }

                    // 检查理论下限约束：(k,i,j) 是否可行
                    int path_kij = 0;
                    double tI = tw[k].earliest + dist[k][i] / speed;
                    if (tI < tw[i].earliest) tI = tw[i].earliest;
                    if (tI <= tw[i].latest)
                    {
                        double tJ2 = tI + dist[i][j] / speed;
                        if (tJ2 <= tw[j].latest) path_kij = 1;
                    }

                    // 动作1：若 k 无论是作为前置还是后置都彻底不可行，说明 i 和 j 之间绝不可能直接连接
                    if (!path_ijk && !path_kij)
                    {
                        E[i][j] = 0;
                        removed2++;

                        // 动作2：连带检查 (i,k,j) 这条非直接子路径是否也可行
                        int path_ikj = 0;
                        double tK2 = tw[i].earliest + dist[i][k] / speed;
                        if (tK2 < tw[k].earliest) tK2 = tw[k].earliest;
                        if (tK2 <= tw[k].latest)
                        {
                            double tJ3 = tK2 + dist[k][j] / speed;
                            if (tJ3 <= tw[j].latest) path_ikj = 1;
                        }

                        // 如果连阻挡点 k 插在中间都不理会时间允许范围内，且双方都为客户，则强制记录前向顺序
                        if (!path_ikj && i > 0 && j > 0)
                        {
                            if (!R[j][i]) {
                                R[j][i] = 1;
                                added2++;
                            }
                        }

                        break;
                    }
                }
            }
        }
    }
    printf("[Filter Step2] Removed %d edges, Added %d R via subpath check\n", removed2, added2);

    // =========================================================
    // 第三步：推移闭包 (Transitive Closure) 与 逆向剔除
    // =========================================================
    int closureAdded = 0;
    int removeByDirect = 0;

    // 绝对先后顺序仅在客户点之间有意义 (1~n-1)，禁止涉及 0 点闭包引发冲突
    for (int k = 1; k < n; k++)
    {
        for (int i = 1; i < n; i++)
        {
            if (!R[i][k]) continue;
            for (int j = 1; j < n; j++)
            {
                if (i == j || !R[k][j]) continue;

                if (!R[i][j])
                {
                    R[i][j] = 1;
                    closureAdded++;
                }

                if (E[i][j])
                {
                    E[i][j] = 0;
                    removeByDirect++;
                }
            }
        }
    }

    int removed3 = 0;
    for (int i = 1; i < n; i++)
    {
        for (int j = 1; j < n; j++)
        {
            if (i == j) continue;
            if (R[i][j])
            {
                if (E[j][i])
                {
                    E[j][i] = 0;
                    removed3++;
                }
            }
        }
    }
    printf("[Filter Step3] Added %d closure R, Removed %d by intermediate, %d reverse edges\n", closureAdded, removeByDirect, removed3);
    printf("[Filter Total] Total valid edges removed: %d\n", removed1 + removed2 + removeByDirect + removed3);
}
