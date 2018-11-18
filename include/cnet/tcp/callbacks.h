#pragma once

#include <functional>
#include <memory>
namespace cnet
{
enum cnetError
{
    cnetError_None,
    cnetError_UnkownError
};
typedef std::function<void()> TimerCallback;

// the data has been read to (buf, len)
class TcpConnection;
class SSLConnection;
class MsgBuffer;
typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;
typedef std::shared_ptr<SSLConnection> SSLConnectionPtr;
//tcp server and connection callback
typedef std::function<void(const TcpConnectionPtr &,
                           MsgBuffer *)>
    RecvMessageCallback;
typedef std::function<void()> ConnectionErrorCallback;
typedef std::function<void(const TcpConnectionPtr &)> ConnectionCallback;
typedef std::function<void(const TcpConnectionPtr &)> CloseCallback;
typedef std::function<void(const TcpConnectionPtr &)> WriteCompleteCallback;
typedef std::function<void(const TcpConnectionPtr &, const size_t)> HighWaterMarkCallback;
//ssl server and connection callback
typedef std::function<void(const SSLConnectionPtr &,
                           MsgBuffer *)>
    SSLRecvMessageCallback;
typedef std::function<void(const SSLConnectionPtr &)> SSLConnectionCallback;
typedef std::function<void(const SSLConnectionPtr &)> SSLCloseCallback;
typedef std::function<void(const SSLConnectionPtr &)> SSLWriteCompleteCallback;
typedef std::function<void(const SSLConnectionPtr &, const size_t)> SSLHighWaterMarkCallback;

typedef std::function<void(const cnetError)> OperationCompleteCallback;

} // namespace cnet
