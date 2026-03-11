#include "myjni.h"
#include "dataset_time_limits.h"
#include "expected_results.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

// 保持种群规模与速度常量
#define POP 200
#define SPEED 1

// 读取数据文件第一行的维度（节点数）
static int readDimFromDataset(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp)
    {
        perror("Failed to open data file");
        return -1;
    }
    int n = -1;
    if (fscanf(fp, "%d", &n) != 1)
    {
        fprintf(stderr, "Failed to read dimension\n");
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return n;
}

// 检查文件名是否为 .txt 数据集文件
static int isDatasetFile(const char *filename)
{
    size_t len = strlen(filename);
    return (len > 4 && strcmp(filename + len - 4, ".txt") == 0);
}

// 从完整文件名中提取数据集基础名称（去掉.txt后缀）
static void extractBaseName(const char *filename, char *basename, size_t maxLen)
{
    strncpy(basename, filename, maxLen - 1);
    basename[maxLen - 1] = '\0';
    char *dot = strrchr(basename, '.');
    if (dot && strcmp(dot, ".txt") == 0)
    {
        *dot = '\0';
    }
}

int main()
{
    // Use relative path to the dataset folder in project root directory
    // Assuming running from cmake-build-debug/Debug/ directory
    const char *datasetDir = "../dataset/SolomonPotvinBengio";
    // Output file in project root directory (two levels up from Debug folder)
    const char *outputFile = "results_run.csv";

    // Open output file
    FILE *resultFp = fopen(outputFile, "w");
    if (!resultFp)
    {
        perror("Failed to create result file");
        return 1;
    }

    // Write CSV header
    fprintf(resultFp, "Dataset,Distance,Makespan,Fitness,Time_ms,TimeLimit_s,Iterations,StopReason\n");
    printf("========================================\n");
    printf("Batch processing SolomonPotvinBengio datasets\n");
    printf("Based on Beam-ACOTime time limits from MT30.docx\n");
    printf("Results will be written to: %s\n", outputFile);
    printf("========================================\n\n");

    int fileCount = 0;

#ifdef _WIN32
    // Windows implementation
    WIN32_FIND_DATAA findData;
    char searchPath[512];
    snprintf(searchPath, sizeof(searchPath), "%s/*.txt", datasetDir);

    HANDLE hFind = FindFirstFileA(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        fprintf(stderr, "Failed to open dataset directory\n");
        fclose(resultFp);
        return 1;
    }

    do
    {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            char *filename = findData.cFileName;
            if (!isDatasetFile(filename))
                continue;

            fileCount++;

            // Construct full path
            char fullPath[512];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", datasetDir, filename);

            printf("[%d] Processing: %s\n", fileCount, filename);

            // Read dimension
            int DIM = readDimFromDataset(fullPath);
            if (DIM <= 0)
            {
                fprintf(stderr, "  Skipped (failed to read dimension)\n\n");
                fprintf(resultFp, "%s,ERROR,ERROR,ERROR,ERROR,ERROR,ERROR\n", filename);
                continue;
            }

            printf("  Dimension: %d\n", DIM);

            // Extract dataset base name and find time limit
            char basename[256];
            extractBaseName(filename, basename, sizeof(basename));

            // 固定使用20秒时间限制
            double timeLimit = 20.0; // 固定20秒
            printf("  Time limit: %.2f seconds (fixed)\n", timeLimit);

            // 不再使用早停机制
            // double expectedMakespan = get_expected_makespan(basename);
            // if (expectedMakespan > 0)
            // {
            //     printf("  Expected Makespan: %.2f (early stop when reached)\n", expectedMakespan);
            // }

            // Dynamically allocate lower and upper bounds
            FIT_DATA_TYPE *lb = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
            FIT_DATA_TYPE *ub = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
            if (!lb || !ub)
            {
                fprintf(stderr, "  Skipped (memory allocation failed)\n\n");
                fprintf(resultFp, "%s,ERROR,ERROR,ERROR,ERROR,%.2f,ERROR\n", filename, timeLimit);
                free(lb);
                free(ub);
                continue;
            }

            for (int i = 0; i < DIM; i++)
            {
                lb[i] = 0;
                ub[i] = DIM;
            }

            // Use SMA algorithm with early stop support
            SMAResult *result = SMA_TimeLimited(POP, DIM, lb, ub, fullPath, SPEED, timeLimit);

            if (result)
            {
                // Write results to CSV file
                fprintf(resultFp, "%s,%.2f,%.2f,%.6f,%.0f,%.2f,%d,%s\n",
                        filename,
                        result->finalDistance,    // 实际距离
                        result->finalMakespan,    // makespan
                        result->destinationFitness,
                        result->elapsedTimeMs,    // 运行时间（毫秒）
                        timeLimit,                 // 时间限制（秒）
                        result->iterationATime,    // 实际迭代次数
                        "TIME_LIMIT");  // 停止原因

                printf("  Distance: %.2f, Makespan: %.2f, Fitness: %.6f, Iterations: %d\n",
                       result->finalDistance, result->finalMakespan,
                       result->destinationFitness, result->iterationATime);

                // 释放结果
                free(result->bestPositions);
                free(result->bestPositionsStart);
                free(result->convergenceCurve);
                free(result);
            }
            else
            {
                fprintf(resultFp, "%s,ERROR,ERROR,ERROR,ERROR,%.2f,ERROR\n", filename, timeLimit);
            }

            free(lb);
            free(ub);

            printf("  Completed!\n\n");
        }
    } while (FindNextFileA(hFind, &findData) != 0);

    FindClose(hFind);

