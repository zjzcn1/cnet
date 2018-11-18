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

#include <stdio.h>
#include <unordered_map>
#include <memory>
#include <functional>

#include <thread>
#include <mutex>
#include <vector>

namespace cnet
{
class HttpObjectBase;
typedef std::function<HttpObjectBase *()> HttpAllocFunc;
class HttpClassMap
{
  public:
    static void registerClass(const std::string &className, const HttpAllocFunc &func);
    static HttpObjectBase *newObject(const std::string &className);
    static std::vector<std::string> getAllClassName();

  protected:
    static std::unordered_map<std::string, HttpAllocFunc> &getMap();
};
} // namespace cnet
