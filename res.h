#pragma once

#include <string>
#include <unordered_map>

class Response {

public:

    std::string version;                                 // 版本
    int code;                                            // 状态码
    std::unordered_map<std::string, std::string> header; // 响应头
    std::string content;                                 // 响应体
    
    Response() {
        version              = "HTTP/1.1";
        header["Server"]     = "C++ Framework by Dawood_Li";
        header["Connection"] = "Close";
    };
    
    std::string to_string() {
        std::string response = version;

        switch (code) {
        default:
        case 200: response += " 200 OK\r\n"; break;
        case 302: response += " 302 Found\r\n"; break;
        case 307: response += " 307 Temporary Redirect\r\n"; break;
        case 403: response += " 403 forbidden\r\n"; break;
        case 404: response += " 404 Not Found\r\n"; break;
        }

        if (content.size()) {
            header["Content-Length"] = std::to_string(content.size());
        }

        for (auto& h : header) {
            response += h.first + ": " + h.second + "\r\n";
        }

        return response + "\r\n" + content;
    }
};
