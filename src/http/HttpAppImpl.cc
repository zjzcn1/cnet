/**
 *
 *  HttpAppFrameworkImpl.cc
 *  An Tao
 *  @section LICENSE
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  cnet
 *
 */
#include "HttpAppImpl.h"
#include "HttpServer.h"
#include "HttpUtils.h"
#include <cnet/http/HttpClassMap.h>
#include <cnet/http/HttpRequest.h>
#include <cnet/http/HttpResponse.h>
#include <cnet/http/CacheMap.h>
#include <cnet/http/Session.h>
#include <cnet/utils/AsyncFileLogger.h>

#ifdef USE_OPENSSL
#include <openssl/sha.h>
#else

#include "cnet/utils/Sha1.h"

#endif

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <algorithm>
#include <memory>
#include <tuple>
#include <utility>
#include <unistd.h>
#include <fcntl.h>

using namespace cnet;
using namespace std::placeholders;
std::map<std::string, std::shared_ptr<cnet::HttpObjectBase>> HttpBinderBase::_objMap;
std::mutex HttpBinderBase::_objMutex;

static void daemon(void) {
    int fs;

    printf("Initializing daemon mode\n");

    if (getppid() != 1) {
        fs = fork();

        if (fs > 0)
            exit(0); // parent

        if (fs < 0) {
            perror("fork");
            exit(1);
        }

        setsid();
    }

    // redirect stdin/stdout/stderr to /dev/null
    close(0);
    close(1);
    close(2);

    open("/dev/null", O_RDWR);
    int ret = dup(0);
    ret = dup(0);
    (void) ret;
    umask(0);

    return;
}

void HttpAppImpl::setFileTypes(const std::vector<std::string> &types) {
    for (auto type : types) {
        _fileTypeSet.insert(type);
    }
}

void HttpAppImpl::initRegex() {
    std::string regString;
    for (auto &binder : _ctrlVector) {
        std::regex reg("\\(\\[\\^/\\]\\*\\)");
        std::string tmp = std::regex_replace(binder.pathParameterPattern, reg, "[^/]*");
        binder._regex = std::regex(binder.pathParameterPattern, std::regex_constants::icase);
        regString.append("(").append(tmp).append(")|");
    }
    if (regString.length() > 0)
        regString.resize(regString.length() - 1); //remove the last '|'
    LOG_DEBUG << "regex string:" << regString;
    _ctrlRegex = std::regex(regString, std::regex_constants::icase);
}

void HttpAppImpl::registerWebSocketController(const std::string &pathName,
                                              const std::string &ctrlName,
                                              const std::vector<std::string> &filters) {
    assert(!pathName.empty());
    assert(!ctrlName.empty());
    std::string path(pathName);
    std::transform(pathName.begin(), pathName.end(), path.begin(), tolower);
    auto objPtr = std::shared_ptr<HttpObjectBase>(HttpClassMap::newObject(ctrlName));
    auto ctrlPtr = std::dynamic_pointer_cast<WebSocketControllerBase>(objPtr);
    assert(ctrlPtr);
    std::lock_guard<std::mutex> guard(_websockCtrlMutex);

    _websockCtrlMap[path].controller = ctrlPtr;
    _websockCtrlMap[path].filtersName = filters;
}

void HttpAppImpl::registerHttpSimpleController(const std::string &pathName,
                                               const std::string &ctrlName,
                                               const std::vector<any> &filtersAndMethods) {
    assert(!pathName.empty());
    assert(!ctrlName.empty());

    std::string path(pathName);
    std::transform(pathName.begin(), pathName.end(), path.begin(), tolower);
    std::lock_guard<std::mutex> guard(_simpCtrlMutex);
    std::vector<HttpMethod> validMethods;
    std::vector<std::string> filters;
    for (auto &filterOrMethod : filtersAndMethods) {
        if (filterOrMethod.type() == typeid(std::string)) {
            filters.push_back(*any_cast<std::string>(&filterOrMethod));
        } else if (filterOrMethod.type() == typeid(const char *)) {
            filters.push_back(*any_cast<const char *>(&filterOrMethod));
        } else if (filterOrMethod.type() == typeid(HttpMethod)) {
            validMethods.push_back(*any_cast<HttpMethod>(&filterOrMethod));
        } else {
            std::cerr << "Invalid controller constraint type:" << filterOrMethod.type().name() << std::endl;
            LOG_ERROR << "Invalid controller constraint type";
            exit(1);
        }
    }
    auto &iterm = _simpCtrlMap[path];
    iterm.controllerName = ctrlName;
    iterm.filtersName = filters;
    iterm._validMethodsFlags.clear(); //There may be old data, first clear
    if (validMethods.size() > 0) {
        iterm._validMethodsFlags.resize(Invalid, 0);
        for (auto method : validMethods) {
            iterm._validMethodsFlags[method] = 1;
        }
    }
}

