#include <atomic>      
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <future>
#include <fstream>
#include <iostream> 
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "Thread_Pool.hpp"
#include "Synced_Stream.hpp"

using std::cout;
using std::endl;

#define _WIN32
#define _linux_

// Structure necessary for statistical calculations.
struct counter {
    int howManyDirectories;
    int howManyFiles;
    int emptyLines;
    int nonEmptyLines;
    int numWords;
    int letters;
};

// Creating objects.
threadPools pool(std::thread::hardware_concurrency());
syncedStream sync_out;
counter count;

// Function to calculate required counters such as number of words or letters
void countStats(std::string path)
{
    std::ifstream inFile;
    inFile.open(path);
    if (inFile)
    {
        std::string line;

        while (getline(inFile, line))
        {
            if (line.empty())
            {
                count.emptyLines++;
            }
            if (line.size() > 0)
            {
                count.nonEmptyLines++;
            }

            for (const auto& elem : line)
            {
                if ((elem >= 65 && elem <= 90) || (elem >= 97 && elem <= 122))
                {
                    count.letters++;
                }
            }
            std::stringstream lineStream(line);
            while (getline(lineStream, line, ' '))
            {
                count.numWords++;
            }

        }

        inFile.close();
    }
    else
    {
        cout << "Wrong path given" << endl;
    }
}

// Function to create task in each directory entry on the path specified by the user
void listFilesWithThreads(std::string path)
{
    try
    {
        for (auto& dirEntry : std::filesystem::directory_iterator(path))
        {
            if (!dirEntry.is_regular_file())
            {
                sync_out.println("Directory: ", dirEntry.path());
                count.howManyDirectories++;

                std::string DirectoryName{ dirEntry.path().filename().string() };
#ifdef _WIN32
                pool.push_task(listFilesWithThreads, path + "/" + DirectoryName);
                continue;
#endif
#ifdef _linux_
                char sign = 92;
                pool.push_task(listFilesWithThreads, path + sign + DirectoryName);
                continue;
#endif
            }
            if (dirEntry.is_regular_file())
            {
                std::filesystem::path file = dirEntry.path();
                sync_out.println("Filename: ", file.filename(), " extension: ", file.extension());
                count.howManyFiles++;
                countStats(dirEntry.path().string());
            }
        }
    }
    catch (...)
    {
        cout << "Don't type path to system directories." << endl;
    }

}

// The statistics are divided by the total number of all threads.
// This is necessary for the correct calculations of the runtime execution.
void summary(std::vector<double> elapsedWithThreads, int maxThreads)
{
    cout << endl << endl << "|| SUMMARY ||" << endl << endl;
    cout << "Numbers of directories:     " << count.howManyDirectories / maxThreads << endl;
    cout << "Numbers of Files:           " << count.howManyFiles / maxThreads << endl;
    cout << "Numbers of non-empty Lines: " << count.nonEmptyLines / maxThreads << endl;
    cout << "Numbers of Empty Lines:     " << count.emptyLines / maxThreads << endl;
    cout << "Number of Words:            " << count.numWords / maxThreads << endl;
    cout << "Numbers of Letters:         " << count.letters / maxThreads << endl;

    cout << endl << endl << "|| BENCHMARK ||" << endl << endl;
    for (int i = 0; i < maxThreads; i++)
    {
        if (i >= 9) cout << "Elapsed time listing with using " << i + 1 << " threads: " << std::setw(14) << std::fixed << elapsedWithThreads[i] << std::endl;
        else if (i > 0) cout << "Elapsed time listing with using " << i + 1 << " threads: " << std::setw(15) << std::fixed << elapsedWithThreads[i] << std::endl;
        else cout << "Elapsed time listing with using " << i + 1 << " thread: " << std::setw(16) << std::fixed << elapsedWithThreads[i] << std::endl;
    }
}

int main()
{
    int maxThreads = std::thread::hardware_concurrency();
    std::string path;
    std::vector<double> elapsedWithThreads;

    cout << "|| ANALIZE SPECIFIED DIRECTORY ||" << endl << endl;
    cout << "Don't type path to system directories." << endl;
    cout << "Enter the path to be analyzed:" << endl;
    std::cin >> path;

    while (!std::filesystem::exists(path))
    {
        cout << endl << "The path is incorrect! Try again:" << endl;
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