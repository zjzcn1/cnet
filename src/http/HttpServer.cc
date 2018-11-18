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

#include "HttpServer.h"

#include <cnet/utils/Logger.h>
#include "HttpServerContext.h"
#include "HttpResponseImpl.h"
#include <cnet/http/HttpRequest.h>
#include <cnet/http/HttpResponse.h>
#include "HttpUtils.h"
#include <functional>

using namespace std::placeholders;
using namespace cnet;
using namespace cnet;

static void
defaultHttpAsyncCallback(const HttpRequestPtr &, const std::function<void(const HttpResponsePtr &resp)> &callback) {
    auto resp = HttpResponse::newNotFoundResponse();
    resp->setCloseConnection(true);
    callback(resp);
}

static void defaultWebSockAsyncCallback(const HttpRequestPtr &,
                                        const std::function<void(const HttpResponsePtr &resp)> &callback,
                                        const WebSocketConnectionPtr &wsConnPtr) {
    auto resp = HttpResponse::newNotFoundResponse();
    resp->setCloseConnection(true);
    callback(resp);
}

static void defaultConnectionCallback(const cnet::TcpConnectionPtr &conn) {
    return;
}

HttpServer::HttpServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       const std::string &name)
        : server_(loop, listenAddr, name.c_str()),
          httpAsyncCallback_(defaultHttpAsyncCallback),
          newWebsocketCallback_(defaultWebSockAsyncCallback),
          _connectionCallback(defaultConnectionCallback) {
    server_.setConnectionCallback(
            std::bind(&HttpServer::onConnection, this, _1));
    server_.setRecvMessageCallback(
            std::bind(&HttpServer::onMessage, this, _1, _2));
}

HttpServer::~HttpServer() {
}

void HttpServer::start() {
    LOG_INFO << "HttpServer[" << server_.name()
             << "] starts listening on " << server_.ipPort();
    server_.start();
}

void HttpServer::onConnection(const TcpConnectionPtr &conn) {
    if (conn->connected()) {
        LOG_DEBUG << "conn connected!";
        conn->setContext(HttpServerContext(conn));
    } else if (conn->disconnected()) {
        LOG_DEBUG << "conn disconnected!";
        HttpServerContext *context = any_cast<HttpServerContext>(conn->getMutableContext());

        // LOG_INFO << "###:" << string(buf->peek(), buf->readableBytes());
        if (context->webSocketConn()) {
            disconnectWebsocketCallback_(context->webSocketConn());
        }
        conn->setContext(std::string("None"));
    }
    _connectionCallback(conn);
}

void HttpServer::onMessage(const TcpConnectionPtr &conn,
                           MsgBuffer *buf) {
    HttpServerContext *context = any_cast<HttpServerContext>(conn->getMutableContext());

    LOG_DEBUG << "###:" << string(buf->peek(), buf->readableBytes());
    if (context->webSocketConn()) {
        //websocket payload,we shouldn't parse it
        webSocketMessageCallback_(context->webSocketConn(), buf);
        return;
    }
    if (!context->parseRequest(buf)) {
        conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
        //conn->shutdown();
    }

    if (context->gotAll()) {
        context->requestImpl()->parsePremeter();
        context->requestImpl()->setPeerAddr(conn->peerAddr());
        context->requestImpl()->setLocalAddr(conn->localAddr());
        context->requestImpl()->setReceiveDate(cnet::Date::date());
        if (context->firstReq() && isWebSocket(conn, context->request())) {
            auto wsConn = std::make_shared<WebSocketConnectionImpl>(conn);
            newWebsocketCallback_(context->request(), [=](const HttpResponsePtr &resp) mutable {
                                      if (resp->statusCode() == HttpResponse::k101SwitchingProtocols) {
                                          context->setWebsockConnection(wsConn);
                                      }
                                      MsgBuffer buffer;
                                      std::dynamic_pointer_cast<HttpResponseImpl>(resp)->appendToBuffer(&buffer);
                                      conn->send(std::move(buffer));
                                  },
                                  wsConn);
        } else
            onRequest(conn, context->request());
        context->reset();
    }
}

bool HttpServer::isWebSocket(const TcpConnectionPtr &conn, const HttpRequestPtr &req) {
    if (req->getHeader("Connection") == "Upgrade" &&
        req->getHeader("Upgrade") == "websocket") {
        LOG_TRACE << "new websocket request";

        return true;
    }
    return false;
}

void HttpServer::onRequest(const TcpConnectionPtr &conn, const HttpRequestPtr &req) {
    const std::string &connection = req->getHeader("Connection");
    bool _close = connection == "close" ||
                  (req->getVersion() == HttpRequestImpl::kHttp10 && connection != "Keep-Alive");

    bool _isHeadMethod = (req->method() == Head);
    if (_isHeadMethod) {
        req->setMethod(Get);
    }
    HttpServerContext *context = any_cast<HttpServerContext>(conn->getMutableContext());
    //request will be received in same thread,so we don't need mutex;
    context->pushRquestToPipeLine(req);
    httpAsyncCallback_(req, [=](const HttpResponsePtr &response) {
        if (!response)
            return;
        response->setCloseConnection(_close);
        //if the request method is HEAD,remove the body of response(rfc2616-9.4)
        auto newResp = response;
        if (_isHeadMethod) {
            if (newResp->expiredTime() >= 0) {
                //Cached response,make a copy
                newResp = std::make_shared<HttpResponseImpl>(*std::dynamic_pointer_cast<HttpResponseImpl>(response));
            }
            newResp->setBody(std::string());
        }

        {
            /*
             * A client that supports persistent connections MAY “pipeline”
             * its requests (i.e., send multiple requests without waiting
             * for each response). A server MUST send its responses to those
             * requests in the same order that the requests were received.
             *                                             rfc2616-8.1.1.2
             */
            std::lock_guard<std::mutex> guard(context->getPipeLineMutex());
            if (context->getFirstRequest() == req) {
                context->popFirstRequest();
                sendResponse(conn, newResp);
                while (1) {
                    auto resp = context->getFirstResponse();
                    if (resp) {
                        context->popFirstRequest();
                        sendResponse(conn, resp);
                    } else
                        return;
                }
            } else {
                //some earlier requests are waiting for responses;
                context->pushResponseToPipeLine(req, newResp);
            }
        }
    });
}

void HttpServer::sendResponse(const TcpConnectionPtr &conn,
                              const HttpResponsePtr &response) {
    MsgBuffer buf;
    std::dynamic_pointer_cast<HttpResponseImpl>(response)->appendToBuffer(&buf);
    conn->send(std::move(buf));
    auto &sendfileName = std::dynamic_pointer_cast<HttpResponseImpl>(response)->sendfileName();
    if (!sendfileName.empty()) {
        conn->sendFile(sendfileName.c_str());
    }
    if (response->closeConnection()) {
        conn->shutdown();
    }
}