void HttpAppImpl::addHttpPath(const std::string &path,
                              const HttpBinderBasePtr &binder,
                              const std::vector<HttpMethod> &validMethods,
                              const std::vector<std::string> &filters) {
    //path will be like /api/v1/service/method/{1}/{2}/xxx...
    std::vector<size_t> places;
    std::string tmpPath = path;
    std::string paras = "";
    std::regex regex = std::regex("\\{([0-9]+)\\}");
    std::smatch results;
    auto pos = tmpPath.find("?");
    if (pos != std::string::npos) {
        paras = tmpPath.substr(pos + 1);
        tmpPath = tmpPath.substr(0, pos);
    }
    std::string originPath = tmpPath;
    while (std::regex_search(tmpPath, results, regex)) {
        if (results.size() > 1) {
            size_t place = (size_t) std::stoi(results[1].str());
            if (place > binder->paramCount() || place == 0) {
                LOG_ERROR << "parameter placeholder(value=" << place << ") out of range (1 to "
                          << binder->paramCount() << ")";
                exit(0);
            }
            places.push_back(place);
        }
        tmpPath = results.suffix();
    }
    std::map<std::string, size_t> parametersPlaces;
    if (!paras.empty()) {
        std::regex pregex("([^&]*)=\\{([0-9]+)\\}&*");
        while (std::regex_search(paras, results, pregex)) {
            if (results.size() > 2) {
                size_t place = (size_t) std::stoi(results[2].str());
                if (place > binder->paramCount() || place == 0) {
                    LOG_ERROR << "parameter placeholder(value=" << place << ") out of range (1 to "
                              << binder->paramCount() << ")";
                    exit(0);
                }
                parametersPlaces[results[1].str()] = place;
            }
            paras = results.suffix();
        }
    }
    struct CtrlBinder _binder;
    _binder.parameterPlaces = std::move(places);
    _binder.queryParametersPlaces = std::move(parametersPlaces);
    _binder.binderPtr = binder;
    _binder.filtersName = filters;
    _binder.pathParameterPattern = std::regex_replace(originPath, regex, "([^/]*)");
    if (validMethods.size() > 0) {
        _binder._validMethodsFlags.resize(Invalid, 0);
        for (auto method : validMethods) {
            _binder._validMethodsFlags[method] = 1;
        }
    }
    {
        std::lock_guard<std::mutex> guard(_ctrlMutex);
        _ctrlVector.push_back(std::move(_binder));
    }
}

void HttpAppImpl::registerHttpController(const std::string &pathPattern,
                                         const HttpBinderBasePtr &binder,
                                         const std::vector<HttpMethod> &validMethods,
                                         const std::vector<std::string> &filters) {
    assert(!pathPattern.empty());
    assert(binder);
    std::string path(pathPattern);

    //std::transform(path.begin(), path.end(), path.begin(), tolower);
    addHttpPath(path, binder, validMethods, filters);
}

void HttpAppImpl::setThreadNum(size_t threadNum) {
    assert(threadNum >= 1);
    _threadNum = threadNum;
}

void HttpAppImpl::setMaxConnectionNum(size_t maxConnections) {
    _maxConnectionNum = maxConnections;
}

void HttpAppImpl::setMaxConnectionNumPerIP(size_t maxConnectionsPerIP) {
    _maxConnectionNumPerIP = maxConnectionsPerIP;
}

void HttpAppImpl::setLogPath(const std::string &logPath,
                             const std::string &logfileBaseName,
                             size_t logfileSize) {
    if (logPath == "")
        return;
    if (access(logPath.c_str(), 0) != 0) {
        std::cerr << "Log path dose not exist!\n";
        exit(1);
    }
    if (access(logPath.c_str(), R_OK | W_OK) != 0) {
        std::cerr << "Unable to access log path!\n";
        exit(1);
    }
    _logPath = logPath;
    _logfileBaseName = logfileBaseName;
    _logfileSize = logfileSize;
}

void HttpAppImpl::setSSLFiles(const std::string &certPath,
                              const std::string &keyPath) {
    _sslCertPath = certPath;
    _sslKeyPath = keyPath;
}

void HttpAppImpl::addListener(const std::string &ip,
                              uint16_t port,
                              bool useSSL,
                              const std::string &certFile,
                              const std::string &keyFile) {
    assert(!_running);

#ifndef USE_OPENSSL
    if (useSSL) {
        LOG_ERROR << "Can't use SSL without OpenSSL found in your system";
    }
#endif

    _listeners.push_back(std::make_tuple(ip, port, useSSL, certFile, keyFile));
}

