// SMA-Beam 混合算法测试程序
#include "sma_beam.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define POP 200
#define SPEED 1

// 控制是否输出最佳路线详情，注释掉此行即可关闭
//#define PRINT_BEST_ROUTE

double get_expected_makespan(const char* datasetName) {
    FILE* fp = fopen("../dataset/Makespan_Bounds.csv", "r");
    if (!fp) {
        fp = fopen("dataset/Makespan_Bounds.csv", "r");
        if (!fp) {
            return -1.0;
        }
    }
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        char folder[256], file[256], lb[256], ub[256];
        int parsed = sscanf(line, "%[^,],%[^,],%[^,],%[^,]", folder, file, lb, ub);
        if (parsed >= 3) {
            if (strcmp(file, datasetName) == 0) {
                fclose(fp);
                if (strcmp(lb, "*") == 0) return -1.0;
                return atof(lb);
            }
        }
    }
    fclose(fp);
    return -1.0;
}

int main()
{
    printf("===========================================\n");
    printf("  SMA-Beam Hybrid Algorithm Test Program  \n");
    printf("  Testing ALL datasets from Dumas  \n");
    printf("===========================================\n\n");

    // 测试数据集列表 - Dumas 文件夹下的全部数据集
    const char *testDatasets[] = {
        "n100w20.001.txt", "n100w20.002.txt", "n100w20.003.txt", "n100w20.004.txt", "n100w20.005.txt",
        "n100w40.001.txt", "n100w40.002.txt", "n100w40.003.txt", "n100w40.004.txt", "n100w40.005.txt",
        "n100w60.001.txt", "n100w60.002.txt", "n100w60.003.txt", "n100w60.004.txt", "n100w60.005.txt",
        "n150w20.001.txt", "n150w20.002.txt", "n150w20.003.txt", "n150w20.004.txt", "n150w20.005.txt",
        "n150w40.001.txt", "n150w40.002.txt", "n150w40.003.txt", "n150w40.004.txt", "n150w40.005.txt",
        "n150w60.001.txt", "n150w60.002.txt", "n150w60.003.txt", "n150w60.004.txt", "n150w60.005.txt",
        "n200w20.001.txt", "n200w20.002.txt", "n200w20.003.txt", "n200w20.004.txt", "n200w20.005.txt",
        "n200w40.001.txt", "n200w40.002.txt", "n200w40.003.txt", "n200w40.004.txt", "n200w40.005.txt",
        "n20w100.001.txt", "n20w100.002.txt", "n20w100.003.txt", "n20w100.004.txt", "n20w100.005.txt",
        "n20w20.001.txt", "n20w20.002.txt", "n20w20.003.txt", "n20w20.004.txt", "n20w20.005.txt",
        "n20w40.001.txt", "n20w40.002.txt", "n20w40.003.txt", "n20w40.004.txt", "n20w40.005.txt",
        "n20w60.001.txt", "n20w60.002.txt", "n20w60.003.txt", "n20w60.004.txt", "n20w60.005.txt",
        "n20w80.001.txt", "n20w80.002.txt", "n20w80.003.txt", "n20w80.004.txt", "n20w80.005.txt",
        "n40w100.001.txt", "n40w100.002.txt", "n40w100.003.txt", "n40w100.004.txt", "n40w100.005.txt",
        "n40w20.001.txt", "n40w20.002.txt", "n40w20.003.txt", "n40w20.004.txt", "n40w20.005.txt",
        "n40w40.001.txt", "n40w40.002.txt", "n40w40.003.txt", "n40w40.004.txt", "n40w40.005.txt",
        "n40w60.001.txt", "n40w60.002.txt", "n40w60.003.txt", "n40w60.004.txt", "n40w60.005.txt",
        "n40w80.001.txt", "n40w80.002.txt", "n40w80.003.txt", "n40w80.004.txt", "n40w80.005.txt",
        "n60w100.001.txt", "n60w100.002.txt", "n60w100.003.txt", "n60w100.004.txt", "n60w100.005.txt",
        "n60w20.001.txt", "n60w20.002.txt", "n60w20.003.txt", "n60w20.004.txt", "n60w20.005.txt",
        "n60w40.001.txt", "n60w40.002.txt", "n60w40.003.txt", "n60w40.004.txt", "n60w40.005.txt",
        "n60w60.001.txt", "n60w60.002.txt", "n60w60.003.txt", "n60w60.004.txt", "n60w60.005.txt",
        "n60w80.001.txt", "n60w80.002.txt", "n60w80.003.txt", "n60w80.004.txt", "n60w80.005.txt",
        "n80w20.001.txt", "n80w20.002.txt", "n80w20.003.txt", "n80w20.004.txt", "n80w20.005.txt",
        "n80w40.001.txt", "n80w40.002.txt", "n80w40.003.txt", "n80w40.004.txt", "n80w40.005.txt",
        "n80w60.001.txt", "n80w60.002.txt", "n80w60.003.txt", "n80w60.004.txt", "n80w60.005.txt",
        "n80w80.001.txt", "n80w80.002.txt", "n80w80.003.txt", "n80w80.004.txt", "n80w80.005.txt"
    };

    int numTests = sizeof(testDatasets) / sizeof(testDatasets[0]);

    printf("Total datasets to test: %d\n\n", numTests);

    // 输出文件
    FILE *resultFp = fopen("results_sma_beam.csv", "w");
    if (!resultFp)
    {
        perror("Failed to create result file");
        return 1;
    }

    fprintf(resultFp, "Dataset,Dimension,TimeLimit_s,TargetMakespan,Algorithm,Distance,Makespan,Fitness,Time_ms,Iterations,BeamExec,BeamImprove,Gap_percent\n");

    int successCount = 0;

    for (int test = 0; test < numTests; test++)
    {
        const char *datasetName = testDatasets[test];
        char dataPath[256];
        snprintf(dataPath, sizeof(dataPath), "../dataset/Dumas/%s", datasetName);

        printf("\n========================================\n");
        printf("Testing dataset [%d/%d]: %s\n", test + 1, numTests, datasetName);
        printf("========================================\n");

        // 读取维度
        FILE *fp = fopen(dataPath, "r");
        if (!fp)
        {
            fprintf(stderr, "Failed to open: %s\n", dataPath);
            fprintf(resultFp, "%s,N/A,N/A,N/A,SMA-Beam,N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A\n", datasetName);
            continue;
        }
        int dim;
        fscanf(fp, "%d", &dim);
        fclose(fp);

        printf("Dimension: %d nodes\n", dim);

        // 获取时间限制：统一设置为120秒
        double timeLimit = 120.0;  // 120秒时间上限

        // GendreauDumasExtended 数据集暂无已知目标值
        double targetMakespan = get_expected_makespan(datasetName);  // 未找到则返回 -1

        printf("Time Limit: %.2f seconds (2 minutes)\n", timeLimit);
        if (targetMakespan > 0)
            printf("Target Makespan: %.2f\n", targetMakespan);
        else
            printf("Target Makespan: N/A\n");

        // 准备边界
        FIT_DATA_TYPE *lb = (FIT_DATA_TYPE *)malloc(dim * sizeof(FIT_DATA_TYPE));
        FIT_DATA_TYPE *ub = (FIT_DATA_TYPE *)malloc(dim * sizeof(FIT_DATA_TYPE));
        for (int i = 0; i < dim; i++)
        {
            lb[i] = 0;
            ub[i] = dim - 1;
        }

        // 运行 SMA-Beam 混合算法
        printf("Running SMA-Beam Hybrid Algorithm...\n");
        BeamConfig beamConfig = createAdaptiveBeamConfig(POP, dim);  // 使用自适应配置

        SMABeamResult *beamResult = SMA_Beam_TimeLimited(
            POP, dim, lb, ub, dataPath, SPEED, timeLimit, beamConfig
        );

        if (beamResult)
        {
            // 计算 Gap（仅在有目标值时有意义）
            double gap = 0.0;
            int isSuccess = 0;
            if (targetMakespan > 0)
            {
                gap = ((beamResult->finalMakespan - targetMakespan) / targetMakespan) * 100.0;
                isSuccess = (beamResult->finalMakespan <= targetMakespan);
                if (isSuccess) successCount++;
            }

            if (targetMakespan > 0)
            {
                fprintf(resultFp, "%s,%d,%.2f,%.2f,SMA-Beam,%.2f,%.2f,%.2f,%.2f,%d,%d,%d,%.2f\n",
                        datasetName, dim, timeLimit, targetMakespan,
                        beamResult->finalDistance, beamResult->finalMakespan,
                        beamResult->destinationFitness, beamResult->elapsedTimeMs,
                        beamResult->iterationATime,
                        beamResult->beamSearchExecutions, beamResult->solutionsFromBeam,
                        gap);
            }
            else
            {
                fprintf(resultFp, "%s,%d,%.2f,N/A,SMA-Beam,%.2f,%.2f,%.2f,%.2f,%d,%d,%d,N/A\n",
                        datasetName, dim, timeLimit,
                        beamResult->finalDistance, beamResult->finalMakespan,
                        beamResult->destinationFitness, beamResult->elapsedTimeMs,
                        beamResult->iterationATime,
                        beamResult->beamSearchExecutions, beamResult->solutionsFromBeam);
            }

            printf("Result: Distance=%.2f, Makespan=%.2f",
                   beamResult->finalDistance, beamResult->finalMakespan);
            if (targetMakespan > 0)
                printf(" (Target: %.2f, Gap=%.2f%%)", targetMakespan, gap);
            printf("\n");
            printf("Time: %.2f ms, Iterations: %d\n",
                   beamResult->elapsedTimeMs, beamResult->iterationATime);
            printf("Beam Executions: %d, Improvements: %d\n",
                   beamResult->beamSearchExecutions, beamResult->solutionsFromBeam);
            if (targetMakespan > 0)
                printf("Success: %s\n", isSuccess ? "Yes" : "No");

            #ifdef PRINT_BEST_ROUTE
            {
                // 解码优先级键为实际访问顺序
                // bestPositions[0]=depot(固定), [1..dim-1] 为客户优先级键
                // 按键值升序排序得到访问顺序
                typedef struct { double key; int node; } NodeKey;
                NodeKey *nk = (NodeKey *)malloc((dim - 1) * sizeof(NodeKey));
                for (int i = 1; i < dim; i++) {
                    nk[i-1].key  = beamResult->bestPositions[i];
                    nk[i-1].node = i;
                }
                // 冒泡排序（dim 一般不大）
                for (int i = 0; i < dim - 2; i++)
                    for (int j = i + 1; j < dim - 1; j++)
                        if (nk[j].key < nk[i].key) { NodeKey tmp = nk[i]; nk[i] = nk[j]; nk[j] = tmp; }

                // 读取数据矩阵用于验证
                dataMatrix *vdata = readMatrix(dataPath);

                printf("--- Best Route ---\n");
                printf("0");
                for (int i = 0; i < dim - 1; i++) printf(" -> %d", nk[i].node);
                printf(" -> 0\n");

                // 逐步验证时间窗，输出每站到达/离开时间
                if (vdata) {
                    printf("%-6s %-8s %-10s %-10s %-10s %-6s\n",
                           "Step", "Node", "Arrive", "Early", "Late", "OK?");
                    double t = vdata->tw[0].earliest;
                    int feasible = 1;
                    for (int i = 0; i < dim - 1; i++) {
                        int from = (i == 0) ? 0 : nk[i-1].node;
                        int to   = nk[i].node;
                        t += vdata->dist[from][to] / (double)SPEED;
                        int ok = (t <= vdata->tw[to].latest);
                        if (t < vdata->tw[to].earliest) t = vdata->tw[to].earliest;
                        if (!ok) feasible = 0;
                        printf("%-6d %-8d %-10.2f %-10.2f %-10.2f %-6s\n",
                               i + 1, to, t,
                               vdata->tw[to].earliest, vdata->tw[to].latest,
                               ok ? "YES" : "*** NO ***");
                    }
                    // 返回仓库
                    t += vdata->dist[nk[dim-2].node][0] / (double)SPEED;
                    printf("Return to depot at: %.2f\n", t);
                    printf("Route feasible: %s\n", feasible ? "YES" : "NO (time window violated!)");
                    freeDataMatrix(vdata);
                }
                free(nk);
            }
            #endif

            free(beamResult->bestPositions);
            free(beamResult->bestPositionsStart);
            free(beamResult->convergenceCurve);
            free(beamResult);
        }
        else
        {
            fprintf(resultFp, "%s,%d,%.2f,N/A,SMA-Beam,N/A,N/A,N/A,N/A,N/A,N/A,N/A,N/A\n",
                    datasetName, dim, timeLimit);
            printf("Failed to run algorithm!\n");
        }

        free(lb);
        free(ub);
    }

    fclose(resultFp);

    printf("\n===========================================\n");
    printf("All tests completed!\n");
    printf("===========================================\n");
    printf("Total datasets tested: %d\n", numTests);
    printf("Results saved to: results_sma_beam.csv\n");
    printf("===========================================\n");

    return 0;
}
