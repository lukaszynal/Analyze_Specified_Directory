#pragma once
#include <iostream>
#include <mutex>

using std::cout;

class syncedStream
{
public:

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

    mutable std::mutex stream_mutex = {};
    std::ostream& out_stream;
};
