// SMA-Beam 混合算法测试程序
#include "sma_beam.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define POP 200
#define SPEED 1

// 控制是否输出最佳路线详情，注释掉此行即可关闭
#define PRINT_BEST_ROUTE

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
    printf("  Testing ALL datasets from SolomonPotvinBengio  \n");
    printf("===========================================\n\n");

    // 测试数据集列表 - SolomonPotvinBengio 文件夹下的全部数据集
    const char *testDatasets[] = {
         "rc_201.1.txt", "rc_201.2.txt", "rc_201.3.txt", "rc_201.4.txt",
         "rc_202.1.txt", "rc_202.2.txt", "rc_202.3.txt", "rc_202.4.txt",
         "rc_203.1.txt", "rc_203.2.txt", "rc_203.3.txt", "rc_203.4.txt",
         "rc_204.1.txt", "rc_204.2.txt", "rc_204.3.txt",
         "rc_205.1.txt", "rc_205.2.txt", "rc_205.3.txt", "rc_205.4.txt",
         "rc_206.1.txt", "rc_206.2.txt", "rc_206.3.txt", "rc_206.4.txt",
         "rc_207.1.txt", "rc_207.2.txt", "rc_207.3.txt", "rc_207.4.txt",
         "rc_208.1.txt", "rc_208.2.txt", "rc_208.3.txt"
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

    fprintf(resultFp, "Dataset,Dimension,TimeLimit_s,TargetMakespan,Algorithm,Distance,Makespan,Fitness,Time_ms,Iterations,BeamExec,BeamImprove,Gap_percent,Feasible\n");

    int successCount = 0;

    for (int test = 0; test < numTests; test++)
    {
        const char *datasetName = testDatasets[test];
        char dataPath[256];
        snprintf(dataPath, sizeof(dataPath), "../dataset/SolomonPotvinBengio/%s", datasetName);

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
        double timeLimit = 1200.0;  // 120秒时间上限

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

        SMABeamResult *beamResult = SMA_Beam_TimeLimited_WithEarlyStop(
            POP, dim, lb, ub, dataPath, SPEED, timeLimit, targetMakespan, beamConfig
        );

        if (beamResult)
        {
            // --- 执行严格可行性验证 ---
            int actual_feasible = 1;
            typedef struct { double key; int node; } NodeKey;
            NodeKey *nk = (NodeKey *)malloc((dim - 1) * sizeof(NodeKey));
            for (int i = 1; i < dim; i++) {
                nk[i-1].key  = beamResult->bestPositions[i];
                nk[i-1].node = i;
            }
            // 冒泡排序
            for (int i = 0; i < dim - 2; i++)
                for (int j = i + 1; j < dim - 1; j++)
                    if (nk[j].key < nk[i].key) { NodeKey tmp = nk[i]; nk[i] = nk[j]; nk[j] = tmp; }

            dataMatrix *vdata = readMatrix(dataPath);
            if (vdata) {
                double t = vdata->tw[0].earliest;
                for (int i = 0; i < dim - 1; i++) {
                    int from = (i == 0) ? 0 : nk[i-1].node;
                    int to   = nk[i].node;
                    t += vdata->dist[from][to] / (double)SPEED;
                    int ok = (t <= vdata->tw[to].latest);
                    if (t < vdata->tw[to].earliest) t = vdata->tw[to].earliest;
                    if (!ok) actual_feasible = 0;
                }
                // 返回仓库
                t += vdata->dist[nk[dim-2].node][0] / (double)SPEED;
                if (t > vdata->tw[0].latest) actual_feasible = 0;
                freeDataMatrix(vdata);
            }

            // 计算 Gap（仅在有目标值时有意义）
            double gap = 0.0;
            int isSuccess = 0;
            if (targetMakespan > 0)
            {
                gap = ((beamResult->finalMakespan - targetMakespan) / targetMakespan) * 100.0;
                isSuccess = (beamResult->finalMakespan <= targetMakespan && actual_feasible);
                if (isSuccess) successCount++;
            }

            if (targetMakespan > 0)
            {
                fprintf(resultFp, "%s,%d,%.2f,%.2f,SMA-Beam,%.2f,%.2f,%.2f,%.2f,%d,%d,%d,%.2f,%d\n",
                        datasetName, dim, timeLimit, targetMakespan,
                        beamResult->finalDistance, beamResult->finalMakespan,
                        beamResult->destinationFitness, beamResult->elapsedTimeMs,
                        beamResult->iterationATime,
                        beamResult->beamSearchExecutions, beamResult->solutionsFromBeam,
                        gap, actual_feasible);
            }
            else
            {
                fprintf(resultFp, "%s,%d,%.2f,N/A,SMA-Beam,%.2f,%.2f,%.2f,%.2f,%d,%d,%d,N/A,%d\n",
                        datasetName, dim, timeLimit,
                        beamResult->finalDistance, beamResult->finalMakespan,
                        beamResult->destinationFitness, beamResult->elapsedTimeMs,
                        beamResult->iterationATime,
                        beamResult->beamSearchExecutions, beamResult->solutionsFromBeam,
                        actual_feasible);
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

            printf("Route feasible (verification): %s\n", actual_feasible ? "YES" : "NO (time window violated!)");

            // --- 新增：将最佳路径保存到对应的 txt 文件 ---
            {
                char outPath[256];
                snprintf(outPath, sizeof(outPath), "%s_route.txt", datasetName);
                FILE *routeFp = fopen(outPath, "w");
                if (routeFp) {
                    fprintf(routeFp, "0 ");
                    for (int i = 0; i < dim - 1; i++) {
                        fprintf(routeFp, "%d ", nk[i].node);
                    }
                    fprintf(routeFp, "0\n");
                    fclose(routeFp);
                    printf("Route saved to %s\n", outPath);
                } else {
                    printf("Failed to save route to %s\n", outPath);
                }
            }
            // ------------------------------------------------
            free(nk);

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