void HttpAppImpl::run() {
    //
    LOG_INFO << "Start to run...";
    cnet::AsyncFileLogger asyncFileLogger;

    if (_runAsDaemon) {
        //go daemon!
        daemon();
#ifdef __linux__
        _loop.resetTimerQueue();
#endif
    }
    //set relaunching
    if (_relaunchOnError) {
        while (true) {
            int child_status = 0;
            auto child_pid = fork();
            if (child_pid < 0) {
                LOG_ERROR << "fork error";
                abort();
            } else if (child_pid == 0) {
                //child
                break;
            }
            waitpid(child_pid, &child_status, 0);
            sleep(1);
            LOG_INFO << "start new process";
        }
    }

    //set logger
    if (!_logPath.empty()) {
        if (access(_logPath.c_str(), R_OK | W_OK) >= 0) {
            std::string baseName = _logfileBaseName;
            if (baseName == "") {
                baseName = "cnet";
            }
            asyncFileLogger.setFileName(baseName, ".log", _logPath);
            asyncFileLogger.startLogging();
            cnet::Logger::setOutputFunction(
                    [&](const char *msg, const uint64_t len) { asyncFileLogger.output(msg, len); },
                    [&]() { asyncFileLogger.flush(); });
            asyncFileLogger.setFileSizeLimit(_logfileSize);
        } else {
            LOG_ERROR << "log file path not exist";
            abort();
        }
    }
    if (_relaunchOnError) {
        LOG_INFO << "Start child process";
    }
    //now start runing!!

    _running = true;

    std::vector<std::shared_ptr<HttpServer>> servers;
    std::vector<std::shared_ptr<EventLoopThread>> loopThreads;
    initRegex();
    for (auto listener : _listeners) {
        LOG_DEBUG << "thread num=" << _threadNum;
#ifdef __linux__
        for (size_t i = 0; i < _threadNum; i++)
        {
            auto loopThreadPtr = std::make_shared<EventLoopThread>();
            loopThreadPtr->run();
            loopThreads.push_back(loopThreadPtr);
            auto serverPtr = std::make_shared<HttpServer>(loopThreadPtr->getLoop(),
                                                          InetAddress(std::get<0>(listener), std::get<1>(listener)), "cnet");
            if (std::get<2>(listener))
            {
                //enable ssl;
#ifdef USE_OPENSSL
                auto cert = std::get<3>(listener);
                auto key = std::get<4>(listener);
                if (cert == "")
                    cert = _sslCertPath;
                if (key == "")
                    key = _sslKeyPath;
                if (cert == "" || key == "")
                {
                    std::cerr << "You can't use https without cert file or key file" << std::endl;
                    exit(1);
                }
                serverPtr->enableSSL(cert, key);
#endif
            }
            serverPtr->setHttpAsyncCallback(std::bind(&HttpAppImpl::onAsyncRequest, this, _1, _2));
            serverPtr->setConnectionCallback(std::bind(&HttpAppImpl::onConnection, this, _1));
            serverPtr->kickoffIdleConnections(_idleConnectionTimeout);
            serverPtr->start();
            servers.push_back(serverPtr);
        }
#else
        auto loopThreadPtr = std::make_shared<EventLoopThread>();
        loopThreadPtr->run();
        loopThreads.push_back(loopThreadPtr);
        auto serverPtr = std::make_shared<HttpServer>(loopThreadPtr->getLoop(),
                                                      InetAddress(std::get<0>(listener), std::get<1>(listener)),
                                                      "cnet");
        if (std::get<2>(listener)) {
            //enable ssl;
#ifdef USE_OPENSSL
            auto cert = std::get<3>(listener);
            auto key = std::get<4>(listener);
            if (cert == "")
                cert = _sslCertPath;
            if (key == "")
                key = _sslKeyPath;
            if (cert == "" || key == "")
            {
                std::cerr << "You can't use https without cert file or key file" << std::endl;
                exit(1);
            }
            serverPtr->enableSSL(cert, key);
#endif
        }
        serverPtr->setIoLoopNum(_threadNum);
        serverPtr->setHttpAsyncCallback(std::bind(&HttpAppImpl::onAsyncRequest, this, _1, _2));
        serverPtr->setNewWebsocketCallback(std::bind(&HttpAppImpl::onNewWebsockRequest, this, _1, _2, _3));
        serverPtr->setWebsocketMessageCallback(std::bind(&HttpAppImpl::onWebsockMessage, this, _1, _2));
        serverPtr->setDisconnectWebsocketCallback(std::bind(&HttpAppImpl::onWebsockDisconnect, this, _1));
        serverPtr->setConnectionCallback(std::bind(&HttpAppImpl::onConnection, this, _1));
        serverPtr->kickoffIdleConnections(_idleConnectionTimeout);
        serverPtr->start();
        servers.push_back(serverPtr);
#endif
    }
    if (_useSession) {
        if (_sessionTimeout > 0) {
            size_t wheelNum = 1;
            size_t bucketNum = 0;
            if (_sessionTimeout < 500) {
                bucketNum = _sessionTimeout + 1;
            } else {
                auto tmpTimeout = _sessionTimeout;
                bucketNum = 100;
                while (tmpTimeout > 100) {
                    wheelNum++;
                    tmpTimeout = tmpTimeout / 100;
                }
            }
            _sessionMapPtr = std::unique_ptr<CacheMap<std::string, SessionPtr>>(
                    new CacheMap<std::string, SessionPtr>(&_loop, 1.0, wheelNum, bucketNum));
        } else if (_sessionTimeout == 0) {
            _sessionMapPtr = std::unique_ptr<CacheMap<std::string, SessionPtr>>(
                    new CacheMap<std::string, SessionPtr>(&_loop, 0, 0, 0));
        }
    }
    _responseCacheMap = std::unique_ptr<CacheMap<std::string, HttpResponsePtr>>(
            new CacheMap<std::string, HttpResponsePtr>(&_loop, 1.0, 4, 50)); //Max timeout up to about 70 days;
    _loop.loop();
}

