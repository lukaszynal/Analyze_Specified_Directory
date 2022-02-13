#pragma once
#include <thread>
#include <mutex>
#include <queue>
#include <functional>

class threadPools
{
	bool paused = false;
	int sleep_duration = 1000;
	mutable std::mutex queue_mutex = {};
	bool running = true;
	std::queue<std::function<void()>> tasks = {};
	int thread_count;
	std::unique_ptr<std::thread[]> threads;
	int tasks_total = 0;

public:

	threadPools(int _thread_count);

	~threadPools();

	int get_tasks();


	template <typename F>
	void push_task(const F& task);

	template <typename F, typename... A>
	void push_task(const F& task, const A &...args);

	void reset(int thread_count);

	void wait_for_tasks();




private:

	void create_threads();

	void destroy_threads();

	bool pop_task(std::function<void()>& task);

	void sleep_or_yield();

	void worker();


};

threadPools::threadPools(int _threadPool)
{
	create_threads();
}

threadPools::~threadPools()
{
	wait_for_tasks();
	running = false;
	destroy_threads();
}

int threadPools::get_tasks()
{
	return tasks_total;
}

template <typename F>
void threadPools::push_task(const F& task)
{
	tasks_total++;
	{
		const std::scoped_lock lock(queue_mutex);
		tasks.push(std::function<void()>(task));
	}
}
template <typename F, typename... A>
void threadPools::push_task(const F& task, const A &...args)
{
	push_task([task, args...]
		{ task(args...); });
}

void threadPools::reset(int threadCount)
{
	bool was_paused = paused;
	paused = true;
	wait_for_tasks();
	running = false;
	destroy_threads();
	thread_count = std::thread::hardware_concurrency();
	threads.reset(new std::thread[thread_count]);
	paused = was_paused;
	running = true;
	create_threads();
}

void threadPools::wait_for_tasks()
{
	while (true)
	{
		if (!paused)
		{
			if (tasks_total == 0)
				break;
		}
		else
		{
			if (get_tasks() == 0)
				break;
		}
		sleep_or_yield();
	}
}

void threadPools::create_threads()
{
	for (int i = 0; i < thread_count; i++)
	{
		threads[i] = std::thread(&threadPools::worker, this);
	}
}

void threadPools::destroy_threads()
{
	for (int i = 0; i < thread_count; i++)
	{
		threads[i].join();
	}
}

bool threadPools::pop_task(std::function<void()>& task)
{
	const std::scoped_lock lock(queue_mutex);
	if (tasks.empty())
		return false;
	else
	{
		task = std::move(tasks.front());
		tasks.pop();
		return true;
	}
}

void threadPools::sleep_or_yield()
{
	if (sleep_duration)
		std::this_thread::sleep_for(std::chrono::microseconds(sleep_duration));
	else
		std::this_thread::yield();
}

void threadPools::worker()
{
	while (running)
	{
		std::function<void()> task;
		if (!paused && pop_task(task))
		{
			task();
			tasks_total--;
		}
		else
		{
			sleep_or_yield();
		}
	}
}