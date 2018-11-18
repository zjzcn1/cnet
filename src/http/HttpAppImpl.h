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

#include "HttpRequestImpl.h"
#include "HttpResponseImpl.h"
#include "HttpClientImpl.h"
#include "WebSockectConnectionImpl.h"
#include <cnet/http/HttpApp.h>
#include <cnet/http/HttpSimpleController.h>

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <regex>

namespace cnet {
    class HttpAppImpl : public HttpApp {
    public:
        HttpAppImpl() : _connectionNum(0) {
        }

        virtual void addListener(const std::string &ip,
                                 uint16_t port,
                                 bool useSSL = false,
                                 const std::string &certFile = "",
                                 const std::string &keyFile = "") override;

        virtual void setThreadNum(size_t threadNum) override;

        virtual void setSSLFiles(const std::string &certPath,
                                 const std::string &keyPath) override;

        virtual void run() override;

        virtual void registerWebSocketController(const std::string &pathName,
                                                 const std::string &crtlName,
                                                 const std::vector<std::string> &filters =
                                                 std::vector<std::string>()) override;

        virtual void registerHttpSimpleController(const std::string &pathName,
                                                  const std::string &crtlName,
                                                  const std::vector<any> &filtersAndMethods =
                                                  std::vector<any>()) override;

        virtual void enableSession(const size_t timeout = 0) override {
            _useSession = true;
            _sessionTimeout = timeout;
        }

        virtual void disableSession() override { _useSession = false; }

        virtual const std::string &getDocumentRoot() const override { return _rootPath; }

        virtual void setDocumentRoot(const std::string &rootPath) override { _rootPath = rootPath; }

        virtual void setFileTypes(const std::vector<std::string> &types) override;

        virtual void setMaxConnectionNum(size_t maxConnections) override;

        virtual void setMaxConnectionNumPerIP(size_t maxConnectionsPerIP) override;

        virtual void enableRunAsDaemon() override { _runAsDaemon = true; }

        virtual void enableRelaunchOnError() override { _relaunchOnError = true; }

        virtual void setLogPath(const std::string &logPath,
                                const std::string &logfileBaseName = "",
                                size_t logfileSize = 100000000) override;

        virtual void enableSendfile(bool sendFile) override { _useSendfile = sendFile; }

        virtual void setStaticFilesCacheTime(int cacheTime) override { _staticFilesCacheTime = cacheTime; }

        virtual int staticFilesCacheTime() const override { return _staticFilesCacheTime; }

        virtual void setIdleConnectionTimeout(size_t timeout) override { _idleConnectionTimeout = timeout; }

        virtual ~HttpAppImpl() {
            //Destroy the following objects before _loop destruction
            _sessionMapPtr.reset();
        }

        cnet::EventLoop *loop();

        virtual void quit() override {
            assert(_loop.isRunning());
            _loop.quit();
        }

    private:
        virtual void registerHttpController(const std::string &pathPattern,
                                            const HttpBinderBasePtr &binder,
                                            const std::vector<HttpMethod> &validMethods = std::vector<HttpMethod>(),
                                            const std::vector<std::string> &filters = std::vector<std::string>()) override;

        std::vector<std::tuple<std::string, uint16_t, bool, std::string, std::string>> _listeners;

        void onAsyncRequest(const HttpRequestPtr &req, const std::function<void(const HttpResponsePtr &)> &callback);

        void onNewWebsockRequest(const HttpRequestPtr &req,
                                 const std::function<void(const HttpResponsePtr &)> &callback,
                                 const WebSocketConnectionPtr &wsConnPtr);

        void onWebsockMessage(const WebSocketConnectionPtr &wsConnPtr, cnet::MsgBuffer *buffer);

        void onWebsockDisconnect(const WebSocketConnectionPtr &wsConnPtr);

        void onConnection(const TcpConnectionPtr &conn);

        void readSendFile(const std::string &filePath, const HttpRequestPtr &req, const HttpResponsePtr &resp);

