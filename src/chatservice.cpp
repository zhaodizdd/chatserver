#include "chatservice.h"
#include "msg.h"
#include "net/Logger.h"
#include "json.hpp"

#include <vector>

using namespace std::placeholders;
using json = nlohmann::json;

// 获取单例对象的接口函数
ChatService *ChatService::instance()
{
    static ChatService service;
    return &service;
}

// 注册消息以及对应的Handler回调操作
ChatService::ChatService()
{
    // 用户基本业务管理相关事件处理回调注册
    msgHandlerMap_.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    msgHandlerMap_.insert({LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});
    msgHandlerMap_.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    msgHandlerMap_.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    msgHandlerMap_.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});

    // 群组业务管理相关事件处理回调注册
    msgHandlerMap_.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    msgHandlerMap_.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    msgHandlerMap_.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

    // 连接redis服务器
    if (redis_.connect())
    {
        // 设置上报消息的回调
        redis_.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

// 发送消息
void ChatService::sendPackage(const TcpConnectionPtr &conn, json &js)
{
    std::string strPackageData;
    MsgHeader header;
    header.bodySize = js.dump().size();
    // 插入包头
    strPackageData.append((const char*)&header, sizeof(header));
    strPackageData.append(js.dump());

    conn->send(strPackageData);
}

// 服务器异常，业务重置方法
void ChatService::reset()
{
    // 把online状态的用户，设置成offline
    userModel_.resetState();
}

// 获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    // 记录错误日志，msgid没有对应的事件处理回调
    auto it = msgHandlerMap_.find(msgid);
    if (it == msgHandlerMap_.end())
    {
        // 返回一个默认的处理器，空操作
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp) {
            LOG_ERROR("msgid:%d can not find handler!", msgid);
        };
    }
    else
    {
        return msgHandlerMap_[msgid];
    }
}

// 处理登录业务  id  pwd   pwd
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int id = js["id"].get<int>();
    string pwd = js["password"];

    User user = userModel_.query(id);
    if (user.getId() == id && user.getPwd() == pwd)
    {
        if (user.getState() == "online")
        {
            // 该用户已经登录，不允许重复登录
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = ERROR_LOGIN_2;
            sendPackage(conn,response);
        }
        else
        {
            // 登录成功，记录用户连接信息
            {
                lock_guard<mutex> lock(connMutex_);
                userConnMap_.insert({id, conn});
            }

            // id用户登录成功后，向redis订阅channel(id)
            redis_.subscribe(id);

            // 登录成功，更新用户状态信息 state offline=>online
            user.setState("online");
            userModel_.updateState(user);

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = ERROR_OK;
            response["id"] = user.getId();
            response["name"] = user.getName();
            // 查询该用户是否有离线消息
            vector<string> vec = offlineMsgModel_.query(id);
            if (!vec.empty())
            {
                response["offlinemsg"] = vec;
                // 读取该用户的离线消息后，把该用户的所有离线消息删除掉
                offlineMsgModel_.remove(id);
            }

            // 查询该用户的好友信息并返回
            vector<User> userVec = friendModel_.query(id);
            if (!userVec.empty())
            {
                vector<string> vec2;
                for (User &user : userVec)
                {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }

            // 查询用户的群组信息
            vector<Group> groupuserVec = groupModel_.queryGroups(id);
            if (!groupuserVec.empty())
            {
                // group:[{groupid:[xxx, xxx, xxx, xxx]}]
                vector<string> groupV;
                for (Group &group : groupuserVec)
                {
                    json grpjson;
                    grpjson["id"] = group.getId();
                    grpjson["groupname"] = group.getName();
                    grpjson["groupdesc"] = group.getDesc();
                    vector<string> userV;
                    for (GroupUser &user : group.getUsers())
                    {
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

            sendPackage(conn, response);
        }
    }
    else
    {
        // 该用户不存在，用户存在但是密码错误，登录失败
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = ERROR_LOGIN_1;
        sendPackage(conn, response);
    }
}

// 处理注册业务  name  password
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool state = userModel_.insert(user);
    if (state)
    {
        // 注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = ERROR_OK;
        response["id"] = user.getId();
        response["name"] = name;
        sendPackage(conn, response);
    }
    else
    {
        // 注册失败
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = ERROR_REG_1;
        sendPackage(conn, response); 
    }
}

// 处理注销业务
void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();

    {
        lock_guard<mutex> lock(connMutex_);
        auto it = userConnMap_.find(userid);
        if (it != userConnMap_.end())
        {
            userConnMap_.erase(it);
        }
    }

    // 用户注销，相当于下线，在redis中取消订阅通道
    redis_.unsubscribe(userid);

    json response;
    response["msgid"] = LOGINOUT_MSG_ACk;
    // 更新用户的状态信息
    User user(userid, "", "", "offline");
    if (userModel_.updateState(user))
    {
        response["errno"] = ERROR_OK;
    }
    else
    {
        response["errno"] = -1;
    }
    sendPackage(conn, response);
    conn->shutdown();
}

// 处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    {
        lock_guard<mutex> lock(connMutex_);
        for (auto it = userConnMap_.begin(); it != userConnMap_.end(); ++it)
        {
            if (it->second == conn)
            {
                // 从map表删除用户的链接信息
                user.setId(it->first);
                userConnMap_.erase(it);
                break;
            }
        }
    }

     // 用户下线，在redis中取消订阅通道
    redis_.unsubscribe(user.getId());

    // 更新用户的状态信息
    if (user.getId() != -1)
    {
        user.setState("offline");
        userModel_.updateState(user);
    }
}

