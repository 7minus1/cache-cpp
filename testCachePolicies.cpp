#include <iostream>
#include <string>
#include <chrono>
#include <vector>
#include <iomanip>
#include <random>
#include <algorithm>

#include "CachePolicy.h"
#include "LruCache.h"
#include "LfuCache.h"
#include "ArcCache/ArcCache.h"

class Timer {
  public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}

    double elapsed() {
      auto now = std::chrono::high_resolution_clock::now();
      return std::chrono::duration_cast<std::chrono::microseconds>(now - start_).count();
    }

  private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};

void printResult(const std::string& testName, int capacity, const std::vector<int>& hits, const std::vector<int>& get_operations) {
  std::cout << "Test: " << testName << ", Capacity: " << capacity << "\n";
  std::cout << "LRU - Hits: " << std::fixed << std::setprecision(2) << 100 * hits[0] / get_operations[0] << "%\n";
  std::cout << "LFU - Hits: " << std::fixed << std::setprecision(2) << 100 * hits[1] / get_operations[1] << "%\n";
  std::cout << "ARC - Hits: " << std::fixed << std::setprecision(2) << 100 * hits[2] / get_operations[2] << "%\n";
  std::cout << std::endl;
}

void testHotDataAccess() {
  std::cout << "\n ===== 测试场景1: 热点数据访问测试 ===== \n";

  const int CAPACITY = 50;        // 缓存容量
  const int OPERATIONS = 500000;  // 操作次数
  const int HOT_KEYS = 20;        // 热点数据数量
  const int COLD_KEYS = 5000;

  MyCache::LruCache<int, std::string> lru(CAPACITY);
  MyCache::LfuCache<int, std::string> lfu(CAPACITY);
  MyCache::ArcCache<int, std::string> arc(CAPACITY);
  
  std::random_device rd;
  std::mt19937 gen(rd());  // 随机数生成器

  std::array<MyCache::CachePolicy<int, std::string>*, 3> caches = {&lru, &lfu, &arc};
  std::vector<int> hits(3, 0);
  std::vector<int> get_operations(3, 0);

  for (int i = 0; i < caches.size(); ++i) {
    // put操作
    for (int op = 0; op < OPERATIONS; ++op) {
      // 70 热点数据 30 冷数据
      int key = op % 100 < 70? gen() % HOT_KEYS : HOT_KEYS + gen() % COLD_KEYS;
      std::string value = "value" + std::to_string(key);
      caches[i]->put(key, value);
    }
    // 随机get操作
    for (int op = 0; op < OPERATIONS; ++op) {
      int key = op % 100 < 70? gen() % HOT_KEYS : HOT_KEYS + gen() % COLD_KEYS;
      std::string result;
      get_operations[i]++;
      if (caches[i]->get(key, result)) {
        ++hits[i];
      }
    }

    printResult("热点数据访问测试", CAPACITY, hits, get_operations);
  }
}


void testLoopPattern() {
  std::cout << "\n ===== 测试场景2: 循环扫描测试 ===== \n";
  const int CAPACITY = 50;        // 缓存容量
  const int LOOP_SIZE = 500;        // 缓存容量
  const int OPERATIONS = 200000;  // 操作次数
  MyCache::LruCache<int, std::string> lru(CAPACITY);
  MyCache::LfuCache<int, std::string> lfu(CAPACITY);
  MyCache::ArcCache<int, std::string> arc(CAPACITY);
  
  std::random_device rd;
  std::mt19937 gen(rd());  // 随机数生成器

  std::array<MyCache::CachePolicy<int, std::string>*, 3> caches = {&lru, &lfu, &arc};
  std::vector<int> hits(3, 0);
  std::vector<int> get_operations(3, 0);

  for (int i = 0; i < caches.size(); ++i) {
    // 填充LOOP_SIZE个数据
    for (int key = 0; key < LOOP_SIZE; ++key) {
      std::string value = "loop" + std::to_string(key);
      caches[i]->put(key, value);
    }
    // 访问测试
    int current_pos = 0;
    for (int op = 0; op < OPERATIONS; ++op) {
      int key;
      // 60% 顺序扫描
      if (op % 100 < 60) {
        key = current_pos;
        current_pos = (current_pos + 1) % LOOP_SIZE;
      } else if (op % 100 < 90) {
        key = gen() % LOOP_SIZE;
      } else {
        key = LOOP_SIZE + gen() % LOOP_SIZE; 
      }
      
      std::string result;
      get_operations[i]++;
      if (caches[i]->get(key, result)) {
        ++hits[i];
      }
    }

    printResult("循环扫描测试", CAPACITY, hits, get_operations);
  }
}


void testWorkkLoadShift() {
  std::cout << "\n ===== 测试场景3: 工作负载剧烈变化测试 ===== \n";
  const int CAPACITY = 4;        // 缓存容量
  const int DATA_SIZE = 1000;         
  const int OPERATIONS = 80000;  // 操作次数
  const int HOT_KEYS = 5;   // 热点访问数据量
  const int PHASE_LENGTH = OPERATIONS / HOT_KEYS;


  MyCache::LruCache<int, std::string> lru(CAPACITY);
  MyCache::LfuCache<int, std::string> lfu(CAPACITY);
  MyCache::ArcCache<int, std::string> arc(CAPACITY);
  
  std::random_device rd;
  std::mt19937 gen(rd());  // 随机数生成器

  std::array<MyCache::CachePolicy<int, std::string>*, 3> caches = {&lru, &lfu, &arc};
  std::vector<int> hits(3, 0);
  std::vector<int> get_operations(3, 0);

  for (int i = 0; i < caches.size(); ++i) {
    // 填充LOOP_SIZE个数据
    for (int key = 0; key < DATA_SIZE; ++key) {
      std::string value = "init" + std::to_string(key);
      caches[i]->put(key, value);
    }

    // 访问测试
    for (int op = 0; op < OPERATIONS; ++op) {
      int key;
      // 根据不同阶段选择不同的访问模式
      if (op < PHASE_LENGTH) {
        
      } else if (op < PHASE_LENGTH * 2) { // 大范围随机访问
        key = gen() % DATA_SIZE;
      } else if (op < PHASE_LENGTH * 3) { // 顺序扫描
        key = (op - PHASE_LENGTH * 2) % 100;
      } else if (op < PHASE_LENGTH * 4) { // 局部性随机
        int locality = (op % DATA_SIZE) % 10;
        key = locality * 20 + gen() % 20;
      } else {  // 混合访问
        int r = gen() % 100;
        if (r < 30) {
          key = gen() % HOT_KEYS;
        } else if (r < 60) {
          key = 5 + gen() % 95;
        } else {
          key = 100 + gen() % 900;
        }
      }
      
      std::string result;
      get_operations[i]++;
      if (caches[i]->get(key, result)) {
        ++hits[i];
      }

      // 随机put
      if (gen() % 100 < 30) {
        std::string value = "new" + std::to_string(key);
        caches[i]->put(key, value);
      }
    }

    printResult("工作负载剧烈变化测试", CAPACITY, hits, get_operations);
  }

}

int main() {
  // 测试代码
  testHotDataAccess();
  testLoopPattern();
  testWorkkLoadShift();
  
  return 0;
}