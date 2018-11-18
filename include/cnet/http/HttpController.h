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

#include <cnet/http/HttpObject.h>
#include <cnet/http/HttpApp.h>
#include <cnet/utils/Logger.h>
#include <string>
#include <vector>
#include <iostream>

#define METHOD_LIST_BEGIN     \
    static void initMethods() \
    {

#define METHOD_ADD(method, pattern, filters...)      \
    {                                                \
        registerMethod(&method, pattern, {filters}); \
    }

#define METHOD_LIST_END \
    return;             \
    }                   \
                        \
  protected:

namespace cnet
{
template <typename T>
class HttpController : public HttpObject<T>
{
  protected:
    template <typename FUNCTION>
    static void registerMethod(FUNCTION &&function,
                               const std::string &pattern,
                               const std::vector<any> &filtersAndMethods = std::vector<any>())
    {
        std::string path = std::string("/") + HttpController<T>::classTypeName();
        LOG_TRACE << "classname:" << HttpController<T>::classTypeName();

        //transform(path.begin(), path.end(), path.begin(), tolower);
        std::string::size_type pos;
        while ((pos = path.find("::")) != std::string::npos)
        {
            path.replace(pos, 2, "/");
        }
        if (pattern.empty() || pattern[0] == '/')
            app().registerHttpMethod(path + pattern,
                                        std::forward<FUNCTION>(function),
                                        filtersAndMethods);
        else
            app().registerHttpMethod(path + "/" + pattern,
                                        std::forward<FUNCTION>(function),
                                        filtersAndMethods);
    }

  private:
    class methodRegister
    {
      public:
        methodRegister()
        {
            T::initMethods();
        }
    };
    //use static value to register controller method in framework before main();
    static methodRegister _register;
    virtual void *touch()
    {
        return &_register;
    }
};
template <typename T>
typename HttpController<T>::methodRegister HttpController<T>::_register;
} // namespace cnet
