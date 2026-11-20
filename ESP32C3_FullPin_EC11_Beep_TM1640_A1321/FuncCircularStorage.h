#ifndef CIRCULAR_STORAGE_H
#define CIRCULAR_STORAGE_H
#include "GlobalParam.h"
#include <Arduino.h>
#include "FuncStandardPrint.h"
// 定义最大存储容量（例如 12 条数据）
#define MAX_CAPACITY 5  //  20ms 5次  最大 响应滞后大约在 5摄氏度  120  //3分钟  //这个决定响应速度   200-500ms  10度 
#include <vector>
#include <algorithm>
class CircularStorage {
private:
  float buffer[MAX_CAPACITY];  // 固定大小的存储数组
  int currentIndex;            // 当前写入位置索引
  int count;                   // 当前有效数据量

public:
  // 构造函数：初始化
  CircularStorage() {
    reset();
  }

  // 初始化 / 重置存储状态
  void reset() {
    currentIndex = 0;
    count = 0;
    for (int i = 0; i < MAX_CAPACITY; i++) {
      buffer[i] = 0.0f;
    }
    print(F("[存储模块] 循环存储已初始化/重置"));
  }

  // 添加新数据（如果满了，自动覆盖最老的数据）
  void addData(float val) {
    buffer[currentIndex] = val;

    // 索引循环向后移动
    currentIndex = (currentIndex + 1) % MAX_CAPACITY;

    // 有效数据量在达到最大容量前持续增加，达到后保持满状态
    if (count < MAX_CAPACITY) {
      count++;
    }
  }
  //【获取平均值 】
  float getAverage() {
    // 检查是否有有效数据，避免除以零错误
    if (count == 0) {
      return 0.0f;
    } 
    float sum = 0.0f;
    for (int i = 0; i < count; i++) {
      sum += buffer[i];
    } 
    return sum / count;
  } 
  // 【trimCount 表示头尾各去掉几个极端值】
  float getTrimmedAverage(int trimCount) {
    // 1. 检查是否有有效数据
    if (count == 0) {
      return 0.0f;
    }

    // 如果有效数据太少，不够去极值（比如总数小于等于去极端值的两倍），则退化为普通平均
    if (count <= 2 * trimCount) {
      float sum = 0.0f;
      for (int i = 0; i < count; i++) {
        sum += buffer[i];
      }
      return sum / count;
    }

    // 2. 将当前有效的 buffer 数据复制到临时容器中
    std::vector<float> temp;
    temp.reserve(count);
    for (int i = 0; i < count; i++) {
      temp.push_back(buffer[i]);
    }

    // 3. 对临时数据进行升序排序
    std::sort(temp.begin(), temp.end());

    // 4. 去掉前 trimCount 个（最小值）和后 trimCount 个（最大值），累加中间的数据
    float sum = 0.0f;
    int validCount = 0;
    for (int i = trimCount; i < count - trimCount; i++) {
      sum += temp[i];
      validCount++;
    }

    // 5. 计算并返回平均值
    return (validCount > 0) ? (sum / validCount) : 0.0f;
  }


  // 获取当前存储了多少个有效数据
  int getCount() const {
    return count;
  }

  // 获取最大容量
  int getCapacity() const {
    return MAX_CAPACITY;
  }

  // 获取指定索引的数据 (index 从 0 到 count-1，自动按时间先后排序读取)
  float getDataAt(int index) const {
    if (index < 0 || index >= count) return 0.0f;
    int actualIndex = (currentIndex - count + index + MAX_CAPACITY) % MAX_CAPACITY;
    return buffer[actualIndex];
  }

  // 获取缓冲区中的“首个”（最早的一条）数据
  float getFirst() const {
    if (count == 0) return 0.0f;
    int firstIndex = (count < MAX_CAPACITY) ? 0 : currentIndex;
    return buffer[firstIndex];
  }

  // 获取缓冲区中的“最后一个”（刚刚写入的最新一条）数据
  float getLast() const {
    if (count == 0) return 0.0f;
    int lastIndex = (currentIndex - 1 + MAX_CAPACITY) % MAX_CAPACITY;
    return buffer[lastIndex];
  }

  // === 新增：获取缓冲区中所有有效数据的最大值 ===
  float getMax() const {
    if (count == 0) return 0.0f;
    float maxVal = getDataAt(0);  // 假定第一个是最大值
    for (int i = 1; i < count; i++) {
      float val = getDataAt(i);
      if (val > maxVal) {
        maxVal = val;
      }
    }
    return maxVal;
  }

  // === 新增：获取缓冲区中所有有效数据的最小值 ===
  float getMin() const {
    if (count == 0) return 0.0f;
    float minVal = getDataAt(0);  // 假定第一个是最小值
    for (int i = 1; i < count; i++) {
      float val = getDataAt(i);
      if (val < minVal) {
        minVal = val;
      }
    }
    return minVal;
  }

  float analyzeAllPairsTrend() {
    if (count < 2) return 0.0f;

    float totalDeltaSum = 0;
    int validPairs = 0;

    // 确定最老数据的起始位置
    int startIndex = (count < MAX_CAPACITY) ? 0 : currentIndex;

    // 外层循环：作为“较早”的时间点 (i)
    for (int i = 0; i < count - 1; i++) {
      int pos1 = (startIndex + i) % MAX_CAPACITY;
      float val1 = buffer[pos1];  // 较旧的值

      // 内层循环：作为“较晚”的时间点 (j)，必须在 i 的后面
      for (int j = i + 1; j < count; j++) {
        int pos2 = (startIndex + j) % MAX_CAPACITY;
        float val2 = buffer[pos2];  // 较新的值

        // 计算差值：较新的值 - 较旧的值（保证是正向时间差）
        float delta = val2 - val1;

        totalDeltaSum += delta;
        validPairs++;
      }
    }

    if (validPairs == 0) return 0.0f;

    // 返回所有组合的平均差值
    return (float)totalDeltaSum / validPairs;
  }
};


// 最大允许温度变化速度，单位：℃/秒
constexpr float MAX_TEMP_RATE = 15.0f;

// 【判断新温度是否异常】
bool isAbnormalTemp(float newTemp, float oldTemp, unsigned long elapsedMs) {
  if (elapsedMs == 0) return true;

  float maxDelta = MAX_TEMP_RATE * elapsedMs / 1000.0f;

  return fabsf(newTemp - oldTemp) > maxDelta;
}


#endif