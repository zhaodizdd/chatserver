#pragma once

#include "user.hpp"
#include <vector>
using namespace std;

// 维护好友信息的操作接口方法
class FriendModel
{
public:
    FriendModel() = default;
    // 添加好友关系
    bool insert(int userid, int friendid);

    // 查找好友

    // 返回用户好友列表
    vector<User> query(int userid);
};