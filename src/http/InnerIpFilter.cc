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

#include <cnet/http/InnerIpFilter.h>
#include "HttpResponseImpl.h"
using namespace cnet;
void InnerIpFilter::doFilter(const HttpRequestPtr &req,
                             const FilterCallback &fcb,
                             const FilterChainCallback &fccb)
{
    if (req->peerAddr().isInnerIp())
    {
        fccb();
        return;
    }
    auto res = cnet::HttpResponse::newNotFoundResponse();
    fcb(res);
}
