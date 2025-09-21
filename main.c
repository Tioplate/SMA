#include "myjni.h"
#define DIM 35
#define POP 30
#define SPEED 1000

int main()
{
    const char *dataPath = "../dataset/rc_205.3.txt"; // 从构建目录运行时使用上级目录
    FIT_DATA_TYPE lb[DIM];
    FIT_DATA_TYPE ub[DIM];
    for (int i = 0; i < DIM; i++)
    {
        lb[i] = 0;
        ub[i] = DIM;
    }
    SMAResult *result;
    result = SMA(POP, DIM, lb, ub, (char*)dataPath, SPEED);
    return 0;
}
