/*
Create the application that will analyze and show statistics of specified directory that will handle recursive directories.
It should print number of files and total number of lines (might as well count non-empty and empty lines).
Provide unit tests for solutions.
- Application should use multithreading(keep in mind system limitations, you might need to control number of concurrent calls, e.g., use thread pool).
- Use std::filesystem
- Use GTest for Unit Tests
- As a bonus, you could also count words and letters and provide performance benchmarks(e.g., measure the impact of using the different number of threads).
*/

#include <atomic>      
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <future>
#include <iostream> 
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

// Several global variables necessary for statistical calculations.
unsigned int howManyDirectories = 0, howManyFiles = 0, howManyWords = 0, howManyLetters = 0;
int isEmptyLine = 0, nonEmptyLine = 0;

class threadPool
{
    typedef std::uint_fast32_t ui32;
    typedef std::uint_fast64_t ui64;

public:
    // ============================
    // Constructors and destructors
    // ============================

    threadPool(const ui32& _thread_count = std::thread::hardware_concurrency())
        : thread_count(_thread_count ? _thread_count : std::thread::hardware_concurrency()), threads(new std::thread[_thread_count ? _thread_count : std::thread::hardware_concurrency()])
    {
        create_threads();
    }

    ~threadPool()
    {
        wait_for_tasks();
        running = false;
        destroy_threads();
    }

    // =======================
    // Public member functions
    // =======================
    ui64 get_tasks_queued() const
    {
        const std::scoped_lock lock(queue_mutex);
        return tasks.size();
    }

    ui32 get_tasks_running() const
    {
        return tasks_total - (ui32)get_tasks_queued();
    }

    ui32 get_tasks_total() const
    {
        return tasks_total;
    }

    template <typename F>
    void push_task(const F& task)
    {
        tasks_total++;
        {
            const std::scoped_lock lock(queue_mutex);
            tasks.push(std::function<void()>(task));
        }
    }

    template <typename F, typename... A>
    void push_task(const F& task, const A &...args)
    {
        push_task([task, args...]
            { task(args...); });
    }

    void reset(const ui32& _thread_count = std::thread::hardware_concurrency())
    {
        bool was_paused = paused;
        paused = true;
        wait_for_tasks();
        running = false;
        destroy_threads();
        thread_count = _thread_count ? _thread_count : std::thread::hardware_concurrency();
        threads.reset(new std::thread[thread_count]);
        paused = was_paused;
        running = true;
        create_threads();
    }

    void wait_for_tasks()
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
                if (get_tasks_running() == 0)
                    break;
            }
            sleep_or_yield();
        }
    }

    // ===========
    // Public data
    // ===========

    std::atomic<bool> paused = false;
    ui32 sleep_duration = 1000;

private:
    // ========================
    // Private member functions
    // ========================

    void create_threads()
    {
        for (ui32 i = 0; i < thread_count; i++)
        {
            threads[i] = std::thread(&threadPool::worker, this);
        }
    }

    void destroy_threads()
    {
        for (ui32 i = 0; i < thread_count; i++)
        {
            threads[i].join();
        }
    }

    bool pop_task(std::function<void()>& task)
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

    void sleep_or_yield()
    {
        if (sleep_duration)
            std::this_thread::sleep_for(std::chrono::microseconds(sleep_duration));
        else
            std::this_thread::yield();
    }

    void worker()
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

    // ============
    // Private data
    // ============

    mutable std::mutex queue_mutex = {};
    std::atomic<bool> running = true;
    std::queue<std::function<void()>> tasks = {};
    ui32 thread_count;
    std::unique_ptr<std::thread[]> threads;
    std::atomic<ui32> tasks_total = 0;
};

class syncedStream
{
public:
    // =======================
    // Public member functions
    // =======================

    syncedStream(std::ostream& _out_stream = std::cout)
        : out_stream(_out_stream) {};

    template <typename... T>
    void print(const T &...items)
    {
        const std::scoped_lock lock(stream_mutex);
        (out_stream << ... << items);
    }