        void addHttpPath(const std::string &path,
                         const HttpBinderBasePtr &binder,
                         const std::vector<HttpMethod> &validMethods,
                         const std::vector<std::string> &filters);

        void initRegex();

        //if uuid package found,we can use a uuid string as session id;
        //set _sessionTimeout=0 to make location session valid forever based on cookies;
        size_t _sessionTimeout = 0;
        size_t _idleConnectionTimeout = 60;
        bool _useSession = false;
        typedef std::shared_ptr<Session> SessionPtr;
        std::unique_ptr<CacheMap<std::string, SessionPtr>> _sessionMapPtr;

        std::unique_ptr<CacheMap<std::string, HttpResponsePtr>> _responseCacheMap;

        void doFilters(const std::vector<std::string> &filters,
                       const HttpRequestPtr &req,
                       const std::function<void(const HttpResponsePtr &)> &callback,
                       bool needSetJsessionid,
                       const std::string &session_id,
                       const std::function<void()> &missCallback);

        void doFilterChain(const std::shared_ptr<std::queue<std::shared_ptr<HttpFilterBase>>> &chain,
                           const HttpRequestPtr &req,
                           const std::function<void(const HttpResponsePtr &)> &callback,
                           bool needSetJsessionid,
                           const std::string &session_id,
                           const std::function<void()> &missCallback);

        //
        struct ControllerAndFiltersName {
            std::string controllerName;
            std::vector<std::string> filtersName;
            std::vector<int> _validMethodsFlags;
            std::shared_ptr<HttpSimpleControllerBase> controller;
            std::weak_ptr<HttpResponse> responsePtr;
            std::mutex _mutex;
        };
        std::unordered_map<std::string, ControllerAndFiltersName> _simpCtrlMap;
        std::mutex _simpCtrlMutex;
        struct WSCtrlAndFiltersName {
            WebSocketControllerBasePtr controller;
            std::vector<std::string> filtersName;
        };
        std::unordered_map<std::string, WSCtrlAndFiltersName> _websockCtrlMap;
        std::mutex _websockCtrlMutex;

        struct CtrlBinder {
            std::string pathParameterPattern;
            std::vector<size_t> parameterPlaces;
            std::map<std::string, size_t> queryParametersPlaces;
            HttpBinderBasePtr binderPtr;
            std::vector<std::string> filtersName;
            std::unique_ptr<std::mutex> binderMtx = std::unique_ptr<std::mutex>(new std::mutex);
            std::weak_ptr<HttpResponse> responsePtr;
            std::vector<int> _validMethodsFlags;
            std::regex _regex;
        };
        std::vector<CtrlBinder> _ctrlVector;
        std::mutex _ctrlMutex;

        std::regex _ctrlRegex;
        bool _enableLastModify = true;
        std::set<std::string> _fileTypeSet = {"html", "js", "css", "xml", "xsl", "txt", "svg", "ttf",
                                              "otf", "woff2", "woff", "eot", "png", "jpg", "jpeg",
                                              "gif", "bmp", "ico", "icns"};
        std::string _rootPath = "./";

        std::atomic_bool _running;

        //tool funcs

        size_t _threadNum = 1;

        cnet::EventLoop _loop;

        std::string _sslCertPath;
        std::string _sslKeyPath;

        size_t _maxConnectionNum = 100000;
        size_t _maxConnectionNumPerIP = 0;

        std::atomic<uint64_t> _connectionNum;
        std::unordered_map<std::string, size_t> _connectionsNumMap;

        std::mutex _connectionsNumMapMutex;

        bool _runAsDaemon = false;
        bool _relaunchOnError = false;
        std::string _logPath = "";
        std::string _logfileBaseName = "";
        size_t _logfileSize = 100000000;
        bool _useSendfile = true;
        int _staticFilesCacheTime = 5;
        std::unordered_map<std::string, std::weak_ptr<HttpResponse>> _staticFilesCache;
        std::mutex _staticFilesCacheMutex;
    };

} // namespace cnet
