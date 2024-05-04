#pragma once

#include "groupuser.h"
#include <string>
#include <vector>

// 群组表
class Group
{
public:
    Group(int id = -1, std::string name = "", std::string desc = "")
        : id_(id)
        , name_(name)
        , desc_(desc)
    {}

    void setId(int id) { id_ = id; }
    void setName(std::string name) { name_ = name; }
    void setDesc(std::string desc) { desc_ = desc; }

    int getId() { return id_; }
    std::string getName() { return name_; }
    std::string getDesc() { return desc_; }
    std::vector<GroupUser> &getUsers() { return this->users; }

private:
    int id_;                        // 群id
    std::string name_;              // 群名称
    std::string desc_;              // 群描述
    std::vector<GroupUser> users;   // 群成员信息
};