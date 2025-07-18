#pragma once

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

// enqueue([](bool* S) {}, & (bool));



class ThreadPool {
public:
    ThreadPool();//默认直接全线程
    ThreadPool(size_t);
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>;
    ~ThreadPool();

    bool TasksEnd() {
        return tasks.size() != 0;
    }
private:
    // need to keep track of threads so we can join them
    //需要跟踪线程，这样我们才能加入它们
    std::vector< std::thread > workers;// 线程队列
    // the task queue
    //任务队列
    std::queue< std::function<void()> > tasks;

    // synchronization
    //同步
    std::mutex queue_mutex;// 互斥锁
    std::condition_variable condition;// 条件变量.
    bool stop;// 线程池销毁的开关
};


// the constructor just launches some amount of workers
//构造函数只是启动了一些工人
inline ThreadPool::ThreadPool()
    : stop(false)
{
    int ThreadCount = std::thread::hardware_concurrency();
    for (size_t i = 0; i < ThreadCount; ++i)
        workers.emplace_back(
            [this]()
            {
                for (;;)
                {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);//上锁
                        this->condition.wait(lock,//阻塞当前线程直到条件变量被唤醒 （等待有任务）
                            [this] { return this->stop || !this->tasks.empty(); });//empty()：队列是否为空
                        if (this->stop && this->tasks.empty())//判断线程池被销毁，且当前任务完成
                            return;//线程结束
                        task = std::move(this->tasks.front());//front()：返回队头元素 （获取任务）
                        this->tasks.pop();//pop()：删掉下一个元素，也即队头元素
                    }

                    task();//执行任务
                }
            }
    );
}


inline ThreadPool::ThreadPool(size_t threads)
    : stop(false)
{
    for (size_t i = 0; i < threads; ++i)
        workers.emplace_back(
            [this]()
            {
                for (;;)
                {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);//上锁
                        this->condition.wait(lock,//阻塞当前线程直到条件变量被唤醒 （等待有任务）
                            [this] { return this->stop || !this->tasks.empty(); });//empty()：队列是否为空
                        if (this->stop && this->tasks.empty())//判断线程池被销毁，且当前任务完成
                            return;//线程结束
                        task = std::move(this->tasks.front());//front()：返回队头元素 （获取任务）
                        this->tasks.pop();//pop()：删掉下一个元素，也即队头元素
                    }

                    task();//执行任务
                }
            }
    );
}

// add new work item to the pool
//将新的工作项添加到池中
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
-> std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared< std::packaged_task<return_type()> >(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        //停止池后不允许排队
        if (stop)
            throw std::runtime_error("在已停止的线程池上排队！！！");

        tasks.emplace([task]() { (*task)(); });//emplace(C++11) 于尾部原位构造元素
    }
    condition.notify_one();//通知一个正在等待的线程
    return res;
}

// the destructor joins all threads
//析构函数连接所有线程
inline ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);//锁住所有任务
        stop = true;//通知线程关闭
    }
    condition.notify_all();//通知所有正在等待的线程
    for (std::thread& worker : workers)
        worker.join();//确定全部线程都结束了
}