void HttpAppImpl::doFilterChain(const std::shared_ptr<std::queue<std::shared_ptr<HttpFilterBase>>> &chain,
                                const HttpRequestPtr &req,
                                const std::function<void(const HttpResponsePtr &)> &callback,
                                bool needSetJsessionid,
                                const std::string &session_id,
                                const std::function<void()> &missCallback) {
    if (chain->size() > 0) {
        auto filter = chain->front();
        chain->pop();
        filter->doFilter(req, [=](HttpResponsePtr res) {
            if (needSetJsessionid)
                res->addCookie("JSESSIONID", session_id);
            callback(res);
        }, [=]() { doFilterChain(chain, req, callback, needSetJsessionid, session_id, missCallback); });
    } else {
        missCallback();
    }
}

void HttpAppImpl::doFilters(const std::vector<std::string> &filters,
                            const HttpRequestPtr &req,
                            const std::function<void(const HttpResponsePtr &)> &callback,
                            bool needSetJsessionid,
                            const std::string &session_id,
                            const std::function<void()> &missCallback) {
    LOG_DEBUG << "filters count:" << filters.size();
    std::shared_ptr<std::queue<std::shared_ptr<HttpFilterBase>>> filterPtrs =
            std::make_shared<std::queue<std::shared_ptr<HttpFilterBase>>>();
    for (auto filter : filters) {
        auto _object = std::shared_ptr<HttpObjectBase>(HttpClassMap::newObject(filter));
        auto _filter = std::dynamic_pointer_cast<HttpFilterBase>(_object);
        if (_filter)
            filterPtrs->push(_filter);
        else {
            LOG_ERROR << "filter " << filter << " not found";
        }
    }
    doFilterChain(filterPtrs, req, callback, needSetJsessionid, session_id, missCallback);
}

void HttpAppImpl::onWebsockDisconnect(const WebSocketConnectionPtr &wsConnPtr) {
    auto wsConnImplPtr = std::dynamic_pointer_cast<WebSocketConnectionImpl>(wsConnPtr);
    assert(wsConnImplPtr);
    auto ctrl = wsConnImplPtr->controller();
    if (ctrl) {
        ctrl->handleConnectionClosed(wsConnPtr);
        wsConnImplPtr->setController(WebSocketControllerBasePtr());
    }
}

void HttpAppImpl::onConnection(const TcpConnectionPtr &conn) {
    if (conn->connected()) {
        if (_connectionNum.fetch_add(1) >= _maxConnectionNum) {
            LOG_ERROR << "too much connections!force close!";
            conn->forceClose();
        } else if (_maxConnectionNumPerIP > 0) {
            {
                auto iter = _connectionsNumMap.find(conn->peerAddr().toIp());
                if (iter == _connectionsNumMap.end()) {
                    _connectionsNumMap[conn->peerAddr().toIp()] = 0;
                }
                if (_connectionsNumMap[conn->peerAddr().toIp()]++ >= _maxConnectionNumPerIP) {
                    conn->forceClose();
                }
            }
        }
    } else {
        _connectionNum--;

        if (_maxConnectionNumPerIP > 0 &&
            _connectionsNumMap.find(conn->peerAddr().toIp()) != _connectionsNumMap.end()) {
            std::lock_guard<std::mutex> guard(_connectionsNumMapMutex);
            _connectionsNumMap[conn->peerAddr().toIp()]--;
        }
    }
}

std::string parseWebsockFrame(cnet::MsgBuffer *buffer) {
    if (buffer->readableBytes() >= 2) {
        auto secondByte = (*buffer)[1];
        size_t length = secondByte & 127;
        int isMasked = (secondByte & 0x80);
        if (isMasked != 0) {
            LOG_DEBUG << "data encoded!";
        } else LOG_DEBUG << "plain data";
        size_t indexFirstMask = 2;

        if (length == 126) {
            indexFirstMask = 4;
        } else if (length == 127) {
            indexFirstMask = 10;
        }
        if (indexFirstMask > 2 && buffer->readableBytes() >= indexFirstMask) {
            if (indexFirstMask == 4) {
                length = (unsigned char) (*buffer)[2];
                length = (length << 8) + (unsigned char) (*buffer)[3];
                LOG_DEBUG << "bytes[2]=" << (unsigned char) (*buffer)[2];
                LOG_DEBUG << "bytes[3]=" << (unsigned char) (*buffer)[3];
            } else if (indexFirstMask == 10) {
                length = (unsigned char) (*buffer)[2];
                length = (length << 8) + (unsigned char) (*buffer)[3];
                length = (length << 8) + (unsigned char) (*buffer)[4];
                length = (length << 8) + (unsigned char) (*buffer)[5];
                length = (length << 8) + (unsigned char) (*buffer)[6];
                length = (length << 8) + (unsigned char) (*buffer)[7];
                length = (length << 8) + (unsigned char) (*buffer)[8];
                length = (length << 8) + (unsigned char) (*buffer)[9];
                //                length=*((uint64_t *)(buffer->peek()+2));
                //                length=ntohll(length);
            } else {
                assert(0);
            }
        }
        LOG_DEBUG << "websocket message len=" << length;
        if (buffer->readableBytes() >= (indexFirstMask + 4 + length)) {
            auto masks = buffer->peek() + indexFirstMask;
            int indexFirstDataByte = indexFirstMask + 4;
            auto rawData = buffer->peek() + indexFirstDataByte;
            std::string message;
            message.resize(length);
            LOG_DEBUG << "rawData[0]=" << (unsigned char) rawData[0];
            LOG_DEBUG << "masks[0]=" << (unsigned char) masks[0];
            for (size_t i = 0; i < length; i++) {
                message[i] = (rawData[i] ^ masks[i % 4]);
            }
            buffer->retrieve(indexFirstMask + 4 + length);
            LOG_DEBUG << "got message len=" << message.length();
            return message;
        }
    }
    return std::string();
}

