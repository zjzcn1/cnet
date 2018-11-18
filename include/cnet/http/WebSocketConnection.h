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

#include <cnet/config.h>
#include <cnet/utils/NonCopyable.h>
#include <cnet/tcp/InetAddress.h>
#include <string>
#include <memory>
namespace cnet
{
class WebSocketConnection
{
  public:
    WebSocketConnection() = default;
    virtual ~WebSocketConnection(){};
    virtual void send(const char *msg, uint64_t len) = 0;
    virtual void send(const std::string &msg) = 0;

    virtual const cnet::InetAddress &localAddr() const = 0;
    virtual const cnet::InetAddress &peerAddr() const = 0;

    virtual bool connected() const = 0;
    virtual bool disconnected() const = 0;

    virtual void shutdown() = 0;   //close write
    virtual void forceClose() = 0; //close

    virtual void setContext(const any &context) = 0;
    virtual const any &getContext() const = 0;
    virtual any *getMutableContext() = 0;
};
typedef std::shared_ptr<WebSocketConnection> WebSocketConnectionPtr;
} // namespace cnet
