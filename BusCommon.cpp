#include "BusCommon.hpp"
#include "BusSystem.hpp"
#include <iomanip>
#include <regex>
#include <sstream>

namespace Bus {
    GUID str2GUID(const std::string& guid) {
        std::regex pat("\\{[A-F0-9]{8}(-[A-F0-9]{4}){3}-[A-F0-9]{12}\\}",
                       std::regex::ECMAScript | std::regex::nosubs);
        if(std::regex_match(guid, pat)) {
            char ch[32];
            int cnt = 0;
            auto cast = [](char c) { return c - (c <= '9' ? '0' : 'A' - 10); };
            for(char c : guid)
                if(isalnum(c))
                    ch[cnt++] = cast(c);
            GUID res;
            res.first = res.second = 0;
            for(int i = 0; i < 16; ++i)
                res.first = res.first * 16ULL + ch[i];
            for(int i = 16; i < 32; ++i)
                res.second = res.second * 16ULL + ch[i];
            return res;
        } else
            throw std::logic_error("Bad GUID" + guid);
    }
    std::string GUID2Str(GUID guid) {
        std::stringstream ss;
        ss << std::setw(16) << std::setbase(16) << std::setfill('0')
           << std::noshowbase << std::uppercase << guid.first;
        ss << std::setw(16) << std::setbase(16) << std::setfill('0')
           << std::noshowbase << std::uppercase << guid.second;
        std::string str = ss.str();
        return '{' + str.substr(0, 8) + '-' + str.substr(8, 4) + '-' +
            str.substr(12, 4) + '-' + str.substr(16, 4) + '-' +
            str.substr(20, 12) + '}';
    }
}  // namespace Bus
