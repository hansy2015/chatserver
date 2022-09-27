#include "chatservice.hpp"
#include "public.hpp"
#include <string>
#include <muduo/base/Logging.h>
#include <vector>

using namespace muduo;


// 获取单例对象的接口函数
ChatService* ChatService::instance() {
    static ChatService service;
    return &service;
}

// 注册消息以及对应的Handler回调操作
ChatService::ChatService() {
    _msgHandlerMap.insert({LOGIN_MSG, bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, bind(&ChatService::onChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, bind(&ChatService::addFriend, this, _1, _2, _3)});
    _msgHandlerMap.insert({CREATE_GROUP_MSG, bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, bind(&ChatService::groupChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGINOUT_MSG, bind(&ChatService::loginout, this, _1, _2, _3)});
    if (_redis.connect()) {
        _redis.init_notify_handler(bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

// 处理登录业务 ORM 在业务层操作的都是对象 DAO
void ChatService::login(const TcpConnectionPtr& conn, json& js, Timestamp time) {
    LOG_INFO << "do login service[]";
    int id = js["id"];
    string pwd = js["password"];

    User user = _userModel.query(id);
    if (user.getPassword() == pwd && user.getId() == id) {
        if (user.getState() == "online") {
            // 该用户已经登录不允许重复登录
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 1;
            response["errmsg"] = "该账号已经登录";
            conn->send(response.dump());
        } else {
            // 登入成功，offline->online
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }
            // 向redis中订阅id的消息
            _redis.subscribe(id);
            user.setState("online");
            if (_userModel.updateState(user)) {
                json response;
                response["msgid"] = LOGIN_MSG_ACK;
                response["errno"] = 0;
                response["id"] = user.getId();
                response["name"] = user.getName();
                // 查询该用户是否有离线消息
                vector<string> vec = _offlineMsgModel.query(user.getId());
                if (!vec.empty()) {
                    response["offlinemsg"] = vec;
                    // 读取该用户的离线消息后，把该用户的离线消息删除掉
                    _offlineMsgModel.remove(id);
                }
                vector<User> userVec = _friendModel.query(id);
                if (!userVec.empty()) {
                    vector<string> vec2;
                    for (User &user : userVec) {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        vec2.push_back(js.dump());
                    }
                    response["friends"] = vec2;
                }

                vector<Group> groupuserVec = _groupModel.queryGroup(id);
                if (!groupuserVec.empty()) {
                    vector<string> groupV;
                    for (Group &group : groupuserVec) {
                        json grpjson;
                        grpjson["id"] = group.getId();
                        grpjson["groupname"] = group.getName();
                        grpjson["groupdesc"] = group.getDesc();
                        vector<string> userV;
                        for (GroupUser &user : group.getUsers()) {
                            json js;
                            js["id"] = user.getId();
                            js["name"] = user.getName();
                            js["state"] = user.getState();
                            js["role"] = user.getRole();
                            userV.push_back(js.dump());
                        }
                        grpjson["users"] = userV;
                        groupV.push_back(grpjson.dump());
                    }
                    response["groups"] = groupV;
                }
                conn->send(response.dump());
            } else {
                user.setState("offline");
                json response;
                response["msgid"] = LOGIN_MSG_ACK;
                response["errno"] = 1;
                response["errmsg"] = "状态更新失败";
                conn->send(response.dump());
            }
             
        }
        
    } else {
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "用户名密码错误";
        conn->send(response.dump());
    }
}

void ChatService::reg(const TcpConnectionPtr& conn, json& js, Timestamp time) {
    LOG_INFO << "do reg service[]";
    string name = js["name"];
    string pwd = js["password"];
    User user;
    user.setName(name);
    user.setPassword(pwd);
    bool state = _userModel.insert(user);
    if (state) {
        // 注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    } else {
        // 注册失败
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        conn->send(response.dump());
    }
}

void ChatService::reset() {
    // 把所有online状态用户，设置成offline
    _userModel.resetState();

}

MsgHandler ChatService::getHandler(int msgid) {
    // 记录错误日志
    auto it = _msgHandlerMap.find(msgid);
    if (it == _msgHandlerMap.end()) {
        //LOG_ERROR << "msgid: " << msgid << "can not find handler!";
        // string errstr = "msgid:" + msgid
        // 返回一个默认的处理器
        return  [=](const TcpConnectionPtr &conn, json &js, Timestamp) {
            LOG_ERROR << "msgid: " << msgid << "can not find handler!";
        };
    } else {
        return _msgHandlerMap[msgid];
    }
}

void ChatService::clientCloseException(const TcpConnectionPtr& conn) {
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); it++) {
            if (it->second == conn) {
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }
    _redis.unsubscribe(user.getId());
    // 更新用户信息
    user.setState("offline");
    _userModel.updateState(user);

}

void ChatService::onChat(const TcpConnectionPtr& conn, json& js, Timestamp time) {
    int toid = js["toid"];
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if (it != _userConnMap.end()) {
            // toid 在线, 转发消息，服务器主动推送消息个toid用户
            it->second->send(js.dump());
            return;
        }
    }
    User user = _userModel.query(toid);
    if (user.getState() == "online") {
        _redis.publish(toid, js.dump());
        return;
    }
    // toid 不在线，存储离线消息
    _offlineMsgModel.insert(toid, js.dump());
}

// 添加好友业务
void ChatService::addFriend(const TcpConnectionPtr& conn, json& js, Timestamp time) {
    int userid = js["id"];
    int friendid = js["friendid"];
    _friendModel.insert(userid, friendid);
}

// 创建群组
void ChatService::createGroup(const TcpConnectionPtr& conn, json& js, Timestamp time) {
    int userid = js["id"];
    string name = js["groupname"];
    string desc = js["groupdesc"];

    Group group(-1, name, desc);

    if (_groupModel.createGroup(group)) {
        _groupModel.addGroup(userid, group.getId(), "createor");
    }
}

// 加入群组
void ChatService::addGroup(const TcpConnectionPtr& conn, json& js, Timestamp time) {
    int userid = js["id"];
    int groupid = js["groupid"];
    _groupModel.addGroup(userid, groupid, "normal");
}

// 群聊天业务

void ChatService::groupChat(const TcpConnectionPtr& conn, json& js, Timestamp time) {
    int userid = js["id"];
    int groupid = js["groupid"];

    vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);
    lock_guard<mutex> lock(_connMutex);
    for (int id : useridVec) {
        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end()) {
            it->second->send(js.dump());
        } else {
            User user = _userModel.query(id);
            if (user.getState() == "online") {
                _redis.publish(id, js.dump());
            } else {
                _offlineMsgModel.insert(id, js.dump());
            }
        }
    }
}


void ChatService::loginout(const TcpConnectionPtr& conn, json& js, Timestamp time) {
    int userid = js["id"];
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if (it != _userConnMap.end()) {
            _userConnMap.erase(it);
        }
    }
    _redis.unsubscribe(userid);
    User user(userid, "", "", "offline");
    // 更新用户信息
    user.setState("offline");
    _userModel.updateState(user);
}

void ChatService::handleRedisSubscribeMessage(int userid, string msg) {
    //json js = json::parse(msg.c_str());

    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if (it != _userConnMap.end()) {
        it->second->send(msg);
        return;
    }
    _offlineMsgModel.insert(userid, msg);
}
