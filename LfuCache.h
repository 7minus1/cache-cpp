#pragma once

#include <cmath>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "CachePolicy.h"

namespace MyCache {
  template<typename Key, typename Value> class LfuCache;

  template<typename Key, typename Value>
  class FreqList {
  private:
    struct Node
    {
      /* data */
      int freq;   // 访问频次
      Key key;
      Value value;
      std::shared_ptr<Node> prev;  // 上一结点
      std::shared_ptr<Node> next;

      Node() : freq(1), prev(nullptr), next(nullptr) {}
      Node(Key key, Value value) : freq(1), key(key), value(value), pre(nullptr), next(nullptr) {}
    };

    using NodePtr = std::shared_ptr<Node>;
    int freq_;  // 访问频率
    NodePtr dummyHead_;  // 头结点
    NodePtr dummyTail_;
  public:
    explicit FreqList(int n) : freq_(n) {
      dummyHead_ = std::make_shared<Node>();
      dummyTail_ = std::make_shared<Node>();
      dummyHead_->next = dummyTail_;
      dummyTail_->prev = dummyHead_;
    }

    bool isEmpty() const {
      return dummyHead_->next == dummyTail_;
    }

    void addNode(NodePtr node) {
      if (!node || !dummyHead_ || !dummyTail_) {
        return;
      }
      node->prev = dummyTail_->prev;  // 差在尾部，虚拟尾节点之前
      node->next = dummyTail_;
      dummyTail_->prev->next = node;
      dummyHead_->prev = node;
    }

    void removeNode(NodePtr node) {
      if (!node || !dummyHead_ || !dummyTail_) {
        return;
      }
      if (!node->prev || !node->next) {
        return;
      }

      node->prev->next = node->next;
      node->next->prev = node->prev;
      node->prev = node->next = nullptr;
    }

    NodePtr getFirstNode() const {
      return dummyHead_->next;
    }

    friend class LfuCache<Key, Value>;
  };

  template<typename Key, typename Value>
  class LfuCache : public CachePolicy<Key, Value> {
  public:
    using Node = typename FreqList<Key, Value>::Node;
    using NodePtr = std::shared_ptr<Node>;
    using NodeMap = std::unordered_map<Key, NodePtr>;
  private:
    int capacity_;  // 缓存容量
    int minFreq_;   // 最小访问频次（用于找到最小访问频次结点）
    int maxAvgNum_; // 最大平均访问次数
    int curAvgNum_; // 当前平均访问频次
    int curTotalNum_;   // 当前访问所有缓存次数综述
    std::mutex mutex_;  // 互斥锁
    NodeMap nodeMap_;   // key -> 缓存结点
    std::unordered_map<int, FreqList<Key, Value>*> freqToFreqList_;   // 访问频次 -> 该频次链表
  
  public:
    LfuCache(int capacity, int maxAvgNum = 10) : capacity_(capacity), minFreq_(INT8_MAX), maxAvgNum_(maxAvgNum), curAvgNum_(0), curTotalNum_(0) {}

    ~LfuCache() override = default;

    void put(Key key, Value value) override {
      if (capacity_ == 0) {
        return;
      }
      
      std::lock_guard<std::mutex> lock(mutex_);
      auto it = nodeMap_.find(key);
      if (it != nodeMap_.end()) {
        it->second->value = value;  // 重置value值
        
        return;
      }

    }

    bool get(Key key, Value& value) override {
      std::lock_guard<std::mutex> lock(mutex_);
      auto it = nodeMap_.find(key);
      if (it != nodeMap_.end()) {
        
        return true;
      }
      return false;
    }

    Value get(Key key) override {
      Value value;
      get(key, value);
      return value;
    }

    // 清空缓存，回收资源
    void purge() {
      nodeMap_.clear();
      freqToFreqList_.clear();
    }

  private:
    void putInternal(Key key, Value value);  // 添加缓存
    void getInternal(NodePtr node, Value& value);  // 获取缓存
    void kickOut();  // 移出缓存中的过期缓存

    void removeFromFreqList(NodePtr node);  // 从频次链表中移除结点
    void addToFreqList(NodePtr node);  // 添加到频次链表中

    void addFreqNum();      // 增加平均访问等频率
    void decreaseFreqNum(int num); // 减少平均访问等频率
    void handleOverMaxAvgNum();  // 处理当前平均访问频率超过上限的情况
    void updateMinFreq();   // 更新最小访问频次
  };

  template<typename Key, typename Value>
  void LfuCache<Key, Value>::putInternal(Key key, Value value) {
    // 如果不在缓存中，需要先判断缓存是否已满
    if (nodeMap_.size() >= capacity_) {
      kickOut();  // 删除最不常访问的结点
    }
    NodePtr newNode = std::make_shared<Node>(key, value);
    nodeMap_[key] = newNode;
    addToFreqList(newNode);
    addFreqNum();
    minFreq_ = std::min(minFreq_, 1);
  }