    template <typename... T>
    void println(const T &...items)
    {
        print(items..., '\n');
    }

private:
    // ============
    // Private data
    // ============

    mutable std::mutex stream_mutex = {};
    std::ostream& out_stream;
};

// Creating objects.
syncedStream sync_out;
threadPool pool(std::thread::hardware_concurrency());

// We list the catalog. Each subdirectory is treated as new work for the threading pool.
void listFilesWithThreads(std::string path)
{
    for (auto& dirEntry : std::filesystem::directory_iterator(path))
    {
        if (!dirEntry.is_regular_file())
        {
            sync_out.println("Directory: ", dirEntry.path());
            howManyDirectories++;
            if (std::filesystem::is_empty(dirEntry))
                isEmptyLine++;
            else
            {
                nonEmptyLine++;
                std::string DirectoryName{ dirEntry.path().filename().string() };
                pool.push_task(listFilesWithThreads, path + "/" + DirectoryName);
                continue;
            }
        }
        howManyFiles++;
        std::filesystem::path file = dirEntry.path();
        std::string fileName{ file.filename().string() };
        howManyLetters += fileName.size();
        howManyWords++;
        for (unsigned int i = 0; i < fileName.size(); i++)
        {
            if (fileName[i] == ' ')
                howManyWords++;
        }
        sync_out.println("Filename: ", file.filename(), " extension: ", file.extension());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// The statistics are divided by the total number of all threads.
// This is necessary for the correct calculations of the runtime execution.
void summary(std::vector<double> elapsedWithThreads, int maxThreads)
{
    std::cout << std::endl << std::endl << "|| SUMMARY ||" << std::endl << std::endl;
    std::cout << "Numbers of directories: " << howManyDirectories / maxThreads << std::endl;
    std::cout << "Numbers of files: " << howManyFiles / maxThreads << std::endl;
    std::cout << "Numbers of non-empty lines: " << nonEmptyLine / maxThreads << std::endl;
    std::cout << "Numbers of empty lines: " << isEmptyLine / maxThreads << std::endl << std::endl;
    std::cout << "Letters in names of files: " << howManyLetters / maxThreads << std::endl;
    std::cout << "Words in names of files: " << howManyWords / maxThreads << std::endl << std::endl;

    for (int i = 0; i < maxThreads; i++)
    {
        if (i >= 9) std::cout << "Elapsed time listing with using " << i + 1 << " threads: " << std::setw(14) << std::fixed << elapsedWithThreads[i] << std::endl;
        else if (i > 0) std::cout << "Elapsed time listing with using " << i + 1 << " threads: " << std::setw(15) << std::fixed << elapsedWithThreads[i] << std::endl;
        else std::cout << "Elapsed time listing with using " << i + 1 << " thread: " << std::setw(16) << std::fixed << elapsedWithThreads[i] << std::endl;
    }
}

int main()
{
    // Maximum number of threads handled by the processor.
    int maxThreads = std::thread::hardware_concurrency();
    std::string path;
    std::vector<double> elapsedWithThreads;

    std::cout << "|| ANALIZE SPECIFIED DIRECTORY ||" << std::endl << std::endl;
    std::cout << "Don't try to analyze local drives or system folders!" << std::endl;
    std::cout << "Enter the path to be analyzed:" << std::endl;
    std::cin >> path;

    while (!std::filesystem::exists(path))
    {
        std::cout << std::endl << "The path is incorrect! Try again:" << std::endl;
        std::cin >> path;
    }

    for (int howManyThreads = 1; howManyThreads <= maxThreads; howManyThreads++)
    {
        pool.reset(howManyThreads);
        auto begin = std::chrono::high_resolution_clock::now();
        pool.push_task(listFilesWithThreads, path);
        pool.wait_for_tasks();
        auto end = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count() * 1e-9;
        elapsedWithThreads.push_back(elapsed);
    }
    summary(elapsedWithThreads, maxThreads);
    system("pause");
    return 0;
}