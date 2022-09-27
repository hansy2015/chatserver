#include "json.hpp"
using json = nlohmann::json;

#include <iostream>
#include <vector>
#include <map>
#include <string>

using namespace std;

string func1() {
    json js;
    js["msg_type"] = 2;
    js["from"] = "zhang san";
    js["to"] = "li si";
    js["msg"] = "hello, what are you doing now?";
    string sendBuf = js.dump();
    return sendBuf;
}

string func2() {
    json js;
    js["id"] = {1, 2, 3, 4, 5};
    js["name"] = "zhang san";
    js["msg"]["zhang san"] = "hello world";
    js["msg"]["liu shuo"] = "hello china";
    return js.dump();
}

void func3() {
    json js;
    vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);

    js["lisi"] = vec;

    map<int, string> m;
    m.insert({1, "huangsan"});
    m.insert({2, "huasan"});
    m.insert({3, "taisan"});

    js["path"] = m;

    cout << js << endl;
}

int main() {
    string recvBuf = func2();
    // 反序列化
    json jsbuf = json::parse(recvBuf);
    cout << jsbuf["id"] << endl;
    cout << jsbuf["name"] << endl;
    cout << jsbuf["msg"]["zhang san"] << endl;
    cout << jsbuf["msg"]["liu shuo"] << endl;
    return 0;
}