# CLion 中运行 SMA-Beam 混合算法指南

## 方法一：直接运行（最简单）⭐

### 步骤：

1. **打开CLion**，加载项目 `D:\mypro\SMA\SMA_Back`

2. **选择运行配置**
   - 点击CLion右上角的运行配置下拉菜单
   - 你会看到两个配置：
     - `test_sma_beam` - SMA-Beam混合算法测试程序 ✅ **推荐先运行这个**
     - `SMA_Back` - 原始SMA算法程序

3. **选择 `test_sma_beam`**

4. **点击绿色运行按钮 ▶️** 或按 `Shift + F10`

5. **查看结果**
   - 在底部的"运行"窗口查看输出
   - 会自动测试4个数据集
   - 结果保存在 `cmake-build-debug/Debug/results_sma_beam.csv`

## 方法二：调试运行

### 步骤：

1. **打开 `test_sma_beam.c` 文件**

2. **设置断点**（可选）
   - 在你想调试的代码行号左侧点击，设置断点
   - 例如：第85行（SMA_Beam算法调用处）

3. **点击调试按钮 🐛** 或按 `Shift + F9`

4. **调试控制**
   - F8: 单步跳过
   - F7: 单步进入
   - F9: 继续运行
   - 查看变量值、调用栈等

## 方法三：自定义运行参数

### 如果需要修改测试数据集：

1. **打开 `test_sma_beam.c`**

2. **修改第18-23行**：
```c
const char *testDatasets[] = {
    "rc_201.1",
    "rc_202.1",
    "rc_203.1",
    "rc_205.1",
    // 添加更多数据集...
    "rc_204.1",
    "rc_205.2"
};
```

3. **保存文件** (`Ctrl + S`)

4. **重新运行** (`Shift + F10`)

## 方法四：在自己的代码中使用

### 创建新的测试文件：

1. **在项目根目录右键** → `New` → `C/C++ Source File`

2. **命名为** `my_test.c`

3. **编写代码**：
```c
#include "sma_beam.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    // 参数配置
    int pop = 200;
    int dim = 50;  // 根据你的数据集调整
    double timeLimit = 30.0;  // 30秒
    
    // 边界
    FIT_DATA_TYPE *lb = malloc(dim * sizeof(FIT_DATA_TYPE));
    FIT_DATA_TYPE *ub = malloc(dim * sizeof(FIT_DATA_TYPE));
    for (int i = 0; i < dim; i++) {
        lb[i] = 0;
        ub[i] = dim - 1;
    }
    
    // 创建束搜索配置
    BeamConfig config = createDefaultBeamConfig(pop);
    
    // 可选：自定义配置
    // config.beamWidth = 50;
    // config.expansionFactor = 8;
    // config.hybridInterval = 15;
    
    // 运行算法
    printf("Running SMA-Beam Algorithm...\n");
    SMABeamResult *result = SMA_Beam_TimeLimited_WithEarlyStop(
        pop, dim, lb, ub,
        "dataset/SolomonPotvinBengio/rc_201.1.txt",
        1,           // speed
        timeLimit,
        592.0,       // 预期makespan（用于提前停止）
        config
    );
    
    // 输出结果
    if (result) {
        printf("\n========== Results ==========\n");
        printf("最优适应度: %.2f\n", result->destinationFitness);
        printf("总距离: %.2f\n", result->finalDistance);
        printf("Makespan: %.2f\n", result->finalMakespan);
        printf("运行时间: %.2f ms\n", result->elapsedTimeMs);
        printf("迭代次数: %d\n", result->iterationATime);
        printf("束搜索执行次数: %d\n", result->beamSearchExecutions);
        printf("束搜索改进次数: %d\n", result->solutionsFromBeam);
        printf("是否提前停止: %s\n", result->earlyStopTriggered ? "是" : "否");
        
        // 清理
        free(result->bestPositions);
        free(result->bestPositionsStart);
        free(result->convergenceCurve);
        free(result);
    }
    
    free(lb);
    free(ub);
    
    return 0;
}
```

4. **修改 CMakeLists.txt**，在最后添加：
```cmake
# 你的自定义测试程序
add_executable(my_test my_test.c)
target_link_libraries(my_test sma_beam_lib)
set_target_properties(my_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
```

5. **重新加载CMake**
   - 点击 `Tools` → `CMake` → `Reload CMake Project`
   - 或者点击右上角的 🔄 重新加载按钮

6. **运行新程序**
   - 右上角选择 `my_test`
   - 点击运行 ▶️

## 常见问题解决

### 问题1: 找不到数据文件

**现象：** 运行时提示 `Failed to open: dataset/...`

