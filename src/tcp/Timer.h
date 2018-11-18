#pragma once

#include <cnet/utils/Date.h>
#include <cnet/utils/NonCopyable.h>
#include <cnet/tcp/callbacks.h>
#include <functional>
#include <atomic>
#include <iostream>
namespace cnet
{
typedef uint64_t TimerId;
class Timer : public NonCopyable
{
  public:
    Timer(const TimerCallback &cb, const Date &when, double interval);
    Timer(TimerCallback &&cb, const Date &when, double interval);
    ~Timer()
    {
        //   std::cout<<"Timer unconstract!"<<std::endl;
    }
    void run() const;
    void restart(const Date &now);
    bool operator<(const Timer &t) const;
    bool operator>(const Timer &t) const;
    const Date &when() const { return _when; }
    bool isRepeat() { return _repeat; }
    TimerId id() { return _id; }

  private:
    TimerCallback _callback;
    Date _when;
    const double _interval;
    const bool _repeat;
    const TimerId _id;
    static std::atomic<TimerId> _timersCreated;
};
}; // namespace cnet
