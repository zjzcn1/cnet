#pragma once

#include <cnet/http/WebSocketConnection.h>
#include <cnet/http/WebSocketController.h>
namespace cnet
{
class WebSocketConnectionImpl : public WebSocketConnection
{
  public:
    explicit WebSocketConnectionImpl(const cnet::TcpConnectionPtr &conn);

    virtual void send(const char *msg, uint64_t len) override;
    virtual void send(const std::string &msg) override;

    virtual const cnet::InetAddress &localAddr() const override;
    virtual const cnet::InetAddress &peerAddr() const override;

    virtual bool connected() const override;
    virtual bool disconnected() const override;

    virtual void shutdown() override;   //close write
    virtual void forceClose() override; //close

    virtual void setContext(const any &context) override;
    virtual const any &getContext() const override;
    virtual any *getMutableContext() override;

    void setController(const WebSocketControllerBasePtr &ctrl)
    {
        _ctrlPtr = ctrl;
    }
    WebSocketControllerBasePtr controller()
    {
        return _ctrlPtr;
    }

  private:
    std::weak_ptr<cnet::TcpConnection> _tcpConn;
    cnet::InetAddress _localAddr;
    cnet::InetAddress _peerAddr;
    WebSocketControllerBasePtr _ctrlPtr;
    any _context;
};
} // namespace cnet
