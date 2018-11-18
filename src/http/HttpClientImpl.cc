#include "HttpClientImpl.h"
#include "HttpRequestImpl.h"
#include "HttpClientContext.h"
#include "HttpAppImpl.h"
#include <stdlib.h>
#include <algorithm>
using namespace cnet;
using namespace std::placeholders;
HttpClientImpl::HttpClientImpl(cnet::EventLoop *loop,
                               const cnet::InetAddress &addr,
                               bool useSSL)
    : _loop(loop),
      _server(addr),
      _useSSL(useSSL)
{
}
HttpClientImpl::HttpClientImpl(cnet::EventLoop *loop,
                               const std::string &hostString)
    : _loop(loop)
{
    auto lowerHost = hostString;
    std::transform(lowerHost.begin(), lowerHost.end(), lowerHost.begin(), tolower);
    if (lowerHost.find("https://") != std::string::npos)
    {
        _useSSL = true;
        lowerHost = lowerHost.substr(8);
    }
    else if (lowerHost.find("http://") != std::string::npos)
    {
        _useSSL = false;
        lowerHost = lowerHost.substr(7);
    }
    else
    {
        return;
    }
    auto pos = lowerHost.find(":");
    if (pos != std::string::npos)
    {
        _domain = lowerHost.substr(0, pos);
        auto portStr = lowerHost.substr(pos + 1);
        pos = portStr.find("/");
        if (pos != std::string::npos)
        {
            portStr = portStr.substr(0, pos);
        }
        auto port = atoi(portStr.c_str());
        if (port > 0 && port < 65536)
        {
            _server = InetAddress(port);
        }
    }
    else
    {
        _domain = lowerHost;
        pos = _domain.find("/");
        if (pos != std::string::npos)
        {
            _domain = _domain.substr(0, pos);
        }
        if (_useSSL)
        {
            _server = InetAddress(443);
        }
        else
        {
            _server = InetAddress(80);
        }
    }
    LOG_TRACE << "userSSL=" << _useSSL << " domain=" << _domain;
}
HttpClientImpl::~HttpClientImpl()
{
    LOG_TRACE << "Deconstruction HttpClient";
}
void HttpClientImpl::sendRequest(const cnet::HttpRequestPtr &req, const cnet::HttpReqCallback &callback)
{
    _loop->runInLoop([=]() {
        sendRequestInLoop(req, callback);
    });
}
void HttpClientImpl::sendRequestInLoop(const cnet::HttpRequestPtr &req,
                                       const cnet::HttpReqCallback &callback)
{
    _loop->assertInLoopThread();

    if (!_tcpClient)
    {
        if (_server.ipNetEndian() == 0 &&
            !_domain.empty() &&
            _server.portNetEndian() != 0)
        {
            //dns
            //TODO:timeout should be set by user
            if (InetAddress::resolve(_domain, &_server) == false)
            {
                callback(ReqResult::BadServerAddress,
                         HttpResponse::newHttpResponse());
                return;
            }
            LOG_TRACE << "dns:domain=" << _domain << ";ip=" << _server.toIp();
        }
        if (_server.ipNetEndian() != 0 && _server.portNetEndian() != 0)
        {
            LOG_TRACE << "New TcpClient," << _server.toIpPort();
            _tcpClient = std::make_shared<cnet::TcpClient>(_loop, _server, "httpClient");

#ifdef USE_OPENSSL
            if (_useSSL)
            {
                _tcpClient->enableSSL();
            }
#endif
            auto thisPtr = shared_from_this();
            assert(_reqAndCallbacks.empty());
            _reqAndCallbacks.push(std::make_pair(req, callback));
            _tcpClient->setConnectionCallback([=](const cnet::TcpConnectionPtr &connPtr) {
                if (connPtr->connected())
                {
                    connPtr->setContext(HttpClientContext(connPtr));
                    //send request;
                    LOG_TRACE << "Connection established!";
                    auto req = thisPtr->_reqAndCallbacks.front().first;
                    thisPtr->sendReq(connPtr, req);
                }
                else
                {
                    LOG_TRACE << "connection disconnect";
                    while (!(thisPtr->_reqAndCallbacks.empty()))
                    {
                        auto reqCallback = _reqAndCallbacks.front().second;
                        _reqAndCallbacks.pop();
                        reqCallback(ReqResult::NetworkFailure, HttpResponse::newHttpResponse());
                    }
                    thisPtr->_tcpClient.reset();
                }
            });
            _tcpClient->setConnectionErrorCallback([=]() {
                //can't connect to server
                while (!(thisPtr->_reqAndCallbacks.empty()))
                {
                    auto reqCallback = _reqAndCallbacks.front().second;
                    _reqAndCallbacks.pop();
                    reqCallback(ReqResult::BadServerAddress, HttpResponse::newHttpResponse());
                }
                thisPtr->_tcpClient.reset();
            });
            _tcpClient->setMessageCallback(std::bind(&HttpClientImpl::onRecvMessage, shared_from_this(), _1, _2));
            _tcpClient->connect();
        }
        else
        {
            callback(ReqResult::BadServerAddress,
                     HttpResponse::newHttpResponse());
            return;
        }
    }
    else
    {
        //send request;
        auto connPtr = _tcpClient->connection();
        if (connPtr && connPtr->connected())
        {
            if (_reqAndCallbacks.empty())
            {
                sendReq(connPtr, req);
            }
        }
        _reqAndCallbacks.push(std::make_pair(req, callback));
    }
}

