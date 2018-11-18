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

#include <cnet/http/HttpClient.h>
#include <cnet/tcp/EventLoop.h>
#include <cnet/tcp/TcpClient.h>
#include <mutex>
#include <queue>
namespace cnet
{
class HttpClientImpl : public HttpClient, public std::enable_shared_from_this<HttpClientImpl>
{
  public:
    HttpClientImpl(cnet::EventLoop *loop, const cnet::InetAddress &addr, bool useSSL = false);
    HttpClientImpl(cnet::EventLoop *loop, const std::string &hostString);
    virtual void sendRequest(const HttpRequestPtr &req, const HttpReqCallback &callback) override;
    ~HttpClientImpl();

  private:
    std::shared_ptr<cnet::TcpClient> _tcpClient;
    cnet::EventLoop *_loop;
    cnet::InetAddress _server;
    bool _useSSL;
    void sendReq(const cnet::TcpConnectionPtr &connectorPtr, const HttpRequestPtr &req);
    void sendRequestInLoop(const HttpRequestPtr &req, const HttpReqCallback &callback);
    std::queue<std::pair<HttpRequestPtr, HttpReqCallback>> _reqAndCallbacks;
    void onRecvMessage(const cnet::TcpConnectionPtr &, cnet::MsgBuffer *);
    std::string _domain;
};
} // namespace cnet