  template<typename Key, typename Value>
  void LfuCache<Key, Value>::getInternal(NodePtr node, Value& value) {
    value = node->value;  // value值返回
    // 找到之后需要更新结点到对应的的freqList
    removeFromFreqList(node);
    node->freq++;         // 访问频次+1
    addToFreqList(node);

    // 更新最小访问频次
    if (node->freq == minFreq_ + 1 && freqToFreqList_[minFreq_]->isEmpty()) {
      minFreq_++;
    }

    addFreqNum();
  }

  template<typename Key, typename Value>
  void LfuCache<Key, Value>::kickOut() {
    NodePtr node = freqToFreqList_[minFreq_]->getFirstNode();
    removeFromFreqList(node);
    nodeMap_.erase(node->key);
    decreaseFreqNum(node->freq);
  }

  template<typename Key, typename Value>
  void LfuCache<Key, Value>::addFreqNum() {
    curTotalNum_++; 
    if (nodeMap_.empty()) { // 避免除0
      curAvgNum_ = 0;
    } else {
      curAvgNum_ = curTotalNum_ / nodeMap_.size();
    }
    if (curAvgNum_ > maxAvgNum_) {
      handleOverMaxAvgNum(); 
    }
  }

  template<typename Key, typename Value>
  void LfuCache<Key, Value>::decreaseFreqNum(int num) {
    curTotalNum_-= num;
    if (nodeMap_.empty()) {
      curAvgNum_ = 0;
    } else {
      curAvgNum_ = curTotalNum_ / nodeMap_.size();
    }
  }

  template<typename Key, typename Value>
  void LfuCache<Key, Value>::handleOverMaxAvgNum() {
    if (nodeMap_.empty()) {
      return;
    }

    // 所有结点访问频次 - (maxAvgNum_ / 2)
    for (auto it = nodeMap_.begin(); it != nodeMap_.end(); ++it) {
      if (!it->second) {  // 结点为空
        continue;
      }
      Node node = it->second;
      removeFromFreqList(node);  // 从频次链表中移除
      node->freq -= (maxAvgNum_ / 2);   // 减去频率
      if (node->freq <= 0) {
        node->freq = 1;
      }
      addToFreqList(node);  // 重新添加到对应的频次链表中
    }

    // 更新最小访问频次
    updateMinFreq();
  }

  template<typename Key, typename Value>
  void LfuCache<Key, Value>::updateMinFreq() {
    minFreq_ = INT8_MAX;
    for (auto it = freqToFreqList_.begin(); it!= freqToFreqList_.end(); ++it) {
      if (it->second && !it->second->isEmpty()) {
        minFreq_ = std::min(minFreq_, it->first);
      } 
    }
    if (minFreq_ == INT8_MAX) {
      minFreq_ = 1;   // TODO: 设为1还是0?
    }
  }

  template<typename Key, typename Value>
  void LfuCache<Key, Value>::removeFromFreqList(NodePtr node) {
    if (!node) {
      return;
    }

    auto it = freqToFreqList_.find(node->freq);
    if (it!= freqToFreqList_.end()) {
      it->second->removeNode(node);
    }
  }

  template<typename Key, typename Value>
  void LfuCache<Key, Value>::addToFreqList(NodePtr node) {
    if (!node) {
      return;
    }

    auto it = freqToFreqList_.find(node->freq);
    if (it == freqToFreqList_.end()) {
      // 不存在则创建新的链表
      freqToFreqList_[node->freq] = new FreqList<Key, Value>(node->freq); 
    }

    it->second->addNode(node);
  }

  // HashLfuCache
  template<typename Key, typename Value>
  class HashLfuCache {
  private:
    size_t capacity_; // 缓存总容量
    int sliceNum_;    // 缓存分片数量
    std::vector<std::unique_ptr<LfuCache<Key, Value>>> lfuSliceCaches_; // 缓存lfu分片容器

    // 将key计算成对应哈希值
    size_t Hash(Key key) {
      std::hash<Key> hashFunc;
      return hashFunc(key);
    }
  public:
    HashLfuCache(size_t capacity, int sliceNum, int maxAvgNum = 10) 
      : capacity_(capacity), sliceNum_(sliceNum > 0 ? sliceNum : std::thread::hardware_concurrency()) {
      size_t sliceSize = std::ceil(capacity_ / static_cast<double>(sliceNum_));   // 每个lfu分片的容量
      for (int i = 0; i < sliceNum_; ++i) {
        lfuSliceCaches_.emplace_back(new LfuCache<Key, Value>(sliceSize, maxAvgNum));
      }
    }

    void put(Key key, Value value) {
      // 根据key找到对应的lfu分片
      size_t index = Hash(key) % sliceNum_;
      lfuSliceCaches_[index]->put(key, value);
    }

    bool get(Key key, Value& value) {
      size_t index = Hash(key) % sliceNum_;
      return lfuSliceCaches_[index]->get(key, value);
    }

    Value get(Key key) {
      Value value;
      get(key, value);
      return value;
    }

    // 清空缓存，回收资源
    void purge() {
      for (auto& lfuSliceCache : lfuSliceCaches_) {
        lfuSliceCache->purge();
      }  
    }
  };
} // namespace MyCache
