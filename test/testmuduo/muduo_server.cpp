/*
    muduo主要提供了两个类
    TcpServer: 用于编写服务器
    TcpClient: 用于编写客户端

    epoll + 线程池
    把网络IO和业务代码分离开
*/

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>

#include <iostream>
#include <functional>
#include <string>

using namespace std;
using namespace muduo;
using namespace muduo::net;
using namespace placeholders;

/*
1.组合Tcp对象
2.创建EventLoop时间循环
3.明确TcpServer构造函数
4.在当前服务器类的构造函数，注册两个函数
5.设置合适的服务端线程数量
epoll + 线程池

*/

class ChatServer {
public:
    ChatServer(EventLoop *loop, // 时间循环
            const InetAddress& listenAddr, // IP + Port
            const string& nameArg) // 服务器的名字
            :_server(loop, listenAddr, nameArg)
            ,_loop(loop) {
        // 给服务器注册用户连接的创建和断开回调
        _server.setConnectionCallback(bind(&ChatServer::onConnection, this, _1));
        // 给服务器注册用户读写事件回调
        _server.setMessageCallback(bind(&ChatServer::onMessage, this, _1, _2, _3));
        // 设置服务器线程， 一个IO线程三个worker线程
        _server.setThreadNum(4);

    }

    void start() {
        _server.start();
    }

private:
    // 专门处理用户的连接和断开 epoll listenfd accept
    void onConnection(const TcpConnectionPtr &conn) {
        if (conn->connected()) {
            cout << conn->peerAddress().toIpPort() << "->"
            << conn->localAddress().toIpPort() << "state : online" << endl;
        } else {
            cout << conn->peerAddress().toIpPort() << "->"
            << conn->localAddress().toIpPort() << "state : offline" << endl;
            conn->shutdown();
        }
    }

    void onMessage(const TcpConnectionPtr &conn, // 连接
                    Buffer *buffer,
                    Timestamp time
    ) {
        string buff = buffer->retrieveAllAsString();
        cout << "recv data: " << buff << " time: " << time.toString() << endl;
        conn->send(buff);
    }
private:
    TcpServer _server; // #1
    EventLoop *_loop; // #2 epoll


};

int main() {
    EventLoop loop;
    InetAddress addr("127.0.0.1", 6000);
    ChatServer server(&loop, addr, "ChatServer");

    server.start();
    loop.loop(); // epoll_wait()以阻塞的方式等待新用户的连接
    return 0;
}



