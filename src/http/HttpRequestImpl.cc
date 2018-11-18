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

#include "HttpRequestImpl.h"

#include <iostream>
using namespace cnet;

void HttpRequestImpl::parsePremeter()
{
    std::string type = getHeader("Content-Type");

    std::transform(type.begin(), type.end(), type.begin(), tolower);
    const std::string &input = query();
    if (_method == Get || (_method == Post && (type == "" || type.find("application/x-www-form-urlencoded") != std::string::npos)))
    {

        std::string::size_type pos = 0;
        while ((input[pos] == '?' || isspace(input[pos])) && pos < input.length())
        {
            pos++;
        }
        std::string value = input.substr(pos);
        while ((pos = value.find("&")) != std::string::npos)
        {
            std::string coo = value.substr(0, pos);
            auto epos = coo.find("=");
            if (epos != std::string::npos)
            {
                std::string key = coo.substr(0, epos);
                std::string::size_type cpos = 0;
                while (cpos < key.length() && isspace(key[cpos]))
                    cpos++;
                key = key.substr(cpos);
                std::string pvalue = coo.substr(epos + 1);
                std::string pdecode = HttpUtils::urlDecode(pvalue);
                std::string keydecode = HttpUtils::urlDecode(key);
                _parameters[keydecode] = pdecode;
            }
            value = value.substr(pos + 1);
        }
        if (value.length() > 0)
        {
            std::string &coo = value;
            auto epos = coo.find("=");
            if (epos != std::string::npos)
            {
                std::string key = coo.substr(0, epos);
                std::string::size_type cpos = 0;
                while (cpos < key.length() && isspace(key[cpos]))
                    cpos++;
                key = key.substr(cpos);
                std::string pvalue = coo.substr(epos + 1);
                std::string pdecode = HttpUtils::urlDecode(pvalue);
                std::string keydecode = HttpUtils::urlDecode(key);
                _parameters[keydecode] = pdecode;
            }
        }
    }
    LOG_TRACE << "_parameters:";
    for (auto iter : _parameters)
    {
        LOG_TRACE << iter.first << "=" << iter.second;
    }
}

void HttpRequestImpl::appendToBuffer(MsgBuffer *output) const
{
    switch (_method)
    {
    case Get:
        output->append("GET ");
        break;
    case Post:
        output->append("POST ");
        break;
    case Head:
        output->append("HEAD ");
        break;
    case Put:
        output->append("PUT ");
        break;
    case Delete:
        output->append("DELETE ");
        break;
    default:
        return;
    }

    if (_path.size() != 0)
    {
        output->append(_path);
    }
    else
    {
        output->append("/");
    }

    if (_parameters.size() != 0)
    {
        output->append("?");
        for (auto p : _parameters)
        {
            output->append(p.first);
            output->append("=");
            output->append(p.second);
            output->append("&");
        }
        output->unwrite(1);
    }

    output->append(" ");
    if (_version == kHttp11)
    {
        output->append("HTTP/1.1");
    }
    else if (_version == kHttp10)
    {
        output->append("HTTP/1.0");
    }
    else
    {
        return;
    }
    output->append("\r\n");

    for (std::map<std::string, std::string>::const_iterator it = _headers.begin();
         it != _headers.end();
         ++it)
    {
        output->append(it->first);
        output->append(": ");
        output->append(it->second);
        output->append("\r\n");
    }
    if (_cookies.size() > 0)
    {
        output->append("Cookie: ");
        for (auto it = _cookies.begin(); it != _cookies.end(); it++)
        {
            output->append(it->first);
            output->append("= ");
            output->append(it->second);
            output->append(";");
        }
        output->unwrite(1); //delete last ';'
        output->append("\r\n");
    }

    output->append("\r\n");

    //LOG_INFO<<"request(no body):"<<output->peek();
    output->append(content_);
}

HttpRequestPtr HttpRequest::newHttpRequest()
{
    auto req = std::make_shared<HttpRequestImpl>();
    req->setMethod(cnet::Get);
    req->setVersion(cnet::HttpRequest::kHttp11);
    return req;
}
