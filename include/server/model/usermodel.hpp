#ifndef USERMODEL_H
#define USERMODEL_H

#include "user.hpp"

class UserModel {
public:
    // User表的增加方法
    bool insert(User &user);
    // 根据id查询信息
    User query(int id);
    // 跟新用户的状态信息
    bool updateState(User user);
    // 重置用户的状态信息
    void resetState();
};

#endif