#else
    // Unix/Linux implementation using dirent.h
    DIR *dir = opendir(datasetDir);
    if (!dir)
    {
        perror("Failed to open dataset directory");
        fclose(resultFp);
        return 1;
    }

    struct dirent *entry;

    // Traverse all files in the directory
    while ((entry = readdir(dir)) != NULL)
    {
        if (!isDatasetFile(entry->d_name))
            continue;

        fileCount++;

        // Construct full path
        char fullPath[512];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", datasetDir, entry->d_name);

        printf("[%d] Processing: %s\n", fileCount, entry->d_name);

        // Read dimension
        int DIM = readDimFromDataset(fullPath);
        if (DIM <= 0)
        {
            fprintf(stderr, "  Skipped (failed to read dimension)\n\n");
            fprintf(resultFp, "%s,ERROR,ERROR,ERROR,ERROR,ERROR,ERROR\n", entry->d_name);
            continue;
        }

        printf("  Dimension: %d\n", DIM);

        // Extract dataset base name and find time limit
        char basename[256];
        extractBaseName(entry->d_name, basename, sizeof(basename));

        // 固定使用20秒时间限制
        double timeLimit = 20.0; // 固定20秒
        printf("  Time limit: %.2f seconds (fixed)\n", timeLimit);

        // 不再使用早停机制
        // double expectedMakespan = get_expected_makespan(basename);
        // if (expectedMakespan > 0)
        // {
        //     printf("  Expected Makespan: %.2f (early stop when reached)\n", expectedMakespan);
        // }

        // Dynamically allocate lower and upper bounds
        FIT_DATA_TYPE *lb = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
        FIT_DATA_TYPE *ub = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
        if (!lb || !ub)
        {
            fprintf(stderr, "  Skipped (memory allocation failed)\n\n");
            fprintf(resultFp, "%s,ERROR,ERROR,ERROR,ERROR,%.2f,ERROR\n", entry->d_name, timeLimit);
            free(lb);
            free(ub);
            continue;
        }

        for (int i = 0; i < DIM; i++)
        {
            lb[i] = 0;
            ub[i] = DIM;
        }

        // Use SMA algorithm with early stop support
        SMAResult *result = SMA_TimeLimited(POP, DIM, lb, ub, fullPath, SPEED, timeLimit);

        if (result)
        {
            // Write results to CSV file
            fprintf(resultFp, "%s,%.2f,%.2f,%.6f,%.0f,%.2f,%d,%s\n",
                    entry->d_name,
                    result->finalDistance,    // 实际距离
                    result->finalMakespan,    // makespan
                    result->destinationFitness,
                    result->elapsedTimeMs,    // 运行时间（毫秒）
                    timeLimit,                 // 时间限制（秒）
                    result->iterationATime,    // 实际迭代次数
                    "TIME_LIMIT");  // 停止原因

            printf("  Distance: %.2f, Makespan: %.2f, Fitness: %.6f, Iterations: %d\n",
                   result->finalDistance, result->finalMakespan,
                   result->destinationFitness, result->iterationATime);

            // 释放结果
            free(result->bestPositions);
            free(result->bestPositionsStart);
            free(result->convergenceCurve);
            free(result);
        }
        else
        {
            fprintf(resultFp, "%s,ERROR,ERROR,ERROR,ERROR,%.2f,ERROR\n", entry->d_name, timeLimit);
        }

        free(lb);
        free(ub);

        printf("  Completed!\n\n");
    }

    closedir(dir);
#endif

    fclose(resultFp);

    printf("========================================\n");
    printf("All completed! Total datasets processed: %d\n", fileCount);
    printf("Results saved to: %s\n", outputFile);
    printf("========================================\n");

    return 0;
}
