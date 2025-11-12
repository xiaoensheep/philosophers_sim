#include "philosopher.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

Philosopher::Philosopher(int id, int num_philosophers)
    : id_(id),
      num_philosophers_(num_philosophers),
      state_(PhilosopherState::THINKING),
      running_(false),
      eat_count_(0),
      gen_(rd_()),
      think_dist_(1000, 5000),  // 思考 3-8s
      eat_dist_(1000, 3000)     // 进餐 2-5s
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

        // 短暂等待，让管理器有机会处理
        while (running_.load(std::memory_order_acquire) &&
               state_.load(std::memory_order_acquire) != PhilosopherState::THINKING) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
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
      running_(false),
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
        philosophers_.push_back(std::make_unique<Philosopher>(i, num_philosophers_));
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

    /* 原错误实现：局部线程 + detach，会在未检测到活动时立刻退出
    std::thread coordinator([this]() { ... });
    coordinator.detach();
    */

    running_.store(true, std::memory_order_release);
    coordinator_ = std::thread([this]() {
        while (running_.load(std::memory_order_acquire)) {
            bool hungry_present = false;

            for (int i = 0; i < num_philosophers_; ++i) {
                if (philosophers_[i]->getState() == PhilosopherState::HUNGRY) {
                    hungry_present = true;

                    int left = (i + num_philosophers_ - 1) % num_philosophers_;
                    int right = i;

                    // 服务员算法：占用许可后再尝试拿两支筷子
                    sem_wait(&waiter_);

                    std::unique_lock<std::mutex> left_lock(*chopsticks_[left], std::try_to_lock);
                    std::unique_lock<std::mutex> right_lock(*chopsticks_[right], std::try_to_lock);

                    if (left_lock.owns_lock() && right_lock.owns_lock()) {
                        chopstick_owner_[left].store(i, std::memory_order_release);
                        chopstick_owner_[right].store(i, std::memory_order_release);

                        philosophers_[i]->eat();

                        chopstick_owner_[left].store(-1, std::memory_order_release);
                        chopstick_owner_[right].store(-1, std::memory_order_release);
                    }

                    sem_post(&waiter_);
                }
            }

            if (!hungry_present) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    });
}

void PhilosopherManager::stop()
{
    running_.store(false, std::memory_order_release);

    if (coordinator_.joinable()) {
        coordinator_.join();
    }

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