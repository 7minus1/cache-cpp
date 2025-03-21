#pragma once

#include <cstring>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "CachePolicy.h"

namespace MyCache {
  template <typename Key, typename Value> class LruCache;
  template <typename Key, typename Value>
  class LruNode {
  private:
    Key key_;
    Value value_;
    size_t accessCount_;  // 访问次数
    std::shared_ptr<LruNode<Key, Value>> prev_;
    std::shared_ptr<LruNode<Key, Value>> next_;
  public:
    LruNode(Key key, Value value) : key_(key), value_(value), accessCount_(1), prev_(nullptr), next_(nullptr) {}

    Key getKey() const { return key_; }
    Value getValue() const { return value_; }
    void setValue(const Value& value) { value_ = value; }
    size_t getAccessCount() const { return accessCount_; }
    void incrementAccessCount() { ++accessCount_; }

    friend class LruCache<Key, Value>;
  };

  template<typename Key, typename Value>
  class LruCache : public CachePolicy<Key, Value> {
  public:
    using LruNodetype = LruNode<Key, Value>;
    using NodePtr = std::shared_ptr<LruNodeType>;
    using NodeMap = std::unordered_map<Key, NodePtr>;
  private:
    int capacity_;  // 缓存容量
    NodeMap nodeMap_; // key -> Node
    std::mutex mutex_;
    NodePtr dummyHead_; // 虚拟头节点
    NodePtr dummyTail_;
  public:
    LruCache(int capacity) : capacity_(capacity) {

    }

    ~LruCache() override = default;

    // 添加缓存
    void put(Key key, Value value) override {
      if (capacity_ <= 0) {
        return;
      }

      std::lock_guard<std::mutex> lock(mutex_);
      auto it = nodeMap_.find(key);
      if (it != nodeMap_.end()) {
        updateExistingNode(node, value);
        return;
      }
      
    }

    bool get(Key key, Value& value) override {
      std::lock_guard<std::mutex> lock(mutex_);
      auto it = nodeMap_.find(key);
      if (it != nodeMap_.end()) {
        move2MostRecent(it->second);
        value = it->second->getValue();
        return true;
      }
      return false;
    }

    Value get(Key key) override {
      Value value{};  // 初始化为空
      get(key, value);
      return value;
    }

    // 删除指定元素
    void remove(Key key) {
      std::lock_guard<std::mutex> lock(mutex_);
      auto it = nodeMap_.find(key);
      if (it != nodeMap_.end()) {
        removeNode(it->second);
        nodeMap_.erase(it);
      }
    }
  private:
    void initList() {
      dummyHead_ = std::make_shared<LruNodetype>(Key(), Value());
      dummyTail_ = std::make_shared<LruNodetype>(Key(), Value());
      dummyHead_ -> next_ = dummyTail_;
      dummyTail_ -> prev_ = dummyHead_;
    }

    void removeNode(NodePtr node) {
      node->prev_->next_ = node->next_;
      node->next_->prev_ = node->prev_;
    }

    // 从尾部插入
    void insertNode(NodePtr node) {
      node->next_ = dummyTail_->next;
      node->prev_ = dummyTail_->prev_;
      dummyTail_->prev_->next_ = node;
      dummyTail_->prev_ = node;
    }

    // 驱逐最近最少访问
    void evictLeastRecent() {
      NodePtr leastRecent = dummyHead_->next_;  // 队头
      removeNode(leastRecent);
      nodeMap_.erase(leastRecent->getKey());
    }

    // 移动到最新位置
    void move2MostRecent(NodePtr node) {
      removeNode(node);
      insertNode(node);
    }

    // 更新缓存中的节点值
    void updateExistingNode(NodePtr node, const Value& value) {
      node->setValue(value);
      move2MostRecent(node);
    }

    // 添加新节点
    void addNewNode(const Key& key, const Value& value) {
      if (nodeMap_.size() >= capacity_) {
        evictLeastRecent();
      }
      NodePtr newNode = std::make_shared<LruNodetype>(key, value);
      insertNode(newNode);
      nodeMap_[key] = newNode;
    }
  };
  
  // 优化：LRU-k
  template<typename Key, typename Value>
  class LruKCache : public LruCache<Key, Value> {
  private:
    int k_;   // 进入缓存队列的评判标准
    std::unique_ptr<LruCache<Key, size_t>> historyList_;  // 访问数据历史记录(value=访问次数)
  public:
    LruKCache(int capacity, int historyCapacity, int k) 
      : LruKCache<Key, Value>(capacity), historyList_(std::make_unique<LruCache<Key, size_t>>(historyCapacity)), k_(k)
      {}
    
    Value get(Key key) {
      // 获取访问次数
      size_t historyCount = historyList_->get(key);
      // 存在，count++
      historyList_->put(key, ++historyCount);
      // 读数据（不一定能获取到）
      return LruCache<Key, Value>::get(key);
    }

    void put(Key key, Value value) {
      // 判断是否存在缓存中
      if (LruCache<Key, Value>::get(key) != "") {
        LruCache<Key, Value>::put(key, value);  // 存在，直接覆盖
      }

      // 获取访问次数
      size_t historyCount = historyList_->get(key);
      historyList_->put(key, ++historyCount);

      // 次数达到上限，添加入缓存
      if (historyCount >= k_) {
        historyList_->remove(key);
        LruCache<Key, Value::put(key, value);
      }
    }
  };

  // 优化：lru分片，提高高并发使用性能 (没有继承)
  template<typename Key, typename Value>
  class HashLruCaches {
  private:
    size_t capacity_; // 总容量
    int sliceNum_;    // 切片数量
    std::vector<std::unique_ptr<LruCache<Key, Value>>> lruSliceCaches_; // 切片lru缓存

    // 将key转为对应Hash值
    size_t Hash(Key key) {
      std::hash<Key> hashFunc;
      return hashFunc(key);
    }
  public:
    HashLruCaches(size_t capacity, int sliceNum)
      : capacity_(capacity), sliceNum_(sliceNum > 0 ? sliceNum : std::thread::hardware_concurrency()) {
      size_t sliceSize = std::ceil(capacity / static_cast<double>(sliceNum_));  // 获取每个分片的大小
      for (int i = 0; i < sliceNum_; ++i) {
        lruSliceCaches_.emplace_back(new LruCache<Key, Value>(sliceSize));
      }
    }

    void put(Key key, Value value) {
      // 获取key的hash值，计算对应的分片索引
      size_t sliceIndex = Hash(key) % sliceNum_;
      return lruSliceCaches_[sliceIndex]->put(key, value);
    }

    bool get(Key key, Value& value) {
      size_t sliceIndex = Hash(key) % sliceNum_;
      return lruSliceCaches_[sliceIndex]->get(key, value);
    }
    
    Value get(Key key) {
      Value value{};
      get(key, value);
      return value;
    }
  };
  
}