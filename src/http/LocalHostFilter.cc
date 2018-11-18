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

#include <cnet/http/LocalHostFilter.h>
#include "HttpResponseImpl.h"
using namespace cnet;
void LocalHostFilter::doFilter(const HttpRequestPtr &req,
                               const FilterCallback &fcb,
                               const FilterChainCallback &fccb)
{
    if (req->peerAddr().toIp() == "127.0.0.1")
    {
        fccb();
        return;
    }
    auto res = cnet::HttpResponse::newNotFoundResponse();
    fcb(res);
}