// 一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int toid = js["toid"].get<int>();
    {
        lock_guard<mutex> lock(connMutex_);
        auto it = userConnMap_.find(toid);
        if (it != userConnMap_.end())
        {
            // toid在线，转发消息   服务器主动推送消息给toid用户
            sendPackage(it->second, js);
            return;
        }
    }

    // 当在该服务器中未找到该好友，可能在其他服务器上 查询toid是否在线
    User user = userModel_.query(toid);
    if (user.getState() == "online")
    {
        // 若该用户在其他服务器上，订阅消息
        redis_.publish(toid, js.dump());
        return;
    }

    // toid不在线，存储离线消息
    offlineMsgModel_.insert(toid, js.dump());
}

// 添加好友业务 msgid id friendid
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    std::string username = js["name"];
    int friendid = js["friendid"].get<int>();
    // 存储好友信息
    if (friendModel_.insert(userid, friendid) &&
        friendModel_.insert(friendid, userid))
    {
        {
            lock_guard<mutex> lock(connMutex_);
            auto it = userConnMap_.find(friendid);
            if (it != userConnMap_.end())
            {
                // 好友在线，转发消息   服务器主动推送消息给好友
                json response;
                response["msgid"] = ADD_FRIEND_MSG_ACK;
                response["id"] = userid;
                response["name"] = username;
                response["state"] = "online";
                response["errno"] = ERROR_OK;

                sendPackage(it->second, response);
                return;
            }
        }
    }
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    // 存储新创建的群组信息
    Group group(-1, name, desc);

    // 创建还回创建群组的消息的json
    json response;
    if (groupModel_.createGroup(group)) //创建成功
    {
        // 存储群组创建人信息
        groupModel_.addGroup(userid, group.getId(), "creator");
        response["errno"] = ERROR_OK;
    }
    else
    {
        //创建失败
        response["errno"] = -1;
    }

    response["msgid"] = CREATE_GROUP_MSG_ACK;
    response["groupid"] = group.getId();
    response["groupname"] = name;
    response["groupdesc"] = desc;
    sendPackage(conn,response);
}

// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();

    // 创建返回加入群组的消息的json
    json response;
    response["msgid"] = ADD_GROUP_MSG_ACK;
    response["errno"] = -1;

    // 先判断群组是否存在，在插入数据
    if (groupModel_.isGroups(groupid))
    {
        if (groupModel_.addGroup(userid, groupid, "normal"))
        {
            response["errno"] = ERROR_OK;
        }
        
    }
    sendPackage(conn, response);
}

// 群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = groupModel_.queryGroupUsers(userid, groupid);

    lock_guard<mutex> lock(connMutex_);
    for (int id : useridVec)
    {
        auto it = userConnMap_.find(id);
        if (it != userConnMap_.end())
        {
            // 转发群消息
            sendPackage(it->second, js);
        }
        else
        {
            // 当在该服务器中未找到该好友，可能在其他服务器上 查询toid是否在线
            User user = userModel_.query(id);
            if (user.getState() == "online")
            {
                // 若该用户在其他服务器上，订阅消息
                redis_.publish(id, js.dump());
            }
            // 存储离线群消息
            offlineMsgModel_.insert(id, js.dump());
        }
    }
}

// 从redis消息队列中获取订阅的消息
void ChatService::handleRedisSubscribeMessage(int userid, string msg)
{
    lock_guard<mutex> lock(connMutex_);
    auto it = userConnMap_.find(userid);
    if (it != userConnMap_.end())
    {
        json js = json::parse(msg);
        sendPackage(it->second, js);
        return;
    }

    // 存储该用户的离线消息
    offlineMsgModel_.insert(userid,msg);
}