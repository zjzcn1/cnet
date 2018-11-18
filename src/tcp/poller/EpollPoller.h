#pragma once
#include "tcp/Poller.h"
#include <cnet/utils/NonCopyable.h>
#include <cnet/tcp/EventLoop.h>

#include <memory>
#include <map>
typedef std::vector<struct epoll_event> EventList;

namespace cnet
{
class Channel;

class EpollPoller : public Poller
{
  public:
	explicit EpollPoller(EventLoop *loop);
	virtual ~EpollPoller();
	virtual void poll(int timeoutMs, ChannelList *activeChannels) override;
	virtual void updateChannel(Channel *channel) override;
	virtual void removeChannel(Channel *channel) override;

  private:
	static const int kInitEventListSize = 16;
	int epollfd_;
	EventList events_;
	void update(int operation, Channel *channel);
	typedef std::map<int, Channel *> ChannelMap;
	ChannelMap channels_;
	void fillActiveChannels(int numEvents,
							ChannelList *activeChannels) const;
};
} // namespace cnet
