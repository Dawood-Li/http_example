#pragma once

/*
    自用tcp框架

    用std::function来代替函数重载，最简化接口和数据结构。
    根据目前分析，瓶颈应该是在io，且自用的负载没那么重，暂不考虑线程池

    以下为阶段性目标：
        当前版本：ver 2
        除了exec以外还需要一个stop接口

    2 多线程处理多连接
        阻塞等待连接，子线程阻塞等待消息，同一线程同步处理消息
        初步具备多连接处理能力，但因线程瓶颈而受限
        连接数量我估计应该不大于5倍物理核心数量

    3 非阻塞轮询处理多连接
        非阻塞轮询连接，非阻塞轮询消息，同步处理消息。
        能够处理更多连接
        连接数量一定大于上一种方式，但延迟随连接数量指数上升，效能依然非常有限。
    
    4 epoll边缘触发 + 同步处理消息
        消除新连接/消息事件因连接数量而指数上升的的延迟
        连接数量理论无限 效能可观
    
    5 epoll边缘触发 + 协程
        接近单线程理论性能
*/

// c++ standard library
#include <iostream>
#include <string>
#include <functional>
#include <thread>

// linux
#include <unistd.h> // for close
#include <signal.h> // for signal

// socket
#include <sys/socket.h> // for socket, bind, connect, send, recv, shutdown, ...
#include <sys/types.h>  // i don’t know... what's include it for?
#include <netinet/in.h> // for struct sockaddr_in
#include <arpa/inet.h>  // for inet_ntoa

class Tcp_Server {

public:

    Tcp_Server() : Tcp_Server(5000) {}

    Tcp_Server(int port)
        : _port(port)
        , _thread(&Tcp_Server::loop, this) {}

    void exec() {
        _thread.join();
    }

    ~Tcp_Server() {
        exec();
        close(_socket);
    }

    struct Msg {
        int fd;
        std::string ip;
        int port;
        std::string data;
    };

    // 默认无任何行为
    std::function<void(const Msg&)> on_new_connection = [] (const Msg& msg) {};
    std::function<void(const Msg&)> on_message        = [] (const Msg& msg) {};
    std::function<void(const Msg&)> on_closed         = [] (const Msg& msg) {
        close(msg.fd);
    };
    std::function<void(const Msg&)> on_error          = [] (const Msg& msg) {
        close(msg.fd);
    };

private:

    int _socket;
    int _port;
    std::thread _thread;

    void loop() {

        // 若连接断开时仍发送 默认行为是无任何通知就结束进程
        signal(SIGPIPE, SIG_IGN);

        _socket = socket(AF_INET, SOCK_STREAM, 0);

        // 若server被kill 直接解除端口占用 而不是让端口等待2-4分钟 以便迅速重启
        {
            int reuse = 1;
            if (setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
                perror("setsockopet error\n");
                exit(EXIT_FAILURE);
            }
        }

        // 绑定协议族，端口，网卡
        {
            sockaddr_in server_addr = {
                .sin_family      = AF_INET,
                .sin_port        = htons(_port),
                .sin_addr        = { .s_addr = INADDR_ANY }
            };

            if(bind(_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
                perror("bind error\n");
                exit(EXIT_FAILURE);
            };
        }

        /*
            listen第二个参数为 未连接队列大小 = 未完成三次握手 + 已完成三次握手
            最大队列大小查询指令如下 本机最大为512
            cat /proc/sys/net/ipv4/tcp_max_syn_backlog
            已完成握手队列满 则进入未完成握手队列 连接处于等待状态
            未完成握手队列满 则无法连接
            建议有独立的线程做连接工作
        */
        if(::listen(_socket, 512) == -1) {
            perror("listen error\n");
            exit(EXIT_FAILURE);
        };

        std::cout << "listenning on port " << _port << ".\n";

        sockaddr_in client_addr;
        unsigned client_addr_len;

        while (1) {
            
            client_addr_len = sizeof(client_addr);
            
            int fd = accept(_socket, (sockaddr*)&client_addr, &client_addr_len);

            std::thread([=] () {
            
                Msg msg {
                    .fd   = fd,
                    .ip   = inet_ntoa(client_addr.sin_addr),
                    .port = ntohs(client_addr.sin_port),
                    .data = std::string()
                };

                msg.data.resize(65536);

                on_new_connection(msg);

                while (1) {
                    
                    int recv_len = recv(msg.fd, (char*)msg.data.c_str(), msg.data.size(), 0);
                    
                    if (recv_len <= 0) {
                        if (recv_len < 0) {
                            std::clog << msg.fd << " err.\n";
                            on_error(msg);
                        }
                        std::clog << msg.fd << " close.\n";
                        on_closed(msg);
                        close(msg.fd);
                        break;
                    }

                    msg.data.resize(recv_len);
                    on_message(msg);

                }
            }).detach();
        }
    }
};
