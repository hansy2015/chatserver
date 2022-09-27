#include "json.hpp"
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>

using namespace std;

using json = nlohmann::json;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "group.hpp"
#include "user.hpp"
#include "public.hpp"

// 当前登入用户的信息
User g_currentUser;

// 当前用户的好友列表
vector<User> g_currentUserFriendList;

// 当前用户的群组列表
vector<Group> g_currentUserGroupList;

// 当前登入成功的用户的基本信息
void showCurrentUserData();

// 控制聊天页面
bool isMainMenuRunning = false;

// 接收线程
void readTaskHandler(int clientfd);

// 获取系统时间
string getCurrentTime();

// 聊天主窗口
void mainMenu(int clientfd);

void help(int fd = 0, string str = "");

void chat(int, string);

void addfriend(int, string);

void creategroup(int, string);

void addgroup(int, string);

void groupchat(int, string);

void loginout(int, string);

int main(int argc, char **argv) {
    if (argc < 3) {
        cerr << "command invalid! example : ./ChatClient 127.0.0.1 6000" << endl;
        exit(-1);
    }
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd) {
        cerr << "socket create error" << endl;
        exit(-1);
    }
    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    // client 和 server 进行连接
    if (-1 == connect(clientfd, (sockaddr *)&server, sizeof(sockaddr_in))) {
        cerr << "connect server error" << endl;
        close(clientfd);
        exit(-1);
    }

    while (1) {
        cout << "==================================================" << endl;
        cout << "                     1. login                     " << endl;
        cout << "                     2. register                  " << endl;
        cout << "                     3. quit                      " << endl;
        cout << "==================================================" << endl;

        int choice = 0;
        cin >> choice;
        cin.get();


        switch(choice) {
            case 1: {
                int id = 0;
                char pwd[50] = {0};
                cout << "userid:";
                cin >> id;
                cin.get();
                cout << "userpassword:";
                cin.getline(pwd, 50);

                json js;
                js["msgid"] = LOGIN_MSG;
                js["id"] = id;
                js["password"] = pwd;
                string request = js.dump();
                int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
                if (-1 == len) {
                    cout << "send login msg error:" << request << endl;
                } else {
                    char buffer[1024] = {0};
                    len = recv(clientfd, buffer, 1024, 0);
                    if (-1 == len) {
                        cout << "send login msg error" << endl;
                    } else {
                        json responsejs = json::parse(buffer);
                        if (0 != responsejs["errno"]) {
                            cerr << responsejs["errmsg"] << endl;
                        } else {
                            g_currentUser.setId(responsejs["id"]);
                            g_currentUser.setName(responsejs["name"]);
                            if (responsejs.contains("friends")) {
                                // 初始化
                                g_currentUserFriendList.clear();
                                vector<string> vec = responsejs["friends"];
                                for (string &str : vec) {
                                    json js = json::parse(str);
                                    User user;
                                    user.setId(js["id"]);
                                    user.setName(js["name"]);
                                    user.setState(js["state"]);
                                    g_currentUserFriendList.push_back(user);
                                }
                            }

                            if (responsejs.contains("groups")) {
                                // 初始化
                                g_currentUserGroupList.clear();
                                vector<string> vec1 = responsejs["groups"];
                                for (string &groupstr : vec1) {
                                    json grpjs = json::parse(groupstr);
                                    Group group;
                                    group.setId(grpjs["id"]);
                                    group.setName(grpjs["groupname"]);
                                    group.setDesc(grpjs["groupdesc"]);

                                    vector<string> vec2 = grpjs["users"];
                                    for (string &userstr : vec2) {
                                        GroupUser user;
                                        json js = json::parse(userstr);
                                        user.setId(js["id"]);
                                        user.setName(js["name"]);
                                        user.setState(js["state"]);
                                        user.setRole(js["role"]);
                                        group.getUsers().push_back(user);
                                    }

                                    g_currentUserGroupList.push_back(group);
                                }
                            }

                            showCurrentUserData();

                            if (responsejs.contains("offlinemsg")) {
                                vector<string> vec = responsejs["offlinemsg"];
                                for (string &str : vec) {
                                    json js = json::parse(str);
                                    if (ONE_CHAT_MSG == js["msgid"]) {
                                        cout << js["time"] << "[" << js["id"] << "]" << js["name"]
                                        << "said : " << js["msg"] << endl;
                                    } else {
                                        cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>() << "(" << js["id"] << ")" << js["name"].get<string>()
                                        << "said: " << js["msg"].get<string>() << endl;
                                    }
                                    
                                }
                            }
                            // 该线程只需要启动一次
                            static int threadNumber = 0;
                            if (threadNumber == 0) {
                                thread readTask(readTaskHandler, clientfd);
                                readTask.detach();
                                threadNumber++;
                            }
                            
                            isMainMenuRunning =true;
                            mainMenu(clientfd);
                        }
                    }
                }
                break;
            }
            case 2: {
                char name[50];
                char pwd[50];
                cout << "username:";
                cin.getline(name, 50);
                cout << "userpassword:";
                cin.getline(pwd, 50);
                json js;
                js["msgid"] = REG_MSG;
                js["name"] = name;
                js["password"] = pwd;
                string request = js.dump();
                int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
                if (-1 == len) {
                    cout << "send reg msg error:" << request << endl;
                } else {
                    char buffer[1024] = {0};
                    len = recv(clientfd, buffer, 1024, 0);
                    if (-1 == len) {
                        cout << "recv reg response error" << endl;
                    } else {
                        json  responsejs = json::parse(buffer);
                        if (0 != responsejs["errno"]) {
                            cerr << name << " is already exist, register error!" << endl;
                        } else {
                            cout << name<< " register success, userid is " << responsejs["id"]
                            << ", do not forget it" << endl; 
                        }
                    }
                }
                break;
            }
            case 3: {
                close(clientfd);
                exit(0);
            }
        }
    }
}

