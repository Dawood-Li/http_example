#pragma once

#include <string>
#include <unordered_map>

class Request {

public:

    std::string method;  // 请求类型
    std::string url;     // 请求url
    std::string version; // http版本

    std::unordered_map<std::string, std::string> header; // 请求头

    std::string content; // 请求体

    Request() = default;

    Request(const std::string& buffer) {
        
        const char* p = buffer.c_str();

        // 截取到空格之前 并移动指针到空格之后
        auto get_part = [&]() {
            int len;
            for (len = 0; *(p + len) != ' '; ++len);
            std::string str(p, len);
            p += len + 1;
            return str;
        };

        // 截取到行尾 并移动指针到下一行
        auto get_line = [&]() {
            int len;
            for (len = 0; !(*(p + len) == '\r' && *(p + len + 1) == '\n'); ++len);
            std::string str(p, len);
            p += len + 2;
            return str;
        };

        // 截取每行请求头冒号以前，并移动指针到冒号空格之后。
        auto get_key = [&]() {
            int len;
            for (len = 0; *(p + len) != ':'; ++len);
            std::string str(p, len);
            p += len + 2;
            return str;
        };

        // 检查是否空行
        auto blank_line = [&]() {
            return (*p == '\r' && *(p + 1) == '\n');
        };

        // 读取第一行
        method  = get_part();
        url     = get_part();
        version = get_line();

        // 读取请求头
        while (!blank_line()) {
            auto key   = get_key();
            auto value = get_line();
            header[key] = value;
        }
        
        // 读取请求体
        if(header.count("Content-Length")) {
            content = std::string(p+2, stoi(header["Content-Length"]));
        }
    }

    std::string to_string() {
        std::string response = method + ' ' + url + ' ' + version + "\r\n";

        if (content.size()) {
            header["Content-Length"] = std::to_string(content.size());
        }

        for (auto& h : header) {
            response += h.first + ": " + h.second + "\r\n";
        }

        return response + "\r\n" + content;
    }
};
