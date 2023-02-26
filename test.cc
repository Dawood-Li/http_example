#include "./http.hpp"
#include "./tcp_server.hpp"
#include "./timer.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <set>

#include <filesystem>
#include <iostream>
#include <vector>

using namespace std;
using namespace std::filesystem;

/*
    1 依赖倒置 控制反转 用http去控制tcp
    2 路由
    // 206状态码 和range字段 可以断点续传 但是👴懒的写 又不是不能用
*/

// 文件缓存
std::string loadfile(std::string filename);

// 文件夹->json
string getfloderjson(std::filesystem::path path);

// MIME TYPE
const unordered_map<string, string> mime_type {
    // default: "application/octet-stream"
    { "txt",  "text/plain; charset=\"utf-8\"" },
    { "html", "text/html; charset=\"utf-8\"" },
    { "css",  "text/css; charset=\"utf-8\"" },
    { "js",   "text/javascript; charset=\"utf-8\"" },

    { "zip",  "application/zip" },
    { "7z",  "application/zip"  },

    { "png",  "image/png"     },
    { "gif",  "image/gif"     },
    { "jpeg", "image/jpeg"    },
    { "bmp",  "image/bmp"     },
    { "svj",  "image/svj+xml" },
    { "ico",  "image/x-icon"  },
};

int main(int argc, char** argv) {

    int port = 80;
    
    std::filesystem::path root("/root/tcp"); // web根目录

    HTTP http(port);

    http.route["GET"]["/"] = [&] (Request& req, Response& res) {
        // Timer t("load " + filename);

        cout << "nb666\n";

        // 现在根目录被用于获取文件根目录的json
        // if (req.url.back() != '/' || req.url == "/") {
            // const string&& filename = "." + (req.url == "/" ? "/index.html" : req.url);

        // 情况1 get文件
        if (req.url.back() != '/') {

            const string&& filename = "." + req.url;

            res.content = loadfile(filename);
            if (res.content.size() == 0) {
                res.code = 404;
                res.header["Content-Type"] = "text/html";
                res.content = "<html><body><h1>404 not found</h1></body></html>";
                return;
            }

            // 读入string 并自动计算content长度
            res.code = 200;
            res.header["Content-Length"] = std::to_string(res.content.size());
            
            // 获取文件名后缀 并 根据后缀 判断类型 若没注册该类型 使用默认类型
            string suffix = filename.substr(filename.find_last_of('.') + 1);
            res.header["Content-Type"] = mime_type.count(suffix) ? mime_type.at(suffix) : "application/octet-stream";
        }

        // 情况2 get目录
        else if (req.url.back() == '/') {
            res.code = 200;
            res.header["Content-Type"] = "application/json";
            
            auto path =root;
            path += req.url;
            res.content = getfloderjson(path);
        }

        else {
            res.code = 403;
            res.header["Content-Type"] = "text/html";
            res.content = "<html><body><h1>403 fuck you</h1></body></html>";
            return;
        }
    };

    // api请求
    http.route["POST"]["/"] = [&] (Request& req, Response& res) {

        // 情况1 post目录
        if (req.url.back() == '/') {
            
            // 创建不存在的目录
            auto path = root;
            path += req.url;
            if (exists(path) == false) {
                create_directory(path);
            }
        }

        // 情况2 post文件
        else if (req.url.back() != '/')  {
            auto path = root;
            path += req.url;
            if (exists(path.parent_path()) == false) {
                create_directory(path.parent_path());
            }

            cout << "content(" << req.content.size() << ")" << endl;
            for (auto c : req.content) {
                printf("[%d]", c);
            }
            cout << endl;
            ofstream(path.string()) << req.content << flush;
        }

        else {
            res.code = 403;
            res.header["Content-Type"] = "text/html";
            res.content = "<html><body><h1>403 fuck you</h1></body></html>";
            return;
        }
    };


    Tcp_Server chat(81);

    set<int> connections;

    chat.on_new_connection = [&] (const Tcp_Server::Msg& msg) {
        clog << "new connection from " << msg.ip << endl;
        connections.insert(msg.fd);
    };

    chat.on_closed = [&] (const Tcp_Server::Msg& msg) {
        clog << "connection close from " << msg.ip << endl;
        // close(msg.fd);
        connections.erase(msg.fd);
    };
    chat.on_error = [&] (const Tcp_Server::Msg& msg) {
        clog << "connection error from " << msg.ip << endl;
        connections.erase(msg.fd);
    };

    chat.on_message = [&] (const Tcp_Server::Msg& msg) {
        auto data = msg.ip + ": " + msg.data;
        for (auto& conn : connections) {
            send(conn, data.c_str(), data.size(), 0);
        }
        clog << "chat from " << data << endl;
    };

    http.exec();
    chat.exec();
}

// 文件缓存
std::string loadfile(std::string filename) {
    
    static std::map<std::string, std::string> file_cache;

    Timer t("load " + filename);

    if (file_cache.count(filename) == 0) {
        ifstream f(filename);
        if(!f.is_open()) { return ""; }
        file_cache[filename] = string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    }

    return file_cache[filename];
}

// 文件夹->json
string getfloderjson(std::filesystem::path path) {
    struct fileinfo {
        string name;
        size_t size;
    };

    vector<fileinfo> files, floders;

    if (is_directory(path) == false) {
        cerr << path.string() << " no such file or direcoty.\n";
    } else {

        for (auto &p : directory_iterator(path)) {

            string name = p.path().filename().string();
            // string type;
            size_t size = 0;

            if (p.is_directory() == false) {
                // type = "file";
                size = p.file_size();
                files.push_back({name, size});
            }

            else {
                // type = "floder";
                for (auto &p2 : directory_iterator(p)) {
                    size++;
                }
                floders.push_back({name, size});
            }

        }
    }

    string json = "{\n";
        json += "  \"files\":[\n";
        for (auto& file : files) {
            json += "    {\n      \"name\":\"" + file.name + "\",\n      \"size\":" + to_string(file.size) + "\n    },\n";
        }
        json.resize(json.size()-2);
        json += "\n  ],\n";

        json += "\t\"floders\":[\n";
        for (auto& file : floders) {
            json += "    {\n      \"name\":\"" + file.name + "\",\n      \"size\":" + to_string(file.size) + "\n    },\n";
        }
        json.resize(json.size()-2);
        json += "\n  ]\n";
        
    json += "}";
    return json;
}
