#pragma once

#include <cnet/tcp/EventLoop.h>
#include <cnet/utils/NonCopyable.h>
#include "Socket.h"
#include <cnet/tcp/InetAddress.h>
#include "Channel.h"
#include <functional>

namespace cnet
{
typedef std::function<void(int fd, const InetAddress &)> NewConnectionCallback;
class Acceptor : NonCopyable
{
  public:
    Acceptor(EventLoop *loop, const InetAddress &addr);
    ~Acceptor();
    const InetAddress &addr() const { return addr_; }
    void setNewConnectionCallback(const NewConnectionCallback &cb)
    {
        newConnectionCallback_ = cb;
    };
    void listen();

  protected:
    Socket sock_;
    InetAddress addr_;
    EventLoop *loop_;
    NewConnectionCallback newConnectionCallback_;
    Channel acceptChannel_;
    void readCallback();
};
} // namespace cnet
