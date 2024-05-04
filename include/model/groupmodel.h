#pragma once

#include "group.hpp"
#include <string>
#include <vector>

// 维护群组信息的操作接口方法
class GroupModel
{
public:
    // 创建群组
    bool createGroup(Group &group);
    // 加入群组
    bool addGroup(int userid, int groupid, std::string role);
    // 查询群组是否存在
    bool isGroups(int groupid);
    // 查询用户所在群组信息
    std::vector<Group> queryGroups(int userid);
    // 根据指定的groupid查询群组用户id列表，除userid自己，主要用户群聊业务给群组其它成员群发消息
    std::vector<int> queryGroupUsers(int userid, int groupid);
};