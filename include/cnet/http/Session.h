/**
 *
 *  @file
 *  @author An Tao
 *  @section LICENSE
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  cnet
 *
 */

#pragma once

#include <memory>
#include <map>
#include <mutex>
#include <thread>
#include <cnet/config.h>

typedef std::map<std::string, any> SessionMap;
namespace cnet
{
class Session
{
  public:
    template <typename T>
    T get(const std::string &key) const
    {
        std::lock_guard<std::mutex> lck(mutex_);
        auto it = sessionMap_.find(key);
        if (it != sessionMap_.end())
        {
            return *(any_cast<T>(&(it->second)));
        }
        T tmp;
        return tmp;
    };
    any &operator[](const std::string &key)
    {
        std::lock_guard<std::mutex> lck(mutex_);
        return sessionMap_[key];
    };
    void insert(const std::string &key, const any &obj)
    {
        std::lock_guard<std::mutex> lck(mutex_);
        sessionMap_[key] = obj;
    };
    void insert(const std::string &key, any &&obj)
    {
        std::lock_guard<std::mutex> lck(mutex_);
        sessionMap_[key] = std::move(obj);
    }
    void erase(const std::string &key)
    {
        std::lock_guard<std::mutex> lck(mutex_);
        sessionMap_.erase(key);
    }
    bool find(const std::string &key)
    {
        std::lock_guard<std::mutex> lck(mutex_);
        if (sessionMap_.find(key) == sessionMap_.end())
        {
            return false;
        }
        return true;
    }
    void clear()
    {
        std::lock_guard<std::mutex> lck(mutex_);
        sessionMap_.clear();
    }

  protected:
    SessionMap sessionMap_;
    int timeoutInterval_;
    mutable std::mutex mutex_;
};
typedef std::shared_ptr<Session> SessionPtr;
} // namespace cnet
