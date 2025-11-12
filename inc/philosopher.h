#ifndef PHILOSOPHER_H
#define PHILOSOPHER_H

#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <random>
#include <semaphore.h>
#include <memory>
#include <utility>
#include <chrono>

class PhilosopherManager;

// 哲学家状态枚举
enum class PhilosopherState {
    THINKING,  // 思考状态
    HUNGRY,    // 饥饿状态  
    EATING     // 进餐状态
};

// 哲学家类
class Philosopher {
public:
    Philosopher(int id, int num_philosophers, PhilosopherManager& manager);
    ~Philosopher();
    
    // 禁止拷贝和移动
    Philosopher(const Philosopher&) = delete;
    Philosopher& operator=(const Philosopher&) = delete;
    Philosopher(Philosopher&&) = delete;
    Philosopher& operator=(Philosopher&&) = delete;
    
    void start();  // 启动哲学家线程
    void stop();   // 停止哲学家线程
    PhilosopherState getState() const;  // 获取当前状态
    int getEatCount() const;           // 获取进餐次数
    int getId() const;                 // 获取哲学家ID
    void eat();                        // 进餐方法

private:
    void run();    // 线程主函数
    void think();  // 思考方法

    int id_;                          // 哲学家ID
    int num_philosophers_;            // 哲学家总数
    PhilosopherManager& manager_;     // 管理器引用
    std::thread thread_;              // 哲学家线程
    std::atomic<PhilosopherState> state_;  // 原子状态变量
    std::atomic<bool> running_;       // 运行标志
    std::atomic<int> eat_count_;      // 进餐次数计数
    
    // 随机数生成器
    std::random_device rd_;
    std::mt19937 gen_;
    std::uniform_int_distribution<int> think_dist_;  // 思考时间分布
    std::uniform_int_distribution<int> eat_dist_;    // 进餐时间分布
};

// 哲学家管理器类
class PhilosopherManager {
public:
    PhilosopherManager(int num_philosophers = 5);
    ~PhilosopherManager();
    
    // 禁止拷贝和移动
    PhilosopherManager(const PhilosopherManager&) = delete;
    PhilosopherManager& operator=(const PhilosopherManager&) = delete;
    PhilosopherManager(PhilosopherManager&&) = delete;
    PhilosopherManager& operator=(PhilosopherManager&&) = delete;
    
    void start();  // 启动所有哲学家
    void stop();   // 停止所有哲学家
    PhilosopherState getPhilosopherState(int id) const;  // 获取哲学家状态
    int getPhilosopherEatCount(int id) const;           // 获取进餐次数
    int getNumPhilosophers() const;                     // 获取哲学家数量
    int getChopstickOwner(int idx) const;               // 获取某根筷子的持有者

    struct ChopstickGuard;
    ChopstickGuard acquireChopsticks(int id);

private:
    std::vector<std::unique_ptr<Philosopher>> philosophers_;  // 使用智能指针
    std::vector<std::unique_ptr<std::mutex>> chopsticks_;     // 使用智能指针
    sem_t waiter_;                                            // 服务员信号量
    int num_philosophers_;                                    // 哲学家数量
    std::vector<std::atomic<int>> chopstick_owner_;           // 记录筷子持有者

    void releaseChopsticksInternal(int owner, int left, int right);
};

struct PhilosopherManager::ChopstickGuard {
    std::unique_lock<std::mutex> left_lock;
    std::unique_lock<std::mutex> right_lock;
    PhilosopherManager* manager;
    int owner;
    int left_idx;
    int right_idx;

    ChopstickGuard(std::unique_lock<std::mutex>&& left,
                   std::unique_lock<std::mutex>&& right,
                   PhilosopherManager* mgr,
                   int owner_id,
                   int left_index,
                   int right_index)
        : left_lock(std::move(left)),
          right_lock(std::move(right)),
          manager(mgr),
          owner(owner_id),
          left_idx(left_index),
          right_idx(right_index)
    {}

    ChopstickGuard(ChopstickGuard&& other) noexcept
        : left_lock(std::move(other.left_lock)),
          right_lock(std::move(other.right_lock)),
          manager(other.manager),
          owner(other.owner),
          left_idx(other.left_idx),
          right_idx(other.right_idx)
    {
        other.manager = nullptr;
    }

    ChopstickGuard& operator=(ChopstickGuard&& other) noexcept
    {
        if (this != &other) {
            release();
            left_lock = std::move(other.left_lock);
            right_lock = std::move(other.right_lock);
            manager = other.manager;
            owner = other.owner;
            left_idx = other.left_idx;
            right_idx = other.right_idx;
            other.manager = nullptr;
        }
        return *this;
    }

    ~ChopstickGuard() { release(); }

    void release()
    {
        if (manager) {
            manager->releaseChopsticksInternal(owner, left_idx, right_idx);
            manager = nullptr;
        }
    }

    ChopstickGuard(const ChopstickGuard&) = delete;
    ChopstickGuard& operator=(const ChopstickGuard&) = delete;
};

#endif // PHILOSOPHER_H