void HttpClientImpl::sendReq(const cnet::TcpConnectionPtr &connPtr, const HttpRequestPtr &req)
{

    cnet::MsgBuffer buffer;
    auto implPtr = std::dynamic_pointer_cast<HttpRequestImpl>(req);
    assert(implPtr);
    implPtr->appendToBuffer(&buffer);
    LOG_TRACE << "Send request:" << std::string(buffer.peek(), buffer.readableBytes());
    connPtr->send(std::move(buffer));
}

void HttpClientImpl::onRecvMessage(const cnet::TcpConnectionPtr &connPtr, cnet::MsgBuffer *msg)
{
    HttpClientContext *context = any_cast<HttpClientContext>(connPtr->getMutableContext());

    //LOG_TRACE << "###:" << msg->readableBytes();
    if (!context->parseResponse(msg))
    {
        assert(!_reqAndCallbacks.empty());
        auto cb = _reqAndCallbacks.front().second;
        cb(ReqResult::BadResponse, HttpResponse::newHttpResponse());
        _reqAndCallbacks.pop();

        _tcpClient.reset();
        return;
    }

    if (context->gotAll())
    {
        auto resp = context->responseImpl();
        context->reset();

        assert(!_reqAndCallbacks.empty());
        
        auto type = resp->getHeader("Content-Type");
        auto &cb = _reqAndCallbacks.front().second;
        cb(ReqResult::Ok, resp);
        _reqAndCallbacks.pop();
        
        LOG_TRACE << "req buffer size=" << _reqAndCallbacks.size();
        if (!_reqAndCallbacks.empty())
        {
            auto req = _reqAndCallbacks.front().first;
            sendReq(connPtr, req);
        }
        else
        {
            if (resp->closeConnection())
            {
                _tcpClient.reset();
            }
        }
    }
}

HttpClientPtr HttpClient::newHttpClient(const std::string &ip, uint16_t port, bool useSSL)
{
    return std::make_shared<HttpClientImpl>(((HttpAppImpl &)(HttpApp::instance())).loop(), cnet::InetAddress(ip, port), useSSL);
}
//HttpClientPtr HttpClient::newHttpClient(const cnet::InetAddress &addr,bool useSSL)
//{
//    return std::make_shared<HttpClientImpl>(((HttpAppImpl &)(HttpApp::instance())).loop(),addr,useSSL);
//}
HttpClientPtr HttpClient::newHttpClient(const std::string &hostString)
{
    return std::make_shared<HttpClientImpl>(((HttpAppImpl &)(HttpApp::instance())).loop(), hostString);
}
