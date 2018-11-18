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

#include <cnet/http/HttpRequest.h>
#include <cnet/http/HttpResponse.h>
#include "http/FunctionTraits.h"
#include <cnet/http/HttpObject.h>
#include <list>
#include <string>
#include <sstream>
#include <iostream>
#include <memory>
namespace cnet
{
//we only accept value type or const lreference type as handle method parameters type
template <typename T>
struct BinderArgTypeTraits
{
    static const bool isValid = true;
};
template <typename T>
struct BinderArgTypeTraits<T *>
{
    static const bool isValid = false;
};
template <typename T>
struct BinderArgTypeTraits<T &>
{
    static const bool isValid = false;
};
template <typename T>
struct BinderArgTypeTraits<T &&>
{
    static const bool isValid = true;
};
template <typename T>
struct BinderArgTypeTraits<const T &&>
{
    static const bool isValid = false;
};
template <typename T>
struct BinderArgTypeTraits<const T &>
{
    static const bool isValid = true;
};

class HttpBinderBase
{
  public:
    virtual void handleHttpRequest(std::list<std::string> &pathParameter,
                                      const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> callback) = 0;
    virtual size_t paramCount() = 0;
    virtual ~HttpBinderBase() {}

  protected:
    static std::map<std::string, std::shared_ptr<cnet::HttpObjectBase>> _objMap;
    static std::mutex _objMutex;
};
typedef std::shared_ptr<HttpBinderBase> HttpBinderBasePtr;
template <typename FUNCTION>
class HttpBinder : public HttpBinderBase
{
  public:
    typedef FUNCTION FunctionType;
    virtual void handleHttpRequest(std::list<std::string> &pathParameter,
                                      const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> callback) override
    {
        run(pathParameter, req, callback);
    }
    virtual size_t paramCount() override
    {
        return traits::arity;
    }
    HttpBinder(FUNCTION &&func) : _func(std::forward<FUNCTION>(func))
    {
        static_assert(traits::isHTTPFunction, "Your API handler function interface is wrong!");
    }
    void test()
    {
        std::cout << "argument_count=" << argument_count << " " << traits::isHTTPFunction << std::endl;
    }

  private:
    FUNCTION _func;

    typedef utility::FunctionTraits<FUNCTION> traits;
    template <
        std::size_t Index>
    using nth_argument_type = typename traits::template argument<Index>;

    static const size_t argument_count = traits::arity;

    template <
        typename... Values,
        std::size_t Boundary = argument_count>
    typename std::enable_if<(sizeof...(Values) < Boundary), void>::type run(
        std::list<std::string> &pathParameter,
        const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> callback,
        Values &&... values)
    {
        //call this function recursively until parameter's count equals to the count of target function parameters
        static_assert(BinderArgTypeTraits<nth_argument_type<sizeof...(Values)>>::isValid,
                      "your handler argument type must be value type or const left reference type or right reference type");
        typedef typename std::remove_cv<typename std::remove_reference<nth_argument_type<sizeof...(Values)>>::type>::type ValueType;
        ValueType value = ValueType();
        if (!pathParameter.empty())
        {
            std::string v = std::move(pathParameter.front());
            pathParameter.pop_front();
            if (!v.empty())
            {
                std::stringstream ss(std::move(v));
                ss >> value;
            }
        }

        run(pathParameter, req, callback, std::forward<Values>(values)..., std::move(value));
    }
    template <
        typename... Values,
        std::size_t Boundary = argument_count>
    typename std::enable_if<(sizeof...(Values) == Boundary), void>::type run(
        std::list<std::string> &pathParameter,
        const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> callback,
        Values &&... values)
    {
        callFunction(req, callback, std::move(values)...);
    }
    template <typename... Values,
              bool isClassFunction = traits::isClassFunction>
    typename std::enable_if<isClassFunction, void>::type callFunction(
        const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> callback,
        Values &&... values)
    {
        static auto className = cnet::HttpObjectBase::demangle(typeid(typename traits::class_type).name());
        std::shared_ptr<typename traits::class_type> obj;
        {
            std::lock_guard<std::mutex> guard(_objMutex);
            if (_objMap.find(className) == _objMap.end())
            {
                obj = std::shared_ptr<typename traits::class_type>(new typename traits::class_type);
                _objMap[className] = obj;
            }
            else
            {
                obj = std::dynamic_pointer_cast<typename traits::class_type>(_objMap[className]);
            }
        }
        assert(obj);
        ((*obj).*_func)(req, callback, std::move(values)...);
    };
    template <typename... Values,
              bool isClassFunction = traits::isClassFunction>
    typename std::enable_if<!isClassFunction, void>::type callFunction(
        const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> callback,
        Values &&... values)
    {
        _func(req, callback, std::move(values)...);
    };
};
} // namespace cnet
