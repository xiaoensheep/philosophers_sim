#include "philosopher.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

Philosopher::Philosopher(int id, int num_philosophers, PhilosopherManager& manager)
    : id_(id),
      num_philosophers_(num_philosophers),
      manager_(manager),
      state_(PhilosopherState::THINKING),
      running_(false),
      eat_count_(0),
      gen_(rd_()),
      think_dist_(1000, 5000),  // 思考 s
      eat_dist_(1000, 3000)     // 进餐 s
{
}

Philosopher::~Philosopher()
{
    stop();  // 确保线程停止
}

void Philosopher::start()
{
    running_ = true;
    thread_ = std::thread(&Philosopher::run, this);  // 启动线程
}

void Philosopher::stop()
{
    running_ = false;  // 设置停止标志
    if (thread_.joinable()) {
        thread_.join();  // 等待线程结束
    }
}

PhilosopherState Philosopher::getState() const
{
    return state_.load(std::memory_order_acquire);  // 原子读取状态
}

int Philosopher::getEatCount() const
{
    return eat_count_.load(std::memory_order_acquire);  // 原子读取进餐次数
}

int Philosopher::getId() const
{
    return id_;
}

void Philosopher::eat()
{
    state_ = PhilosopherState::EATING;  // 设置进餐状态
    int eat_time = eat_dist_(gen_);     // 生成随机进餐时间
    std::this_thread::sleep_for(std::chrono::milliseconds(eat_time));  // 模拟进餐
    eat_count_.fetch_add(1, std::memory_order_release);                // 原子增加进餐计数
    state_ = PhilosopherState::THINKING;  // 进餐结束，回到思考等待下一轮
}

void Philosopher::run()
{
    while (running_.load(std::memory_order_acquire)) {
        think();  // 思考阶段

        if (!running_.load(std::memory_order_acquire))
            break;

        state_ = PhilosopherState::HUNGRY;

        auto guard = manager_.acquireChopsticks(id_);
        eat();
    }
}

void Philosopher::think()
{
    state_ = PhilosopherState::THINKING;  // 设置思考状态
    int think_time = think_dist_(gen_);   // 生成随机思考时间
    std::this_thread::sleep_for(std::chrono::milliseconds(think_time));  // 模拟思考
}

// PhilosopherManager 实现
PhilosopherManager::PhilosopherManager(int num_philosophers)
    : num_philosophers_(num_philosophers),
      chopstick_owner_(num_philosophers_)
{
    // 使用 reserve 预分配空间，避免重新分配
    chopsticks_.reserve(num_philosophers_);
    for (int i = 0; i < num_philosophers_; ++i) {
        chopsticks_.push_back(std::make_unique<std::mutex>());
    }

    sem_init(&waiter_, 0, num_philosophers_ - 1);  // 服务员算法，允许 n-1 个哲学家同时拿筷子

    // 创建哲学家对象
    philosophers_.reserve(num_philosophers_);
    for (int i = 0; i < num_philosophers_; ++i) {
        philosophers_.push_back(std::make_unique<Philosopher>(i, num_philosophers_, *this));
    }

    for (auto& owner : chopstick_owner_) {
        owner.store(-1, std::memory_order_relaxed);
    }
}

PhilosopherManager::~PhilosopherManager()
{
    stop();               // 停止所有哲学家
    sem_destroy(&waiter_);  // 销毁信号量
}

void PhilosopherManager::start()
{
    for (auto& philosopher : philosophers_) {
        philosopher->start();
    }

}

void PhilosopherManager::stop()
{
    for (auto& philosopher : philosophers_) {
        philosopher->stop();
    }

    for (auto& owner : chopstick_owner_) {
        owner.store(-1, std::memory_order_release);
    }
}

PhilosopherState PhilosopherManager::getPhilosopherState(int id) const
{
    if (id >= 0 && id < num_philosophers_) {
        return philosophers_[id]->getState();  // 返回指定哲学家的状态
    }
    return PhilosopherState::THINKING;  // 默认返回思考状态
}

int PhilosopherManager::getPhilosopherEatCount(int id) const
{
    if (id >= 0 && id < num_philosophers_) {
        return philosophers_[id]->getEatCount();  // 返回指定哲学家的进餐次数
    }
    return 0;  // 无效 ID 返回 0
}

int PhilosopherManager::getNumPhilosophers() const
{
    return num_philosophers_;  // 返回哲学家数量
}

int PhilosopherManager::getChopstickOwner(int idx) const
{
    if (idx >= 0 && idx < num_philosophers_) {
        return chopstick_owner_[idx].load(std::memory_order_acquire);
    }
    return -1;
}

PhilosopherManager::ChopstickGuard PhilosopherManager::acquireChopsticks(int id)
{
    int left = (id + num_philosophers_ - 1) % num_philosophers_;
    int right = id;

    sem_wait(&waiter_);

    std::unique_lock<std::mutex> left_lock(*chopsticks_[left], std::defer_lock);
    std::unique_lock<std::mutex> right_lock(*chopsticks_[right], std::defer_lock);
    std::lock(left_lock, right_lock);

    chopstick_owner_[left].store(id, std::memory_order_release);
    chopstick_owner_[right].store(id, std::memory_order_release);

    return ChopstickGuard(std::move(left_lock), std::move(right_lock), this, id, left, right);
}

void PhilosopherManager::releaseChopsticksInternal(int owner, int left, int right)
{
    chopstick_owner_[left].store(-1, std::memory_order_release);
    chopstick_owner_[right].store(-1, std::memory_order_release);
    sem_post(&waiter_);
}