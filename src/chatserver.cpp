#include "chatserver.h"
#include "../json/json.hpp"
#include "chatservice.h"
#include "ConnectionPool.h"
#include "msg.h"
#include "net/Logger.h"

#include <iostream>
#include <functional>
#include <string>

//包最大字节数限制为10M
#define MAX_PACKAGE_SIZE    10 * 1024 * 1024

using namespace std::placeholders;
using json = nlohmann::json;

// 初始化聊天服务器对象
ChatServer::ChatServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       const std::string &nameArg)
    : _server(loop, listenAddr, nameArg), _loop(loop)
{
    // 注册连接回调
    _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));

    // 注册消息回调
    _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));

    // 设置线程数量
    _server.setThreadNum(4);
}

// 启动服务
void ChatServer::start()
{
    // 开启连接池
    ConnectionPool::getConnectionPool();
    _server.start();
}

// 上报链接相关信息的回调函数
void ChatServer::onConnection(const TcpConnectionPtr &conn)
{
    // 客户端断开链接
    if (!conn->connected())
    {
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }
}

// 上报读写事件相关信息的回调函数
void ChatServer::onMessage(const TcpConnectionPtr &conn,
                           Buffer *buffer,
                           Timestamp time)
{

    // 不够一个包头大小
    if (buffer->readableBytes() < (size_t)sizeof(MsgHeader))
    {
        //LOG_ERROR("bugou");
        return;
    }
    // 读取包头信息
    MsgHeader header;
    memcpy(&header,buffer->peek(),sizeof(MsgHeader));

    // 包头有错误，立即关闭连接
    if (header.bodySize <= 0 || header.bodySize > MAX_PACKAGE_SIZE)
    {
        //客户端发非法数据包，服务器主动关闭之
        //LOG_ERROR("Illegal package, size:%d, close TcpConnection, client:%s",
        //        header.bodySize,conn->localAddress().toIpPort().c_str());
        conn->forceClose();
    }    
    // 收到的数据不够一个完整的包
    if (buffer->readableBytes() < (size_t)header.bodySize + (size_t)sizeof(MsgHeader))
    {
        return;
    }
    // 先读取消息的头部
    buffer->retrieve(sizeof(MsgHeader));
    // 在读取信息的主体
    string buf = buffer->retrieveAllAsString();

    // 最后对读取的数据进行反序列化
    json js = json::parse(buf);

    // 达到的目的：完全解耦网络模块的代码和业务模块的代码
    // 通过js["msgid"] 获取=》业务handler=》conn  js  time
    auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
    // 回调消息绑定好的事件处理器，来执行相应的业务处理
    msgHandler(conn, js, time);
}