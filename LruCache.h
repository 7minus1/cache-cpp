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
      Value value{};
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
}