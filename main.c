#include "myjni.h"
#include <dirent.h>
#include <string.h>

#define POP 30
#define SPEED 1

int main()
{
    const char *dirPath = "../dataset/GendreauDumasExtended";
    DIR *dir;
    struct dirent *ent;
    FILE *csv = fopen("results_run.csv", "w");
    if (csv) {
        fprintf(csv, "Dataset,DIM,BestFitness,RealMakespan,Feasible,ElaspedTime(s)\n");
    }

    if ((dir = opendir(dirPath)) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (strstr(ent->d_name, ".txt") != NULL) {
                char dataPath[512];
                snprintf(dataPath, sizeof(dataPath), "%s/%s", dirPath, ent->d_name);

                dataMatrix *data = readMatrix((char *)dataPath);
                if (!data) continue;
                int DIM = data->size;
                freeDataMatrix(data);

                FIT_DATA_TYPE *lb = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
                FIT_DATA_TYPE *ub = (FIT_DATA_TYPE *)malloc(DIM * sizeof(FIT_DATA_TYPE));
                for (int i = 0; i < DIM; i++)
                {
                    lb[i] = 0;
                    ub[i] = DIM;
                }

                printf("\n========================================\n");
                printf("Processing dataset: %s (DIM = %d)\n", ent->d_name, DIM);
                SMAResult *result = SMA(POP, DIM, lb, ub, dataPath, SPEED);

                if (csv) {
                    fprintf(csv, "%s,%d,%lf,%lf,%s,%.0f\n", ent->d_name, DIM, result->destinationFitness, result->real_makespan, result->feasible ? "Yes" : "No", result->elapsedSeconds);
                    fflush(csv);
                }

                free(lb);
                free(ub);
                free(result->convergenceCurve);
                free(result->bestPositions);
                free(result);
            }
        }
        closedir(dir);
    } else {
        printf("Failed to open directory %s\n", dirPath);
    }

    if (csv) fclose(csv);
    return 0;
}
