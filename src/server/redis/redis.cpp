#include "redis.hpp"
#include <iostream>

using namespace std;

Redis::Redis()
        : _publish_context(nullptr), _subcribe_context(nullptr) {

}

Redis::~Redis() {
    if (_publish_context == nullptr) {
        redisFree(_publish_context);
    }
    if (_subcribe_context == nullptr) {
        redisFree(_subcribe_context);
    }
}

bool Redis::connect() {
    // 负责publish发布消息的上下文
    _publish_context = redisConnect("127.0.0.1", 6379);
    if (_publish_context == nullptr) {
        cerr << "connected redis failed" << endl;
        return false;
    }
    _subcribe_context = redisConnect("127.0.0.1", 6379);
    if (_subcribe_context == nullptr) {
        cerr << "connected redis failed" << endl;
        return false;
    }
    thread t([&](){observer_channel_message();});
    t.detach();
    cout << "connected success" << endl;
    return true;
}


bool Redis::publish(int channel, string message) {
    redisReply *reply = (redisReply *)redisCommand(_publish_context, "PUBLISH %d %s", channel, message);
    if (nullptr == reply) {
        cerr << "publish command failed" << endl;
        return false;
    }
    freeReplyObject(reply);
    return true;
}


bool Redis::subscribe(int channel) {
    if (REDIS_ERR == redisAppendCommand(this->_subcribe_context, "SUBSCRIBE %d", channel)) {
        cerr << "subscribe command failed" << endl;
        return false;
    }

    int done = 0;
    while (!done) {
        if (REDIS_ERR == redisBufferWrite(this->_subcribe_context, &done)) {
            cerr << "subscribe command failed" << endl;
            return false;
        }
    }
    return true;
}


bool Redis::unsubscribe(int channel) {
    if (REDIS_ERR == redisAppendCommand(this->_subcribe_context, "UNSUBSCRIBE %d", channel)) {
        cerr << "unsubscribe command failed" << endl;
        return false;
    }
    int done = 0;
    while (!done) {
        if (REDIS_ERR == redisBufferWrite(this->_subcribe_context, &done)) {
            cerr << "unsubscribe command failed" << endl;
            return false;
        }
    }
    return true;
}

// 在独立线程中接收订阅通道的消息
void Redis::observer_channel_message() {
    redisReply *reply = nullptr;
    while (REDIS_OK == redisGetReply(this->_subcribe_context, (void **)&reply)) {
        // 收到的消息是一个三元组信息
        if (reply != nullptr && reply->element[2] != nullptr && reply->element[2]->str != nullptr) {
            // 调用回调操作，给业务层上报id+msg
            _notify_message_handler(atoi(reply->element[1]->str), reply->element[2]->str);
        }
        freeReplyObject(reply);
    }
    cerr << ">>>>>>>>>>>>>>> oberver channel message quit <<<<<<<<<<<<" << endl;
}

void Redis::init_notify_handler(function<void(int, string)> fn) {
    this->_notify_message_handler = fn;
}