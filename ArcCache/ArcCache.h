
#include "../CachePolicy.h"
#include "ArcLruPart.h"
#include "ArcLfuPart.h"


namespace MyCache {
  template <typename Key, typename Value>
  class ArcCache : public CachePolicy<Key, Value> {
  private:
    size_t capacity_;
    size_t transformThreshold_;  // 转换门槛值
    std::shared_ptr<ArcLfuPart<Key, Value>> lfuPart_;
    std::shared_ptr<ArcLruPart<Key, Value>> lruPart_;

    bool checkGhostCaches(Key key) {
      bool inGhost = false;
      if (lruPart_->checkGhost(key)) {
        if (lfuPart_->decreaseCapacity()) {
          lruPart_->increaseCapacity();
        }
        inGhost = true;
      } else if (lfuPart_->checkGhost(key)) {
        if (lruPart_->decreaseCapacity()) {
          lfuPart_->increaseCapacity();
        }
        inGhost = true; 
      }
      return inGhost;
    }

  public:
    explicit ArcCache(size_t capacity = 10, size_t transformThreshold = 2)
      : capacity_(capacity), transformThreshold_(transformThreshold)
      , lruPart_(std::make_unique<ArcLruPart<Key, Value>>(capacity, transformThreshold))
      , lfuPart_(std::make_unique<ArcLfuPart<Key, Value>>(capacity, transformThreshold)) {}
    
    ~ArcCache() override = default;

    void put(Key key, Value value) override {
      bool inGhost = checkGhostCaches(key);
      if (inGhost) {
        lruPart_->put(key, value);
      } else {
        if (lruPart_->put(key, value);) {
          lfuPart_->put(key, value);
        }
      }
    }

    bool get(Key key, Value& value) override {
      checkGhostCaches(key);
      bool shouldTransform = false;
      if (lruPart_->get(key, value, shouldTransform)) {
        if (shouldTransform) {
          lfuPart_->put(key, value);
        }
        return true;
      }
      return lfuPart_->get(key, value);
    }

    Value get(Key key) override {
      Value value;
      get(key, value);
      return value; 
    }
  };

}