void HttpAppImpl::onWebsockMessage(const WebSocketConnectionPtr &wsConnPtr,
                                   cnet::MsgBuffer *buffer) {
    auto wsConnImplPtr = std::dynamic_pointer_cast<WebSocketConnectionImpl>(wsConnPtr);
    assert(wsConnImplPtr);
    auto ctrl = wsConnImplPtr->controller();
    if (ctrl) {
        std::string message;
        while (!(message = parseWebsockFrame(buffer)).empty()) {
            LOG_DEBUG << "Got websock message:" << message;
            ctrl->handleNewMessage(wsConnPtr, std::move(message));
        }
    }
}

void HttpAppImpl::onNewWebsockRequest(const HttpRequestPtr &req,
                                      const std::function<void(const HttpResponsePtr &)> &callback,
                                      const WebSocketConnectionPtr &wsConnPtr) {
    std::string wsKey = req->getHeader("Sec-WebSocket-Key");
    if (!wsKey.empty()) {
        // magic="258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        WebSocketControllerBasePtr ctrlPtr;
        std::vector<std::string> filtersName;
        {
            std::string pathLower(req->path());
            std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), tolower);
            std::lock_guard<std::mutex> guard(_websockCtrlMutex);
            if (_websockCtrlMap.find(pathLower) != _websockCtrlMap.end()) {
                ctrlPtr = _websockCtrlMap[pathLower].controller;
                filtersName = _websockCtrlMap[pathLower].filtersName;
            }
        }
        if (ctrlPtr) {
            doFilters(filtersName, req, callback, false, "", [=]() mutable {
                wsKey.append("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
                unsigned char accKey[SHA_DIGEST_LENGTH];
                SHA1(reinterpret_cast<const unsigned char *>(wsKey.c_str()), wsKey.length(), accKey);
                auto base64Key = HttpUtils::base64Encode(accKey, SHA_DIGEST_LENGTH);
                auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(HttpResponse::k101SwitchingProtocols);
                resp->addHeader("Upgrade", "websocket");
                resp->addHeader("Connection", "Upgrade");
                resp->addHeader("Sec-WebSocket-Accept", base64Key);
                callback(resp);
                auto wsConnImplPtr = std::dynamic_pointer_cast<WebSocketConnectionImpl>(wsConnPtr);
                assert(wsConnImplPtr);
                wsConnImplPtr->setController(ctrlPtr);
                ctrlPtr->handleNewConnection(req, wsConnPtr);
                return;
            });
            return;
        }
    }
    auto resp = cnet::HttpResponse::newNotFoundResponse();
    resp->setCloseConnection(true);
    callback(resp);
}

