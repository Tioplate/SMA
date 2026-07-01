#include <stdio.h>
#include <stdlib.h>
#include "readMatrix.h" // 引入用于读取数据文件的函数

/**
 * @brief 独立路径验证程序
 *
 * 功能：
 * 1. 读取一个未经任何修改的原始 TSPLIB 数据文件。
 * 2. 根据给定的节点访问顺序（路径），逐步验证其可行性。
 * 3. 详细输出每一步的到达时间、等待时间、时间窗约束，并判断是否违规。
 * 4. 最终计算总的 Makespan 并给出路径是否完全可行的结论。
 *
 * 如何使用：
 * 1. 在下面的 main 函数中，修改 `dataPath` 为您想验证的数据集文件。
 * 2. 修改 `route` 数组为您想验证的路径（注意：以0开始，以0结束）。
 * 3. 编译并运行此文件。
 *
 *    gcc -o validate_route.exe validate_route.c readMatrix.c
 *    ./validate_route
 */

// 辅助函数：打印分隔线
void print_separator() {
    printf("--------------------------------------------------------------------------------\n");
}

int main() {
    // --- 配置验证参数 ---

    // 1. 指定原始数据文件路径
    const char *dataPath = "dataset/Dumas/n60w60.004.txt";
    const int speed = 1;

    // 2. 指定要验证的路径 (从0开始, 不包含最后的0)
    // 这是从之前日志中提取的、算法认为最优但验证失败的路径
    int route[] = {
        49, 12, 31, 2, 27, 23, 41, 45, 34, 36, 6, 37, 11, 54, 5, 14, 50, 19, 1, 26,
        4, 33, 13, 53, 18, 56, 24, 32, 58, 42, 52, 8, 3, 43, 40, 46, 22, 15, 44, 47, 17,
        21, 29, 20, 51, 48, 25, 7, 10, 39, 9, 57, 55, 35, 30, 38, 16, 60, 59, 28
    };
    int num_customers = sizeof(route) / sizeof(route[0]);

    // --- 开始验证 ---

    printf("Independent Route Validator\n");
    print_separator();
    printf("Dataset: %s\n", dataPath);

    // 读取原始数据，不进行任何预处理
    dataMatrix *data = readMatrix((char*)dataPath);
    if (!data) {
        fprintf(stderr, "Error: Failed to read original data file at '%s'.\n", dataPath);
        return 1;
    }
    if (data->size <= 0) {
        fprintf(stderr, "Error: Data matrix is empty or invalid.\n");
        freeDataMatrix(data);
        return 1;
    }

    printf("Route to validate (Depot -> ... -> Depot):\n0");
    for (int i = 0; i < num_customers; ++i) {
        printf(" -> %d", route[i]);
    }
    printf(" -> 0\n");
    print_separator();

    // 打印表头
    printf("%-5s %-8s %-8s %-10s %-10s %-10s %-10s %-6s\n",
           "Step", "From", "To", "Arrive", "Wait", "[Early", "Late]", "OK?");
    print_separator();

    double currentTime = data->tw[0].earliest;
    double totalWaitTime = 0;
    int isFeasible = 1;
    const double speedVal = (speed <= 0) ? 1.0 : (double)speed;
    const double serviceTime = 0; // 假设服务时间为0，因为数据文件中未提供

    // 遍历路径，从仓库到第一个客户
    int fromNode = 0;
    int toNode = route[0];
    double travelTime = data->dist[fromNode][toNode] / speedVal;
    double arrivalTime = currentTime + travelTime;
    double waitTime = 0;

    if (arrivalTime < data->tw[toNode].earliest) {
        waitTime = data->tw[toNode].earliest - arrivalTime;
        totalWaitTime += waitTime;
        arrivalTime = data->tw[toNode].earliest; // 更新到达时间为最早服务时间
    }

    int ok = (arrivalTime <= data->tw[toNode].latest + 1e-4);
    if (!ok) isFeasible = 0;

    currentTime = arrivalTime + serviceTime; // 更新当前时间为离开客户的时间

    printf("%-5d %-8d %-8d %-10.2f %-10.2f [%-9.2f, %-9.2f] %-6s\n",
           1, fromNode, toNode, arrivalTime, waitTime,
           data->tw[toNode].earliest, data->tw[toNode].latest, ok ? "YES" : "NO");

    // 遍历所有客户
    for (int i = 0; i < num_customers - 1; ++i) {
        fromNode = route[i];
        toNode = route[i+1];
        travelTime = data->dist[fromNode][toNode] / speedVal;
        arrivalTime = currentTime + travelTime;
        waitTime = 0;

        if (arrivalTime < data->tw[toNode].earliest) {
            waitTime = data->tw[toNode].earliest - arrivalTime;
            totalWaitTime += waitTime;
            arrivalTime = data->tw[toNode].earliest; // 更新到达时间为最早服务时间
        }

        ok = (arrivalTime <= data->tw[toNode].latest + 1e-4);
        if (!ok) isFeasible = 0;

        currentTime = arrivalTime + serviceTime;

        printf("%-5d %-8d %-8d %-10.2f %-10.2f [%-9.2f, %-9.2f] %-6s\n",
               i + 2, fromNode, toNode, arrivalTime, waitTime,
               data->tw[toNode].earliest, data->tw[toNode].latest, ok ? "YES" : "NO");
    }

    // 返回仓库
    fromNode = route[num_customers - 1];
    toNode = 0; // Depot
    travelTime = data->dist[fromNode][toNode] / speedVal;
    arrivalTime = currentTime + travelTime;

    ok = (arrivalTime <= data->tw[toNode].latest + 1e-4);
    if (!ok) isFeasible = 0;

    currentTime = arrivalTime; // 最终到达仓库的时间

    printf("%-5d %-8d %-8d %-10.2f %-10.2f [%-9.2f, %-9.2f] %-6s\n",
           num_customers + 1, fromNode, toNode, arrivalTime, 0.0,
           data->tw[toNode].earliest, data->tw[toNode].latest, ok ? "YES" : "NO");

    print_separator();

    // 最终结论
    printf("Validation Complete.\n");
    printf("Final Makespan (Arrival at Depot): %.2f\n", currentTime);
    printf("Total Wait Time: %.2f\n", totalWaitTime);
    printf("Overall Route Feasibility: %s\n", isFeasible ? "FEASIBLE" : "NOT FEASIBLE");
    print_separator();

    // 清理资源
    freeDataMatrix(data);

    return 0;
}
