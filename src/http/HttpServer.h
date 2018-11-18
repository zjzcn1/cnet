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

#include "WebSockectConnectionImpl.h"
#include <cnet/config.h>
#include <cnet/tcp/TcpServer.h>
#include <cnet/tcp/callbacks.h>
#include <cnet/utils/NonCopyable.h>
#include <functional>
#include <string>

using namespace cnet;
namespace cnet
{
class HttpRequest;
class HttpResponse;
typedef std::shared_ptr<HttpRequest> HttpRequestPtr;
class HttpServer : cnet::NonCopyable
{
  public:
    typedef std::function<void(const HttpRequestPtr &, const std::function<void(const HttpResponsePtr &)> &)> HttpAsyncCallback;
    typedef std::function<void(const HttpRequestPtr &,
                               const std::function<void(const HttpResponsePtr &)> &,
                               const WebSocketConnectionPtr &)>
        WebSocketNewAsyncCallback;
    typedef std::function<void(const WebSocketConnectionPtr &)>
        WebSocketDisconnetCallback;
    typedef std::function<void(const WebSocketConnectionPtr &, cnet::MsgBuffer *)>
        WebSocketMessageCallback;

    HttpServer(EventLoop *loop,
               const InetAddress &listenAddr,
               const std::string &name);

    ~HttpServer();

    EventLoop *getLoop() const { return server_.getLoop(); }

    void setHttpAsyncCallback(const HttpAsyncCallback &cb)
    {
        httpAsyncCallback_ = cb;
    }
    void setNewWebsocketCallback(const WebSocketNewAsyncCallback &cb)
    {
        newWebsocketCallback_ = cb;
    }
    void setDisconnectWebsocketCallback(const WebSocketDisconnetCallback &cb)
    {
        disconnectWebsocketCallback_ = cb;
    }
    void setWebsocketMessageCallback(const WebSocketMessageCallback &cb)
    {
        webSocketMessageCallback_ = cb;
    }
    void setConnectionCallback(const ConnectionCallback &cb)
    {
        _connectionCallback = cb;
    }
    void setIoLoopNum(int numThreads)
    {
        server_.setIoLoopNum(numThreads);
    }
    void kickoffIdleConnections(size_t timeout)
    {
        server_.kickoffIdleConnections(timeout);
    }
    void start();

#ifdef USE_OPENSSL
    void enableSSL(const std::string &certPath, const std::string &keyPath)
    {
        server_.enableSSL(certPath, keyPath);
    }
#endif

  private:
    void onConnection(const TcpConnectionPtr &conn);
    void onMessage(const TcpConnectionPtr &,
                   MsgBuffer *);
    void onRequest(const TcpConnectionPtr &, const HttpRequestPtr &);
    bool isWebSocket(const TcpConnectionPtr &conn, const HttpRequestPtr &req);
    void sendResponse(const TcpConnectionPtr &, const HttpResponsePtr &);
    cnet::TcpServer server_;
    HttpAsyncCallback httpAsyncCallback_;
    WebSocketNewAsyncCallback newWebsocketCallback_;
    WebSocketDisconnetCallback disconnectWebsocketCallback_;
    WebSocketMessageCallback webSocketMessageCallback_;
    cnet::ConnectionCallback _connectionCallback;
};

} // namespace cnet