void HttpAppImpl::onAsyncRequest(const HttpRequestPtr &req, const std::function<void(const HttpResponsePtr &)> &callback) {
    LOG_DEBUG << "new request:" << req->peerAddr().toIpPort() << "->" << req->localAddr().toIpPort();
    LOG_DEBUG << "Headers " << req->methodString() << " " << req->path();

    LOG_DEBUG << "http path=" << req->path();
    // LOG_DEBUG << "query: " << req->query() ;

    std::string session_id = req->getCookie("JSESSIONID");
    bool needSetJsessionid = false;
    if (_useSession) {
        if (session_id == "") {
            session_id = HttpUtils::getUuid().c_str();
            needSetJsessionid = true;
            _sessionMapPtr->insert(session_id, std::make_shared<Session>(), _sessionTimeout);
        } else {
            if (!_sessionMapPtr->find(session_id)) {
                _sessionMapPtr->insert(session_id, std::make_shared<Session>(), _sessionTimeout);
            }
        }
        (std::dynamic_pointer_cast<HttpRequestImpl>(req))->setSession((*_sessionMapPtr)[session_id]);
    }

    std::string path = req->path();
    auto pos = path.rfind(".");
    if (pos != std::string::npos) {
        std::string filetype = path.substr(pos + 1);
        transform(filetype.begin(), filetype.end(), filetype.begin(), tolower);
        if (_fileTypeSet.find(filetype) != _fileTypeSet.end()) {
            //LOG_INFO << "file query!";
            std::string filePath = _rootPath + path;
            std::shared_ptr<HttpResponseImpl> resp = std::make_shared<HttpResponseImpl>();
            //find cached response
            HttpResponsePtr cachedResp;
            {
                std::lock_guard<std::mutex> guard(_staticFilesCacheMutex);
                if (_staticFilesCache.find(filePath) != _staticFilesCache.end()) {
                    cachedResp = _staticFilesCache[filePath].lock();
                    if (!cachedResp) {
                        _staticFilesCache.erase(filePath);
                    }
                }
            }

            //check last modified time,rfc2616-14.25
            //If-Modified-Since: Mon, 15 Oct 2018 06:26:33 GMT

            if (_enableLastModify) {
                if (cachedResp) {
                    if (cachedResp->getHeader("Last-Modified") == req->getHeader("If-Modified-Since")) {
                        resp->setStatusCode(HttpResponse::k304NotModified);
                        if (needSetJsessionid) {
                            resp->addCookie("JSESSIONID", session_id);
                        }
                        callback(resp);
                        return;
                    }
                } else {
                    struct stat fileStat;
                    LOG_DEBUG << "enabled LastModify";
                    if (stat(filePath.c_str(), &fileStat) >= 0) {
                        LOG_DEBUG << "last modify time:" << fileStat.st_mtime;
                        struct tm tm1;
                        gmtime_r(&fileStat.st_mtime, &tm1);
                        std::string timeStr;
                        timeStr.resize(64);
                        auto len = strftime((char *) timeStr.data(), timeStr.size(), "%a, %d %b %Y %T GMT", &tm1);
                        timeStr.resize(len);
                        std::string modiStr = req->getHeader("If-Modified-Since");
                        if (modiStr == timeStr && !modiStr.empty()) {
                            LOG_DEBUG << "not Modified!";
                            resp->setStatusCode(HttpResponse::k304NotModified);
                            if (needSetJsessionid) {
                                resp->addCookie("JSESSIONID", session_id);
                            }
                            callback(resp);
                            return;
                        }
                        resp->addHeader("Last-Modified", timeStr);
                        resp->addHeader("Expires", "Thu, 01 Jan 1970 00:00:00 GMT");
                    }
                }
            }

            if (cachedResp) {
                if (needSetJsessionid) {
                    //make a copy
                    auto newCachedResp = std::make_shared<HttpResponseImpl>(
                            *std::dynamic_pointer_cast<HttpResponseImpl>(cachedResp));
                    newCachedResp->addCookie("JSESSIONID", session_id);
                    newCachedResp->setExpiredTime(-1);
                    callback(newCachedResp);
                    return;
                }
                callback(cachedResp);
                return;
            }

            // pick a Content-Type for the file
            if (filetype == "html")
                resp->setContentTypeCode(CT_TEXT_HTML);
            else if (filetype == "js")
                resp->setContentTypeCode(CT_APPLICATION_X_JAVASCRIPT);
            else if (filetype == "css")
                resp->setContentTypeCode(CT_TEXT_CSS);
            else if (filetype == "xml")
                resp->setContentTypeCode(CT_TEXT_XML);
            else if (filetype == "xsl")
                resp->setContentTypeCode(CT_TEXT_XSL);
            else if (filetype == "txt")
                resp->setContentTypeCode(CT_TEXT_PLAIN);
            else if (filetype == "svg")
                resp->setContentTypeCode(CT_IMAGE_SVG_XML);
            else if (filetype == "ttf")
                resp->setContentTypeCode(CT_APPLICATION_X_FONT_TRUETYPE);
            else if (filetype == "otf")
                resp->setContentTypeCode(CT_APPLICATION_X_FONT_OPENTYPE);
            else if (filetype == "woff2")
                resp->setContentTypeCode(CT_APPLICATION_FONT_WOFF2);
            else if (filetype == "woff")
                resp->setContentTypeCode(CT_APPLICATION_FONT_WOFF);
            else if (filetype == "eot")
                resp->setContentTypeCode(CT_APPLICATION_VND_MS_FONTOBJ);
            else if (filetype == "png")
                resp->setContentTypeCode(CT_IMAGE_PNG);
            else if (filetype == "jpg")
                resp->setContentTypeCode(CT_IMAGE_JPG);
            else if (filetype == "jpeg")
                resp->setContentTypeCode(CT_IMAGE_JPG);
            else if (filetype == "gif")
                resp->setContentTypeCode(CT_IMAGE_GIF);
            else if (filetype == "bmp")
                resp->setContentTypeCode(CT_IMAGE_BMP);
            else if (filetype == "ico")
                resp->setContentTypeCode(CT_IMAGE_XICON);
            else if (filetype == "icns")
                resp->setContentTypeCode(CT_IMAGE_ICNS);
            else
                resp->setContentTypeCode(CT_APPLICATION_OCTET_STREAM);

            readSendFile(filePath, req, resp);

            if (needSetJsessionid) {
                auto newCachedResp = resp;
                if (resp->expiredTime() >= 0) {
                    //make a copy
                    newCachedResp = std::make_shared<HttpResponseImpl>(
                            *std::dynamic_pointer_cast<HttpResponseImpl>(resp));
                    newCachedResp->setExpiredTime(-1);
                }
                newCachedResp->addCookie("JSESSIONID", session_id);
                callback(newCachedResp);
                return;
            }
            callback(resp);
            return;
        }
    }

    /*find simple controller*/
    std::string pathLower(req->path());
    std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), tolower);

    if (_simpCtrlMap.find(pathLower) != _simpCtrlMap.end()) {
        auto &ctrlInfo = _simpCtrlMap[pathLower];
        if (ctrlInfo._validMethodsFlags.size() > 0) {
            assert(ctrlInfo._validMethodsFlags.size() > req->method());
            if (ctrlInfo._validMethodsFlags[req->method()] == 0) {
                //Invalid Http Method
                auto res = cnet::HttpResponse::newHttpResponse();
                res->setStatusCode(HttpResponse::k405MethodNotAllowed);
                callback(res);
                return;
            }
        }
        auto &filters = ctrlInfo.filtersName;
        doFilters(filters, req, callback, needSetJsessionid, session_id, [=]() {
            auto &ctrlItem = _simpCtrlMap[pathLower];
            const std::string &ctrlName = ctrlItem.controllerName;
            std::shared_ptr<HttpSimpleControllerBase> controller;
            HttpResponsePtr responsePtr;
            {
                //maybe update controller,so we use lock_guard to protect;
                std::lock_guard<std::mutex> guard(ctrlItem._mutex);
                controller = ctrlItem.controller;
                responsePtr = ctrlItem.responsePtr.lock();
                if (!controller) {
                    auto _object = std::shared_ptr<HttpObjectBase>(HttpClassMap::newObject(ctrlName));
                    controller = std::dynamic_pointer_cast<HttpSimpleControllerBase>(_object);
                    ctrlItem.controller = controller;
                }
            }

            if (controller) {
                if (responsePtr) {
                    //use cached response!
                    LOG_DEBUG << "Use cached response";
                    if (!needSetJsessionid)
                        callback(responsePtr);
                    else {
                        //make a copy response;
                        auto newResp = std::make_shared<HttpResponseImpl>(
                                *std::dynamic_pointer_cast<HttpResponseImpl>(responsePtr));
                        newResp->setExpiredTime(-1); //make it temporary
                        newResp->addCookie("JSESSIONID", session_id);
                        callback(newResp);
                    }
                    return;
                } else {
                    controller->asyncHandleHttpRequest(req, [=](const HttpResponsePtr &resp) {
                        auto newResp = resp;
                        if (resp->expiredTime() >= 0) {
                            //cache the response;
                            std::dynamic_pointer_cast<HttpResponseImpl>(resp)->makeHeaderString();
                            {
                                auto &item = _simpCtrlMap[pathLower];
                                std::lock_guard<std::mutex> guard(item._mutex);
                                _responseCacheMap->insert(pathLower, resp, resp->expiredTime());
                                item.responsePtr = resp;
                            }
                        }
                        if (needSetJsessionid) {
                            //make a copy
                            newResp = std::make_shared<HttpResponseImpl>(
                                    *std::dynamic_pointer_cast<HttpResponseImpl>(resp));
                            newResp->setExpiredTime(-1); //make it temporary
                            newResp->addCookie("JSESSIONID", session_id);
                        }

                        callback(newResp);
                    });
                }

                return;
            } else {
                LOG_ERROR << "can't find controller " << ctrlName;
            }
        });
        return;
    }
    //find http controller
    if (_ctrlRegex.mark_count() > 0) {
        std::smatch result;
        if (std::regex_match(req->path(), result, _ctrlRegex)) {
            for (size_t i = 1; i < result.size(); i++) {
                //FIXME:Is there any better way to find the sub-match index without using loop?
                if (!result[i].matched)
                    continue;
                if (result[i].str() == req->path() && i <= _ctrlVector.size()) {
                    size_t ctlIndex = i - 1;
                    auto &binder = _ctrlVector[ctlIndex];
                    //LOG_DEBUG << "got http access,regex=" << binder.pathParameterPattern;
                    if (binder._validMethodsFlags.size() > 0) {
                        assert(binder._validMethodsFlags.size() > req->method());
                        if (binder._validMethodsFlags[req->method()] == 0) {
                            //Invalid Http Method
                            auto res = cnet::HttpResponse::newHttpResponse();
                            res->setStatusCode(HttpResponse::k405MethodNotAllowed);
                            callback(res);
                            return;
                        }
                    }
                    auto &filters = binder.filtersName;
                    doFilters(filters, req, callback, needSetJsessionid, session_id, [=]() {
                        auto &binder = _ctrlVector[ctlIndex];

                        HttpResponsePtr responsePtr;
                        {
                            std::lock_guard<std::mutex> guard(*(binder.binderMtx));
                            responsePtr = binder.responsePtr.lock();
                        }
                        if (responsePtr) {
                            //use cached response!
                            LOG_DEBUG << "Use cached response";

                            if (!needSetJsessionid)
                                callback(responsePtr);
                            else {
                                //make a copy response;
                                auto newResp = std::make_shared<HttpResponseImpl>(
                                        *std::dynamic_pointer_cast<HttpResponseImpl>(responsePtr));
                                newResp->setExpiredTime(-1); //make it temporary
                                newResp->addCookie("JSESSIONID", session_id);
                                callback(newResp);
                            }
                            return;
                        }

                        std::vector<std::string> params(binder.parameterPlaces.size());
                        std::smatch r;
                        if (std::regex_match(req->path(), r, binder._regex)) {
                            for (size_t j = 1; j < r.size(); j++) {
                                size_t place = binder.parameterPlaces[j - 1];
                                if (place > params.size())
                                    params.resize(place);
                                params[place - 1] = r[j].str();
                                LOG_DEBUG << "place=" << place << " para:" << params[place - 1];
                            }
                        }
                        if (binder.queryParametersPlaces.size() > 0) {
                            auto qureyPara = req->getParameters();
                            for (auto parameter : qureyPara) {
                                if (binder.queryParametersPlaces.find(parameter.first) !=
                                    binder.queryParametersPlaces.end()) {
                                    auto place = binder.queryParametersPlaces.find(parameter.first)->second;
                                    if (place > params.size())
                                        params.resize(place);
                                    params[place - 1] = parameter.second;
                                }
                            }
                        }
                        std::list<std::string> paraList;
                        for (auto p : params) {
                            LOG_DEBUG << p;
                            paraList.push_back(std::move(p));
                        }

                        binder.binderPtr->handleHttpRequest(paraList, req, [=](const HttpResponsePtr &resp) {
                            LOG_DEBUG << "http resp:needSetJsessionid=" << needSetJsessionid << ";JSESSIONID="
                                      << session_id;
                            auto newResp = resp;
                            if (resp->expiredTime() >= 0) {
                                //cache the response;
                                std::dynamic_pointer_cast<HttpResponseImpl>(resp)->makeHeaderString();
                                {
                                    auto &binderIterm = _ctrlVector[ctlIndex];
                                    std::lock_guard<std::mutex> guard(*(binderIterm.binderMtx));
                                    _responseCacheMap->insert(binderIterm.pathParameterPattern, resp,
                                                              resp->expiredTime());
                                    binderIterm.responsePtr = resp;
                                }
                            }
                            if (needSetJsessionid) {
                                //make a copy
                                newResp = std::make_shared<HttpResponseImpl>(
                                        *std::dynamic_pointer_cast<HttpResponseImpl>(resp));
                                newResp->setExpiredTime(-1); //make it temporary
                                newResp->addCookie("JSESSIONID", session_id);
                            }
                            callback(newResp);
                        });
                        return;
                    });
                }
            }
        } else {
            //No controller found
            auto res = cnet::HttpResponse::newNotFoundResponse();
            if (needSetJsessionid)
                res->addCookie("JSESSIONID", session_id);

            callback(res);
        }
    } else {
        //No controller found
        auto res = cnet::HttpResponse::newNotFoundResponse();

        if (needSetJsessionid)
            res->addCookie("JSESSIONID", session_id);

        callback(res);
    }
}

