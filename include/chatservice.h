#pragma once

#include "TcpConnection.h"
#include "groupmodel.h"
#include "friendmodel.h"
#include "usermodel.h"
#include "offlinemessagemodel.h"
#include "json.hpp"
#include "redis.hpp"

#include <unordered_map>
#include <functional>
#include <mutex>

using json = nlohmann::json;

// 表示处理消息的事件回调方法类型
using MsgHandler = std::function<void(const TcpConnectionPtr &conn, json &js, Timestamp time)>;

// 聊天服务器业务类
class ChatService
{
public:
    // 获取单例对象的接口函数
    static ChatService *instance();

    // 发送消息
    void sendPackage(const TcpConnectionPtr &conn, json &js);

    // 处理登录业务
    void login(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 处理注册业务
    void reg(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 一对一聊天业务
    void oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 添加好友业务
    void addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 创建群组业务
    void createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 加入群组业务
    void addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 群组聊天业务
    void groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 处理注销业务
    void loginout(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr &conn);
    // 服务器异常，业务重置方法
    void reset();
    // 获取消息对应的处理器
    MsgHandler getHandler(int msgid);
    // 从redis消息队列中获取订阅的消息
    void handleRedisSubscribeMessage(int userid, string msg);
private:
    ChatService();

    // 存储消息id和其对应的业务处理方法
    std::unordered_map<int, MsgHandler> msgHandlerMap_;
    // 存储在线用户的通信连接
    std::unordered_map<int, TcpConnectionPtr> userConnMap_;
    // 定义互斥锁，保证_userConnMap的线程安全
    std::mutex connMutex_;

    // 数据操作类对象
    UserModel userModel_;               // 用户信息表操作
    OfflineMsgModel offlineMsgModel_;   // 离线消息表操作
    FriendModel friendModel_;           // 用户好友表操作
    GroupModel groupModel_;             // 用户群组表操作

    // redis 操作对象
    Redis redis_;
};