void showCurrentUserData() {
    cout << "=======================login user======================" << endl;
    cout << "current login user => id:" << g_currentUser.getId() <<
    " name:" << g_currentUser.getName() << endl;
    if (!g_currentUserFriendList.empty()) {
        for (User &user : g_currentUserFriendList) {
            cout << user.getId() << " " << user.getName() << " " << user.getState() << endl;
        }
    }
    cout << "----------------------group list----------------------" << endl;
    if (!g_currentUserGroupList.empty()) {
        for (Group &group : g_currentUserGroupList) {
            cout << group.getId() << " " << group.getName() << " " << group.getDesc() << endl;
            for (GroupUser &user : group.getUsers()) {
                cout << user.getId() << " " << user.getName() << " " << user.getState() 
                << user.getRole() << endl;
            }
        }
    }
    cout << "======================================================" << endl;
}

void readTaskHandler(int clientfd) {
    for (;;) {
        char buffer[1024] = {0};
        int len = recv(clientfd, buffer, 1024, 0);
        if (-1 == len || 0 == len) {
            close(clientfd);
            exit(-1);
        }

        json js = json::parse(buffer);
        if (ONE_CHAT_MSG == js["msgid"]) {
            cout << js["time"].get<string>() << "(" << js["id"] << ")" << js["name"].get<string>()
            << " said: " << js["msg"].get<string>() << endl;
            continue;
        } 
        if (GROUP_CHAT_MSG == js["msgid"]) {
            cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>() << "(" << js["id"] << ")" << js["name"].get<string>()
            << " said: " << js["msg"].get<string>() << endl;
            continue;
        }
    }
}

// 获取系统时间
string getCurrentTime() {
    auto tt = chrono::system_clock::to_time_t(chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char data[60] = {0};
    sprintf(data, "%d-%02d-%02d %02d:%02d:%02d", 
            (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return string(data);
}

unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令，格式help"},
    {"chat", "一对一聊天，格式chat:friendid:message"},
    {"addfriend", "添加好友，格式addfriend:friendid"},
    {"creategroup", "创建群组，格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组，格式addgroup:groupid"},
    {"groupchat", "群聊，格式groupchat:groupid:message"},
    {"loginout", "注销，格式loginout"}
};

// 注册系统支持的客户端命令处理
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"loginout", loginout}
};

// 聊天主窗口
void mainMenu(int clientfd) {
    help();
    char buffer[1024] = {0};
    for (;isMainMenuRunning;) {
        cin.getline(buffer, 1024);
        string commandbuffer(buffer);
        string command;
        int idx = commandbuffer.find(":");
        if (-1 == idx) {
            command = commandbuffer;
        } else {
            command = commandbuffer.substr(0, idx);
        }
        auto it = commandHandlerMap.find(command);
        if (it == commandHandlerMap.end()) {
            cerr << "invaild input command" << endl;
            continue;
        }
        
        it->second(clientfd, commandbuffer.substr(idx + 1, commandbuffer.size() - idx));
    }
}


void help(int, string) {
    cout << "show command list" << endl;
    for (auto &p : commandMap) {
        cout << p.first << " : " << p.second << endl; 
    }
    cout << endl;
}


void addfriend(int clientfd, string str) {
    int friendid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);

    if (-1 == len) {
        cerr << "send addfriend msg error -> " << buffer <<  endl;
    }
    return;
}

void chat(int clientfd, string str) {
    
    int idx = str.find(":");
    if (-1 == idx) {
        cerr << "chat command invaild" << endl;
        return;
    }
    int friendid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);
    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["toid"] = friendid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len) {
        cerr << "send chat msg error -> " << buffer << endl;
    }
    return;
}

void creategroup(int clientfd, string str) {
    //creategroup:groupname:groupdesc
    int idx = str.find(":");
    if (-1 == idx) {
        cerr << "creategroup command invaild" << endl;
        return;
    }
    string groupname = str.substr(0, idx);
    string groupdesc = str.substr(idx + 1, str.size() - idx);
    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;
    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len) {
        cout << "send create group error -> " << buffer << endl;
    }
    return;
}

void addgroup(int clientfd, string str) {
    //addgroup:groupid
    int groupid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;
    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len) {
        cout << "send add group error -> " << buffer << endl;
    }
    return;
}

void groupchat(int clientfd, string str) {
    // groupchat:groupid:message
    int idx = str.find(":");
    if (-1 == idx) {
        cerr << "group chat command invaild" << endl;
        return;
    }
    int groupid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);
    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;
    js["name"] = g_currentUser.getName();
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len) {
        cerr << "group chat msg error -> " << buffer << endl;
    }
    return;
}

void loginout(int clientfd, string str) {
    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = g_currentUser.getId();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len) {
        cerr << "send loginout msg error -> " << buffer << endl;
    } else {
        isMainMenuRunning = false;
    }
    return;
}