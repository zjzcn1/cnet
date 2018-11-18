#pragma once
#include "cnet/utils/NonCopyable.h"
#include "cnet/tcp/EventLoop.h"

#include <memory>
#include <map>

namespace cnet
{
class Channel;

class Poller : NonCopyable
{
  public:
	explicit Poller(EventLoop *loop) : ownerLoop_(loop){};
	virtual ~Poller(){};
	void assertInLoopThread() { ownerLoop_->assertInLoopThread(); }
	virtual void poll(int timeoutMs, ChannelList *activeChannels) = 0;
	virtual void updateChannel(Channel *channel) = 0;
	virtual void removeChannel(Channel *channel) = 0;
	static Poller *newPoller(EventLoop *loop);

  private:
	EventLoop *ownerLoop_;
};
} // namespace cnet
