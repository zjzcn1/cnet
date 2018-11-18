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

#include <cnet/http/HttpClassMap.h>
#include <iostream>
using namespace cnet;
//std::map <std::string,HttpAllocFunc> * HttpClassMap::classMap=nullptr;
//std::once_flag HttpClassMap::flag;
void HttpClassMap::registerClass(const std::string &className, const HttpAllocFunc &func)
{
    //std::cout<<"register class:"<<className<<std::endl;

    getMap().insert(std::make_pair(className, func));
}
HttpObjectBase *HttpClassMap::newObject(const std::string &className)
{
    auto iter = getMap().find(className);
    if (iter != getMap().end())
    {
        return iter->second();
    }
    else
        return nullptr;
}
std::vector<std::string> HttpClassMap::getAllClassName()
{
    std::vector<std::string> ret;
    for (auto iter : getMap())
    {
        ret.push_back(iter.first);
    }
    return ret;
}
std::unordered_map<std::string, HttpAllocFunc> &HttpClassMap::getMap()
{
    static std::unordered_map<std::string, HttpAllocFunc> map;
    return map;
}