void HttpAppImpl::readSendFile(const std::string &filePath, const HttpRequestPtr &req, const HttpResponsePtr &resp) {
    std::ifstream infile(filePath, std::ifstream::binary);
    LOG_DEBUG << "send http file:" << filePath;
    if (!infile) {

        resp->setStatusCode(HttpResponse::k404NotFound);
        resp->setCloseConnection(true);
        return;
    }

    std::streambuf *pbuf = infile.rdbuf();
    std::streamsize filesize = pbuf->pubseekoff(0, infile.end);
    pbuf->pubseekoff(0, infile.beg); // rewind

    if (_useSendfile &&
        filesize > 1024 * 200) {
        //The advantages of sendfile() can only be reflected in sending large files.
        std::dynamic_pointer_cast<HttpResponseImpl>(resp)->setSendfile(filePath);
    } else {
        std::string str;
        str.resize(filesize);
        pbuf->sgetn(&str[0], filesize);
        resp->setBody(std::move(str));
    }

    resp->setStatusCode(HttpResponse::k200OK);

    //cache the response for 5 seconds by default

    if (_staticFilesCacheTime >= 0) {
        resp->setExpiredTime(_staticFilesCacheTime);
        _responseCacheMap->insert(filePath, resp, resp->expiredTime(), [=]() {
            std::lock_guard<std::mutex> guard(_staticFilesCacheMutex);
            _staticFilesCache.erase(filePath);
        });
        {
            std::lock_guard<std::mutex> guard(_staticFilesCacheMutex);
            _staticFilesCache[filePath] = resp;
        }
    }
}

cnet::EventLoop *HttpAppImpl::loop() {
    return &_loop;
}

HttpApp &HttpApp::instance() {
    static HttpAppImpl _instance;
    return _instance;
}

HttpApp::~HttpApp() {
}
