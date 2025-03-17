#pragma once

namespace MyCache {
  template <typename Key, typename Value>
  class CachePolicy {
  public:
    virtual ~CachePolicy() {};

    // 缓存接口
    virtual void put(Key key, Value value) = 0;
    // 访问成功true，值以传出参数形式返回
    virtual bool get(Key key, Value& value) = 0;
    // 返回value
    virtual Value get(Key key) = 0;
  };
}