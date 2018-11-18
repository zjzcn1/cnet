#include "HttpUtils.h"
#include <cnet/config.h>
#include <cnet/utils/Logger.h>
#include <string>
#include <zlib.h>
#include <cnet/utils/sole.hpp>

namespace cnet {
    static const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    static inline bool is_base64(unsigned char c) {
        return (isalnum(c) || (c == '+') || (c == '/'));
    }

    bool HttpUtils::isInteger(const std::string &str) {
        for (auto c : str) {
            if (c > '9' || c < '0')
                return false;
        }
        return true;
    }

    std::string HttpUtils::genRandomString(int length) {
        static const char char_space[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        static std::once_flag once;
        static const int len = strlen(char_space);
        std::call_once(once, []() {
            std::srand(time(nullptr));
        });

        int i;
        char str[length + 1];

        for (i = 0; i < length; i++) {
            str[i] = char_space[std::rand() % len];
        }

        str[length] = 0;
        return std::string(str);
    }

    std::string HttpUtils::stringToHex(unsigned char *ptr, long long length) {
        std::string idString;
        for (long long i = 0; i < length; i++) {
            int value = (ptr[i] & 0xf0) >> 4;
            if (value < 10) {
                idString.append(1, char(value + 48));
            } else {
                idString.append(1, char(value + 55));
            }

            value = (ptr[i] & 0x0f);
            if (value < 10) {
                idString.append(1, char(value + 48));
            } else {
                idString.append(1, char(value + 55));
            }
        }
        return idString;
    }

    std::vector<std::string> HttpUtils::splitString(const std::string &str, const std::string &separator) {
        std::vector<std::string> ret;
        std::string::size_type pos1, pos2;
        pos2 = 0;
        pos1 = str.find(separator);
        while (pos1 != std::string::npos) {
            if (pos1 != 0) {
                std::string item = str.substr(pos2, pos1 - pos2);
                ret.push_back(item);
            }
            pos2 = pos1 + separator.length();
            while (pos2 < str.length() && str.substr(pos2, separator.length()) == separator)
                pos2 += separator.length();
            pos1 = str.find(separator, pos2);
        }
        if (pos2 < str.length())
            ret.push_back(str.substr(pos2));
        return ret;
    }

    std::string HttpUtils::getUuid() {
        sole::uuid u4 = sole::uuid4();
        return u4.str();
    }

    std::string HttpUtils::base64Encode(unsigned char const *bytes_to_encode, unsigned int in_len) {
        std::string ret;
        int i = 0;
        int j = 0;
        unsigned char char_array_3[3];
        unsigned char char_array_4[4];

        while (in_len--) {
            char_array_3[i++] = *(bytes_to_encode++);
            if (i == 3) {
                char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                char_array_4[3] = char_array_3[2] & 0x3f;

                for (i = 0; (i < 4); i++)
                    ret += base64_chars[char_array_4[i]];
                i = 0;
            }
        }

        if (i) {
            for (j = i; j < 3; j++)
                char_array_3[j] = '\0';

            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (j = 0; (j < i + 1); j++)
                ret += base64_chars[char_array_4[j]];

            while ((i++ < 3))
                ret += '=';
        }

        return ret;
    }

    std::string HttpUtils::base64Decode(std::string const &encoded_string) {
        int in_len = encoded_string.size();
        int i = 0;
        int j = 0;
        int in_ = 0;
        unsigned char char_array_4[4], char_array_3[3];
        std::string ret;

        while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
            char_array_4[i++] = encoded_string[in_];
            in_++;
            if (i == 4) {
                for (i = 0; i < 4; i++)
                    char_array_4[i] = base64_chars.find(char_array_4[i]);

                char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
                char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
                char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

                for (i = 0; (i < 3); i++)
                    ret += char_array_3[i];
                i = 0;
            }
        }

        if (i) {
            for (j = i; j < 4; j++)
                char_array_4[j] = 0;

            for (j = 0; j < 4; j++)
                char_array_4[j] = base64_chars.find(char_array_4[j]);

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (j = 0; (j < i - 1); j++)
                ret += char_array_3[j];
        }

        return ret;
    }

