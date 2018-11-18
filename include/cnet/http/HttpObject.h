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

#include <cnet/http/HttpClassMap.h>

#include <cxxabi.h>
#include <string>
namespace cnet
{
class HttpObjectBase
{
  public:
    virtual const std::string &className() const
    {
        static std::string _name = "HttpObjectBase";
        return _name;
    }
    virtual bool isClass(const std::string &className_) const
    {
        if (className() == className_)
            return true;
        return false;
    }
    static const std::string demangle(const char *mangled_name)
    {
        std::size_t len = 0;
        int status = 0;
        std::unique_ptr<char, decltype(&std::free)> ptr(
            __cxxabiv1::__cxa_demangle(mangled_name, nullptr, &len, &status), &std::free);
        return ptr.get();
    }
    virtual ~HttpObjectBase() {}
};

/*
    * a class template to
    * implement the reflection function of creating the class object by class name
    * */
template <typename T>
class HttpObject : public virtual HttpObjectBase
{
  public:
    virtual const std::string &className() const override
    {
        return _alloc.className();
    }
    static const std::string &classTypeName()
    {
        return _alloc.className();
    }

    virtual bool isClass(const std::string &className_) const override
    {
        if (className() == className_)
            return true;
        return false;
    }

  protected:
    //protect constructor to make this class only inheritable
    HttpObject() {}

  private:
    class HttpAllocator
    {
      public:
        HttpAllocator()
        {
            HttpClassMap::registerClass(className(), []() -> HttpObjectBase * {
                T *p = new T;
                return static_cast<HttpObjectBase *>(p);
            });
        }
        const std::string &className() const
        {
            static std::string className = demangle(typeid(T).name());
            return className;
        }
    };

    //use static val to register allocator function for class T;
    static HttpAllocator _alloc;
};
template <typename T>
typename HttpObject<T>::HttpAllocator HttpObject<T>::_alloc;
} // namespace cnet
