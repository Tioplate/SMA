// SMA-Beam 混合算法测试程序
#include "sma_beam.h"
#include "dataset_time_limits.h"
#include "expected_results.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define POP 200
#define SPEED 1

int main()
{
    printf("===========================================\n");
    printf("  SMA-Beam Hybrid Algorithm Test Program  \n");
    printf("  Testing ALL datasets from SolomonPotvinBengio  \n");
    printf("===========================================\n\n");

    // 测试数据集列表 - SolomonPotvinBengio 文件夹下的全部30个数据集
    const char *testDatasets[] = {
        "rc_201.1", "rc_201.2", "rc_201.3", "rc_201.4",
        "rc_202.1", "rc_202.2", "rc_202.3", "rc_202.4",
        "rc_203.1", "rc_203.2", "rc_203.3", "rc_203.4",
        "rc_204.1", "rc_204.2", "rc_204.3",
        "rc_205.1", "rc_205.2", "rc_205.3", "rc_205.4",
        "rc_206.1", "rc_206.2", "rc_206.3", "rc_206.4",
        "rc_207.1", "rc_207.2", "rc_207.3", "rc_207.4",
        "rc_208.1", "rc_208.2", "rc_208.3"
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
        snprintf(dataPath, sizeof(dataPath), "../dataset/SolomonPotvinBengio/%s.txt", datasetName);

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
