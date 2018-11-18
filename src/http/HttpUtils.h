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
#include <cnet/utils/Date.h>
#include <string>
#include <vector>

namespace cnet {
    class HttpUtils {
    public:
        static bool isInteger(const std::string &str);

        static std::string genRandomString(int length);

        static std::string stringToHex(unsigned char *ptr, long long length);

        static std::vector<std::string> splitString(const std::string &str, const std::string &separator);

        static std::string getUuid();

        static std::string base64Encode(unsigned char const *bytes_to_encode, unsigned int in_len);

        static std::string base64Decode(std::string const &encoded_string);

        static std::string urlDecode(const std::string &szToDecode);

        static char *getHttpFullDate(const cnet::Date &date);

        static std::string formattedString(const char *format, ...);
    };
} // namespace cnet
