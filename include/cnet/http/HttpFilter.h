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
#include <cnet/http/HttpResponse.h>
#include <cnet/http/HttpRequest.h>
#include <memory>

namespace cnet
{
typedef std::function<void(HttpResponsePtr)> FilterCallback;
typedef std::function<void()> FilterChainCallback;
class HttpFilterBase : public virtual HttpObjectBase
{
  public:
    virtual void doFilter(const HttpRequestPtr &req,
                          const FilterCallback &fcb,
                          const FilterChainCallback &fccb) = 0;
    virtual ~HttpFilterBase() {}
};
template <typename T>
class HttpFilter : public HttpObject<T>, public HttpFilterBase
{
  public:
    virtual ~HttpFilter() {}
};
} // namespace cnet