**解决方法：**
- 确保数据文件在正确位置：`D:\mypro\SMA\SMA_Back\dataset\SolomonPotvinBengio\*.txt`
- 或修改代码中的路径为绝对路径
- 或使用相对路径：`../../dataset/SolomonPotvinBengio/rc_201.1.txt`

### 问题2: CMake配置错误

**现象：** 提示 CMake 错误

**解决方法：**
1. 点击 `Tools` → `CMake` → `Reset Cache and Reload Project`
2. 或者删除 `cmake-build-debug` 文件夹
3. 重新加载项目

### 问题3: 编译错误

**现象：** 提示链接错误或未定义引用

**解决方法：**
1. 确保 CMakeLists.txt 正确配置
2. 点击 `Build` → `Rebuild Project`
3. 检查所有源文件是否存在

### 问题4: 运行配置不显示

**现象：** 右上角没有 `test_sma_beam` 选项

**解决方法：**
1. 点击右上角运行配置下拉菜单
2. 选择 `Edit Configurations...`
3. 点击左上角的 `+` 号
4. 选择 `CMake Application`
5. 在 `Target` 中选择 `test_sma_beam`
6. 点击 `OK`

## 快捷键参考

| 功能 | Windows快捷键 | Mac快捷键 |
|------|--------------|-----------|
| 运行 | `Shift + F10` | `Ctrl + R` |
| 调试 | `Shift + F9` | `Ctrl + D` |
| 停止 | `Ctrl + F2` | `Cmd + F2` |
| 重新运行 | `Ctrl + F5` | `Cmd + R` |
| 查找文件 | `Ctrl + Shift + N` | `Cmd + Shift + O` |
| 查找内容 | `Ctrl + Shift + F` | `Cmd + Shift + F` |
| 重新加载CMake | - | - |
| 构建项目 | `Ctrl + F9` | `Cmd + F9` |

## 查看输出结果

### 控制台输出
- 在CLion底部的"运行"标签页查看实时输出
- 可以看到每次迭代的进度、最优值等信息

### CSV文件
运行完成后查看：
```
cmake-build-debug\Debug\results_sma_beam.csv
```

用Excel或文本编辑器打开，包含：
- Dataset: 数据集名称
- Algorithm: 算法名称（SMA-Beam）
- Distance: 总距离
- Makespan: 总时间
- Fitness: 适应度值
- Time_ms: 运行时间（毫秒）
- Iterations: 迭代次数
- BeamExec: 束搜索执行次数
- BeamImprove: 束搜索改进次数
- EarlyStop: 是否提前停止

## 性能监控

### 查看算法执行情况：

在代码中你可以看到这些输出：
```
SMA-Beam Hybrid Algorithm Started (Time Limit: 30.00 seconds)
Beam Width: 40, Expansion Factor: 5, Hybrid Interval: 20
[Iter 1] Best: 1000.00, Time: 10.00 ms (3.3% of limit)
[Iter 50] Best: 850.00, Time: 150.00 ms (50.0% of limit)
[Iter 60] Beam Search improved: 850.00 -> 820.00  ← 束搜索带来改进！
Expected result achieved at iteration 85: 800.00 <= 805.00  ← 提前停止
```

**关键指标：**
- `Best`: 当前最优适应度
- `Time`: 已用时间和占比
- `Beam Search improved`: 束搜索是否改进了解
- `Expected result achieved`: 是否达到预期目标

## 参数调优指南

### 追求高质量解：
```c
BeamConfig config;
config.beamWidth = 50;           // 更大的束宽
config.expansionFactor = 8;      // 更多邻域解
config.hybridInterval = 15;      // 更频繁的束搜索
config.eliteRatio = 0.15;        // 保护更多精英
```

### 追求运行速度：
```c
BeamConfig config;
config.beamWidth = 20;           // 较小的束宽
config.expansionFactor = 3;      // 较少邻域解
config.hybridInterval = 30;      // 较低频率
config.eliteRatio = 0.1;         // 标准精英比例
```

### 平衡模式（推荐）：
```c
BeamConfig config = createDefaultBeamConfig(pop);  // 使用默认配置
```

## 下一步

1. **✅ 运行测试程序** - 验证算法工作正常
2. **📊 查看结果** - 分析CSV文件中的性能数据
3. **🔧 调整参数** - 根据你的问题特性优化参数
4. **📝 阅读文档** - 查看 `SMA-Beam混合算法说明文档.md`
5. **🚀 集成到项目** - 在你的代码中使用算法

---

**提示：** 如果遇到任何问题，可以：
1. 查看 `快速入门指南.md`
2. 查看 `SMA-Beam混合算法说明文档.md` 的FAQ部分
3. 检查控制台的错误信息

**祝你使用愉快！** 🎉

