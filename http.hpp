#pragma once

/*
    和tcp实现一样
    理论上应该需要一个stop指令
    但是没来得及写
*/

#include "./tcp_server.hpp"

#include "./req.h"
#include "./res.h"

#include <iostream>

class HTTP {

public:

    HTTP(int port) : server(port) {

        server.on_message = [this] (const Tcp_Server::Msg& msg) {
            
            // string -> http request
            Request req(msg.data);
            Response res;
            
            std::clog << req.method << " " << req.url << " " << req.version << " from " << msg.ip << " " << req.content.size() << "bytes.\n";

            // 回溯url查找注册的函数处理请求 如果没注册 则response 403
            process(req, res);

            // http response -> string
            std::string res_str = res.to_string();

            send(msg.fd, res_str.c_str(), res_str.size(), 0); 
            close(msg.fd);
        };

        _default = [] (Request& req, Response& res) {
            res.code = 403;
            res.header["Content-Type"] = "text/html";
            res.content = "<html><body><h1>403 fxxk you</h1></body></html>";
        };
        
        route["GET"]["/"] = [this] (Request& req, Response& res) {
            // 根据url确定文件名 根目录则定向到主页
            const std::string&& filename = req.url == "/" ? "/index.html" : req.url;
        };
    }

    // 默认处理函数
    std::function<void(Request& req, Response& res)> _default;

    // 处理函数容器
    std::unordered_map<std::string,
        std::unordered_map<std::string,
            std::function<void(Request& req, Response& res)>>>
                route;

    // 阻塞
    void exec() { server.exec(); }

    ~HTTP() { exec(); }

private:
    Tcp_Server server;

    void process(Request& req, Response& res) {
        
        std::function<void(Request& req, Response& res)> *func = &_default;
        for(auto& r : route) {
            if(req.method != r.first) { continue; } // 比对method
            for (std::string url_buf = req.url; url_buf.size(); ) { // 比对url
                // 已找到url 退出循环
                if(r.second.count(url_buf)) {
                    func = &r.second.at(url_buf);
                    break;
                }
                // 截断url 继续循环
                if(url_buf[url_buf.size() - 1] == '/') {
                    url_buf.resize(url_buf.size()-1);
                }
                url_buf = url_buf.substr(0, url_buf.find_last_of('/') + 1);
            }
        }

        // 执行处理函数
        (*func)(req, res);
    }
};
