#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <string>

using namespace muduo;
using namespace muduo::net;

class ChatServer {
public:
    ChatServer(EventLoop *loop, const InetAddress& listenAddr, const string& nameArg);
    void start();
private:
    // 上报连接事件的回调函数
    void onConnection(const TcpConnectionPtr&);
    // 上报读写事件的回调函数
    void onMessage(const TcpConnectionPtr&, Buffer*, Timestamp);
private:
    TcpServer _server;
    EventLoop *_loop;
};

#endif