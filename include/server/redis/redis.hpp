#ifndef REDIS_H
#define REDIS_H

#include <hiredis/hiredis.h>
#include <thread>
#include <functional>

using namespace std;

class Redis {
public:
    Redis();
    ~Redis();
    // 连接redis服务器
    bool connect();
    // 向redis指定通道发消息
    bool publish(int channel, string message);
    // 向指定通道订阅消息
    bool subscribe(int channel);
    // 向指定通道取消订阅
    bool unsubscribe(int channel);
    // 在独立线程中接收订阅通道中的消息
    void observer_channel_message();

    // 初始化向业务层上报回调对象
    void init_notify_handler(function<void(int, string)> fn);
private:
    // hiredis同步上下文对象，负责publish消息
    redisContext *_publish_context;
    redisContext *_subcribe_context;

    function<void(int, string)> _notify_message_handler;
};

#endif