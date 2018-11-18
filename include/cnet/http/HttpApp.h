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

#include "http/HttpUtils.h"
#include <cnet/http/HttpBinder.h>
#include <cnet/utils/NonCopyable.h>
#include <cnet/http/HttpObject.h>
#include <cnet/http/HttpRequest.h>
#include <cnet/http/HttpResponse.h>
#include <cnet/http/LocalHostFilter.h>
#include <cnet/http/InnerIpFilter.h>
#include <cnet/http/NotFound.h>
#include <cnet/http/HttpClient.h>
#include <cnet/http/FileUpload.h>
#include <cnet/tcp/EventLoop.h>
#include <cnet/http/CacheMap.h>
#include <memory>
#include <string>
#include <functional>
#include <vector>

namespace cnet {
//the cnet banner
    const char banner[] = "     _                             \n"
                          "  __| |_ __ ___   __ _  ___  _ __  \n"
                          " / _` | '__/ _ \\ / _` |/ _ \\| '_ \\ \n"
                          "| (_| | | | (_) | (_| | (_) | | | |\n"
                          " \\__,_|_|  \\___/ \\__, |\\___/|_| |_|\n"
                          "                 |___/             \n";

    class HttpApp : public cnet::NonCopyable {
    public:
        ///Get the instance of HttpAppFramework
        /**
         * HttpAppFramework works at singleton mode, so any calling of this
         * method will get the same instance;
         * Calling cnet::HttpAppFramework::instance()
         * can be replaced by a simple interface -- cnet::app()
         */
        static HttpApp &instance();

        ///Run the event loop
        /**
         * Calling this method will start the event loop which drive the network;
         * The thread calling this method must be the first thread to call instance() method;
         * Usually the thread calling this method is main thread of the application;
         */
        virtual void run() = 0;

        ///Quit the event loop
        /**
         * Calling this method will result in stopping all network IO in the
         * framework and interrupting the blocking of the run() method. Usually,
         * after calling this method, the application will exit.
         *
         * NOTE:
         * This method can be called in any thread and anywhere.
         * This method should not be called before calling run().
         */
        virtual void quit() = 0;

        virtual void setThreadNum(size_t threadNum) = 0;

        virtual void setSSLFiles(const std::string &certPath,
                                 const std::string &keyPath) = 0;

        virtual void addListener(const std::string &ip,
                                 uint16_t port,
                                 bool useSSL = false,
                                 const std::string &certFile = "",
                                 const std::string &keyFile = "") = 0;

        virtual ~HttpApp();

        virtual void registerWebSocketController(const std::string &pathName,
                                                 const std::string &crtlName,
                                                 const std::vector<std::string> &filters =
                                                 std::vector<std::string>()) = 0;

        virtual void registerHttpSimpleController(const std::string &pathName,
                                                  const std::string &crtlName,
                                                  const std::vector<any> &filtersAndMethods = std::vector<any>()) = 0;

        template<typename FUNCTION>
        void registerHttpMethod(const std::string &pathPattern,
                                FUNCTION &&function,
                                const std::vector<any> &filtersAndMethods = std::vector<any>()) {
            LOG_TRACE << "pathPattern:" << pathPattern;
            HttpBinderBasePtr binder;

            binder = std::make_shared<
                    HttpBinder<FUNCTION>>(std::forward<FUNCTION>(function));

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

            registerHttpController(pathPattern, binder, validMethods, filters);
        }

        virtual void enableSession(const size_t timeout = 0) = 0;

        virtual void disableSession() = 0;

        virtual const std::string &getDocumentRoot() const = 0;

        virtual void setDocumentRoot(const std::string &rootPath) = 0;

        virtual void setFileTypes(const std::vector<std::string> &types) = 0;

        virtual void setMaxConnectionNum(size_t maxConnections) = 0;

        virtual void setMaxConnectionNumPerIP(size_t maxConnectionsPerIP) = 0;

        virtual void enableRunAsDaemon() = 0;

        virtual void enableRelaunchOnError() = 0;

        virtual void setLogPath(const std::string &logPath,
                                const std::string &logfileBaseName = "",
                                size_t logSize = 100000000) = 0;

        virtual void enableSendfile(bool sendFile) = 0;

        virtual void enableGzip(bool useGzip) = 0;

        virtual bool useGzip() const = 0;

        virtual void setStaticFilesCacheTime(int cacheTime) = 0;

        virtual int staticFilesCacheTime() const = 0;

        virtual void setIdleConnectionTimeout(size_t timeout) = 0;

    private:
        virtual void registerHttpController(const std::string &pathPattern,
                                            const HttpBinderBasePtr &binder,
                                            const std::vector<HttpMethod> &validMethods = std::vector<HttpMethod>(),
                                            const std::vector<std::string> &filters = std::vector<std::string>()) = 0;
    };

    inline HttpApp &app() {
        return HttpApp::instance();
    }

} // namespace cnet
