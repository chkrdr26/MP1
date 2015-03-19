//
//  MTQueue.h
//  CentralServer
//
//  a thread-safe queue data struture which is based on
//  queue data structure from C++ standard template library
//  and mutex, this queue is for simulating the FIFO feature
//  of communication channels between each pairs

#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class MTQueue
{
public:    
    T& pop()
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        while (queue_.empty())                             //critical section
        {
            cond_.wait(mlock);
        }
        auto val = queue_.front();
        queue_.pop();
        return val;
    }
    
    void pop(T& item)
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        while (queue_.empty())                             //critical section
        {
            cond_.wait(mlock);
        }
        item = queue_.front();
        queue_.pop();
    }
    
    void push(const T& item)
    {
        std::unique_lock<std::mutex> mlock(mutex_);        //critical section
        queue_.push(item);
        mlock.unlock();
        cond_.notify_one();
    }
    
    T& front()
    {
        return queue_.front();
    }
    
    T& back()
    {
        return queue_.back();
    }
    
    bool empty()
    {
        return queue_.empty() ? true : false;
    }
    MTQueue()=default;
    MTQueue(const MTQueue&) = delete;                      //disable copying
    MTQueue& operator=(const MTQueue&) = delete;           //disable assignment
    
private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;
};
