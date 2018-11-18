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

#include <cnet/http/HttpFilter.h>

namespace cnet
{
class InnerIpFilter : public HttpFilter<InnerIpFilter>
{
  public:
    InnerIpFilter() {}
    virtual void doFilter(const HttpRequestPtr &req,
                          const FilterCallback &fcb,
                          const FilterChainCallback &fccb) override;
};
} // namespace cnet
