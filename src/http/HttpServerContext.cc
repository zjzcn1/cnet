// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

//taken from muduo and modified

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

#include <cnet/utils/MsgBuffer.h>
#include <cnet/utils/Logger.h>
#include "HttpServerContext.h"
#include "HttpResponseImpl.h"
#include <iostream>
using namespace cnet;
using namespace cnet;
HttpServerContext::HttpServerContext(const cnet::TcpConnectionPtr &connPtr)
    : state_(kExpectRequestLine),
      request_(new HttpRequestImpl),
      _pipeLineMutex(std::make_shared<std::mutex>()),
      _conn(connPtr)
{
}
bool HttpServerContext::processRequestLine(const char *begin, const char *end)
{
    bool succeed = false;
    const char *start = begin;
    const char *space = std::find(start, end, ' ');
    if (space != end && request_->setMethod(start, space))
    {
        start = space + 1;
        space = std::find(start, end, ' ');
        if (space != end)
        {
            const char *question = std::find(start, space, '?');
            if (question != space)
            {
                request_->setPath(start, question);
                request_->setQuery(question + 1, space);
            }
            else
            {
                request_->setPath(start, space);
            }
            start = space + 1;
            succeed = end - start == 8 && std::equal(start, end - 1, "HTTP/1.");
            if (succeed)
            {
                if (*(end - 1) == '1')
                {
                    request_->setVersion(HttpRequest::kHttp11);
                }
                else if (*(end - 1) == '0')
                {
                    request_->setVersion(HttpRequest::kHttp10);
                }
                else
                {
                    succeed = false;
                }
            }
        }
    }
    return succeed;
}

// return false if any error
bool HttpServerContext::parseRequest(MsgBuffer *buf)
{
    bool ok = true;
    bool hasMore = true;
    //  std::cout<<std::string(buf->peek(),buf->readableBytes())<<std::endl;
    while (hasMore)
    {
        if (state_ == kExpectRequestLine)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                ok = processRequestLine(buf->peek(), crlf);
                if (ok)
                {
                    //request_->setReceiveTime(receiveTime);
                    buf->retrieveUntil(crlf + 2);
                    state_ = kExpectHeaders;
                }
                else
                {
                    hasMore = false;
                }
            }
            else
            {
                hasMore = false;
            }
        }
        else if (state_ == kExpectHeaders)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                const char *colon = std::find(buf->peek(), crlf, ':');
                if (colon != crlf)
                {
                    request_->addHeader(buf->peek(), colon, crlf);
                }
                else
                {
                    // empty line, end of header
                    std::string len = request_->getHeader("Content-Length");
                    LOG_TRACE << "content len=" << len;
                    if (len != "")
                    {
                        request_->contentLen = atoi(len.c_str());
                        state_ = kExpectBody;
                        auto expect = request_->getHeader("Expect");
                        if (expect == "100-continue" &&
                            request_->getVersion() >= HttpRequest::kHttp11)
                        {
                            //rfc2616-8.2.3
                            //TODO:here we can add content-length limitation
                            auto connPtr = _conn.lock();
                            if (connPtr)
                            {
                                auto resp = HttpResponse::newHttpResponse();
                                resp->setStatusCode(HttpResponse::k100Continue);
                                MsgBuffer buffer;
                                std::dynamic_pointer_cast<HttpResponseImpl>(resp)
                                    ->appendToBuffer(&buffer);
                                connPtr->send(std::move(buffer));
                            }
                        }
                        else if (!expect.empty())
                        {
                            LOG_WARN << "417ExpectationFailed for \"" << expect << "\"";
                            auto connPtr = _conn.lock();
                            if (connPtr)
                            {
                                auto resp = HttpResponse::newHttpResponse();
                                resp->setStatusCode(HttpResponse::k417ExpectationFailed);
                                MsgBuffer buffer;
                                std::dynamic_pointer_cast<HttpResponseImpl>(resp)
                                    ->appendToBuffer(&buffer);
                                connPtr->send(std::move(buffer));
                                buf->retrieveAll();
                                connPtr->forceClose();

                                //return false;
                            }
                        }
                    }
                    else
                    {
                        state_ = kGotAll;
                        hasMore = false;
                    }
                }
                buf->retrieveUntil(crlf + 2);
            }
            else
            {
                hasMore = false;
            }
        }
        else if (state_ == kExpectBody)
        {
            //LOG_INFO << "expectBody:len=" << request_->contentLen;
            //LOG_INFO << "expectBody:buf=" << buf;
            if (buf->readableBytes() == 0)
            {
                if (request_->contentLen == 0)
                {
                    state_ = kGotAll;
                }
                break;
            }
            if (request_->contentLen >= buf->readableBytes())
            {
                request_->contentLen -= buf->readableBytes();
                request_->content_ += std::string(buf->peek(), buf->readableBytes());
                buf->retrieveAll();
            }
            else
            {
                request_->content_ += std::string(buf->peek(), request_->contentLen);
                buf->retrieve(request_->contentLen);
                request_->contentLen = 0;
            }
            if (request_->contentLen == 0)
            {
                state_ = kGotAll;
                LOG_TRACE << "post got all:len=" << request_->content_.length();
                //LOG_INFO<<"content:"<<request_->content_;
                LOG_TRACE << "content(END)";
                hasMore = false;
            }
        }
    }

    return ok;
}

void HttpServerContext::pushRquestToPipeLine(const HttpRequestPtr &req)
{
    std::pair<HttpRequestPtr, HttpResponsePtr> reqPair(req, HttpResponseImplPtr());

    _requestPipeLine.push_back(std::move(reqPair));
}
HttpRequestPtr HttpServerContext::getFirstRequest() const
{
    if (_requestPipeLine.size() > 0)
    {
        return _requestPipeLine.front().first;
    }
    return HttpRequestImplPtr();
}
HttpResponsePtr HttpServerContext::getFirstResponse() const
{
    if (_requestPipeLine.size() > 0)
    {
        return _requestPipeLine.front().second;
    }
    return HttpResponseImplPtr();
}
void HttpServerContext::popFirstRequest()
{
    _requestPipeLine.pop_front();
}
void HttpServerContext::pushResponseToPipeLine(const HttpRequestPtr &req,
                                               const HttpResponsePtr &resp)
{
    for (auto &iter : _requestPipeLine)
    {
        if (iter.first == req)
        {
            iter.second = resp;
            return;
        }
    }
}

std::mutex &HttpServerContext::getPipeLineMutex()
{
    return *_pipeLineMutex;
}