    std::string HttpUtils::urlDecode(const std::string &szToDecode) {
        std::string result;
        result.reserve(szToDecode.length());
        int hex = 0;
        for (size_t i = 0; i < szToDecode.length(); ++i) {
            switch (szToDecode[i]) {
                case '+':
                    result += ' ';
                    break;
                case '%':
                    if (isxdigit(szToDecode[i + 1]) && isxdigit(szToDecode[i + 2])) {
                        std::string hexStr = szToDecode.substr(i + 1, 2);
                        hex = strtol(hexStr.c_str(), nullptr, 16);
                        //字母和数字[0-9a-zA-Z]、一些特殊符号[$-_.+!*'(),] 、以及某些保留字[$&+,/:;=?@]
                        //可以不经过编码直接用于URL
                        if (!((hex >= 48 && hex <= 57) ||  //0-9
                              (hex >= 97 && hex <= 122) || //a-z
                              (hex >= 65 && hex <= 90) ||  //A-Z
                              //一些特殊符号及保留字[$-_.+!*'(),]  [$&+,/:;=?@]
                              hex == 0x21 || hex == 0x24 || hex == 0x26 || hex == 0x27 || hex == 0x28 || hex == 0x29 ||
                              hex == 0x2a || hex == 0x2b || hex == 0x2c || hex == 0x2d || hex == 0x2e || hex == 0x2f ||
                              hex == 0x3A || hex == 0x3B || hex == 0x3D || hex == 0x3f || hex == 0x40 || hex == 0x5f)) {
                            result += char(hex);
                            i += 2;
                        } else
                            result += '%';
                    } else {
                        result += '%';
                    }
                    break;
                default:
                    result += szToDecode[i];
                    break;
            }
        }
        return result;
    }


    char *HttpUtils::getHttpFullDate(const cnet::Date &date) {
        //rfc2616-3.3.1
        //Full Date format like this:Sun, 06 Nov 1994 08:49:37 GMT
        //                           Wed, 12 Sep 2018 09:22:40 GMT
        static __thread int64_t lastSecond = 0;
        static __thread char lastTimeString[128] = {0};
        auto nowSecond = date.microSecondsSinceEpoch() / MICRO_SECONDS_PRE_SEC;
        if (nowSecond == lastSecond) {
            return lastTimeString;
        }
        lastSecond = nowSecond;
        date.toCustomedFormattedString("%a, %d %b %Y %T GMT", lastTimeString, sizeof(lastTimeString));
        return lastTimeString;
    }

    std::string HttpUtils::formattedString(const char *format, ...) {
        std::string strBuffer;
        strBuffer.resize(1024);
        va_list ap, backup_ap;
        va_start(ap, format);
        va_copy(backup_ap, ap);
        auto result = vsnprintf((char *) strBuffer.data(), strBuffer.size(), format, backup_ap);
        va_end(backup_ap);
        if ((result >= 0) && ((std::string::size_type) result < strBuffer.size())) {
            strBuffer.resize(result);
        } else {
            while (true) {
                if (result < 0) {
                    // Older snprintf() behavior. Just try doubling the buffer size
                    strBuffer.resize(strBuffer.size() * 2);
                } else {
                    strBuffer.resize(result + 1);
                }

                va_copy(backup_ap, ap);
                auto result = vsnprintf((char *) strBuffer.data(), strBuffer.size(), format, backup_ap);
                va_end(backup_ap);

                if ((result >= 0) && ((std::string::size_type) result < strBuffer.size())) {
                    strBuffer.resize(result);
                    break;
                }
            }
        }
        va_end(ap);
        return strBuffer;
    }
} // namespace cnet
