#pragma once

#include "ArcCacheNode.h"
#include <unordered_map>
#include <map>
#include <mutex>

namespace MyCache {
  template <typename Key, typename Value>
  class ArcLfuPart {
  public:
    using NodeType = ArcNode<Key, Value>;
    using NodePtr = std::shared_ptr<NodeType>;
    using NodeMap = std::unordered_map<Key, NodePtr>; 
    using FreqMap = std::map<size_t, std::list<NodePtr>>;

  private:
    size_t capacity_;
    size_t ghostCapacity_;
    size_t transformThreshold_;  // 转换门槛值
    size_t minFreq_;

    std::mutex mutex_;

    NodeMap mainCache_;
    NodeMap ghostCache_;
    FreqMap freqMap_;

    NodePtr ghostHead_;
    NodePtr ghostTail_;
  
  public:
    explicit ArcLfuPart(size_t capacity, size_t transformThreshold)
      : capacity_(capacity), ghostCapacity_(capacity), transformThreshold_(transformThreshold), minFreq_(0) {
        initLists();
    }

    bool put(Key key, Value value) {
      if (capacity_ == 0) {
        return false;
      } 
      std::lock_guard<std::mutex> lock(mutex_);
      auto it = mainCache_.find(key);
      if (it != mainCache_.end()) {
        return updateExistingNode(it->second, value);
      }
      return addNewNode(key, value);
    }

    bool get(Key key, Value& value) {
      std::lock_guard<std::mutex> lock(mutex_);
      auto it = mainCache_.find(key);
      if (it != mainCache_.end()) {
        updateNodeFrequency(it->second);
        value = it->second->getValue();
        return true;
      }
      return false;
    }

    bool checkGhost(Key, key) {
      auto it = ghostCache_.find(key);
      if (it != ghostCache_.end()) {
        removeFromGhost(it->second);
        ghostCache_.erase(it);
        return true;
      }
      return false;
    }

    void increaseCapacity() {
      ++capacity_;
    }

    bool decreaseCapacity() {
      if (capacity_ <= 0) {
        return false;
      }
      if (mainCache_.size() == capacity_) {
        evictLeastFrequency();
      }
      --capacity_;
      return true;
    }

  private:
    void initLists() {
      ghostHead_ = std::make_shared<NodeType>();
      ghostTail_ = std::make_shared<NodeType>();
      ghostHead_->next_ = ghostTail_;
      ghostTail_->prev_ = ghostHead_;
    }

    bool updateExistingNode(NodePtr node, const Value& value) {
      node->setValue(value);
      updateNode
      return true;
    }

    bool addNewNode(const Key& key, const Value& value) {
      if (mainCache_.size() >= capacity_) {
        evictLeastFrequency();
      }
      NodePtr newNode = std::make_shared<NodeType>(key, value);
      mainCache_[key] = newNode;
      // 将新节点添加到频率为1的列表中
      if (freqMap_.find(1) == freqMap_.end()) {
        freqMap_[1] = std::list<NodePtr>();
      }
      freqMap_[1].push_back(newNode);
      minFreq_ = 1;
      return true;
    }

    void updateNodeFrequency(NodePtr node) {
      size_t oldFreq = node->getAccessCount();
      node->incrementAccessCount();
      size_t newFreq = node->getAccessCount();

      // 从旧频率列表中移除节点
      auto& oldFreqList = freqMap_[oldFreq];
      oldFreqList.remove(node);
      if (oldFreqList.empty()) {
        freqMap_.erase(oldFreq);
        if (minFreq_ == oldFreq) {
          minFreq_ = newFreq;
        }
      }

      // 添加到新频率列表
      if (freqMap_.find(newFreq) == freqMap_.end()) {
        freqMap_[newFreq] = std::list<NodePtr>();
      }
      freqMap_[newFreq].push_back(node);
    }

    void evictLeastFrequency() {
      if (freqMap_.empty()) {
        return;
      }
      // 获取最小频率列表
      auto& minFreqList = freqMap_[minFreq_];
      if (minFreqList.empty()) {
        freqMap_.erase(minFreq_);
        return;
      }

      // 移除最少使用节点
      NodePtr leastNode = minFreqList.front();
      minFreqList.pop_front();

      // 移除节点后，然后频率列表为空
      if (minFreqList.empty()) {
        freqMap_.erase(minFreq_);
        // 更新最小频率
        if (!freqMap_.empty()) {
          minFreq_ = freqMap_.begin()->first;
        }
      }

      // 将节点移到幽灵缓存
      if (ghostCache_.size() >= ghostCapacity_) {
        removeOldestGhost();
      }
      addToGhost(leastNode);
      // 从主缓存中移除
      mainCache_.erase(leastNode->getKey());
    }

    void removeOldestGhost() {
      NodePtr oldestGhost = ghostHead_->next_;
      if (oldestGhost == ghostTail_) {
        return;
      }
      removeFromGhost(oldestGhost);
      ghostCache_.erase(oldestGhost->getKey());
    }

    void addToGhost(NodePtr node) {
      node->prev_ = ghostTail_->prev_;
      node->next_ = ghostTail_;
      ghostTail_->prev_->next_ = node;
      ghostTail_->prev_ = node;
      ghostCache_[node->getKey()] = node;
    }

    void removeFromGhost(NodePtr node) {
      node->prev_->next_ = node->next_;
      node->next_->prev_ = node->prev_;
    }
  };

}