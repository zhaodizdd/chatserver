#include "EPollpoller.h"
#include "Logger.h"
#include "Channel.h"

#include <error.h>
#include <unistd.h>
#include <string.h>

// channel未添加搭配poller中
const int kNew = -1; //channel的成员index_ = -1
// channel已添加到poller中
const int kAdded = 1;
// channel已从poller中删除
const int kDeleted = 2;

EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop)
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC))
    , events_(kInitEventListSize) // vector<epoll_event>
{
    if (epollfd_ < 0)
    {
        LOG_FATAL("epoll_create error:%d \n", errno);
    }
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

 // epoll_wait()
Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels) 
{
    // 实际上用LOG_DEBUG输出日志更合理
    LOG_DEBUG("func=%s => fd total count:%lu \n", __FUNCTION__, channels_.size());

    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()),timeoutMs);
    int saveErrno = errno;
    Timestamp now(Timestamp::now());

    if (numEvents > 0)
    {
        LOG_DEBUG("%d events happened \n", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if (numEvents ==events_.size())
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if (numEvents == 0) // 超时
    {
        LOG_DEBUG("%s timeout! \n", __FUNCTION__);
    }
    else // 小于0 发生错误
    {
        if (saveErrno != EINTR) // EINTR 响应外部中断
        {
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() err!");
        }
    }
    return now;
}

/**
 *          Eventloop
 *    ChannelList       Poller
 *                     ChannelMap <fd, channel*>
 */
void EPollPoller::updateChannel(Channel *channel) 
{
    const int index = channel->index();
    LOG_DEBUG("func=%s => fd=%d events=%d index=%d \n", __FUNCTION__, channel->fd(), channel->events(), index);

    if (index == kNew || index == kDeleted)
    {
       if (index == kNew)
       {
            int fd = channel->fd();
            channels_[fd] = channel;
       }
       
       channel->set_index(kAdded);
       update(EPOLL_CTL_ADD, channel);
    }
    else    // channel已经在poller上注册过了
    {
        int fd = channel->fd();
        if (channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

// 从poller中删除channelv
void EPollPoller::removeChannel(Channel *channel) 
{
    int fd = channel->fd();
    channels_.erase(fd);

    LOG_DEBUG("func=%s => fd=%d \n", __FUNCTION__, fd);


    int index = channel->index();
    if (index == kAdded)
    {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

// 填写活跃的连接
void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    for (int i = 0; i < numEvents; ++i)
    {
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel); // EventLoop就拿到了它的Poller给它返回的所以发生事件的channel列表了
    }
}

// 更新channel通道 epoll_ctl add/mod/del 
void EPollPoller::update(int operation, Channel *channel)
{
    epoll_event event;
    memset(&event, 0,sizeof event); //清零

    int fd = channel->fd();

    event.events = channel->events();
    event.data.fd = fd;
    event.data.ptr = channel;


    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error:%d\n", errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
        }
    }
    
}