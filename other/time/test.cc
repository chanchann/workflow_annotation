#include <chrono>
#include <iostream>
#include <stdio.h>

using namespace std::chrono;

int main()
{
    uint64_t ms_since_epoch = duration_cast<microseconds>(system_clock::now().time_since_epoch()).count();
    time_t sec = static_cast<time_t>(ms_since_epoch / 1000000);
    struct tm tm_time;
    char time_str_utc[64];
    ::gmtime_r(&sec, &tm_time);

    int len = snprintf(time_str_utc, sizeof time_str_utc, "%4d-%02d-%02d_%02d:%02d:%02d",
                        tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
                        tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
    std::cout << time_str_utc << " UTC" << std::endl;

    char time_str_local[64];
    ::localtime_r(&sec, &tm_time);
    int len2 = snprintf(time_str_local, sizeof time_str_local, "%4d-%02d-%02d_%02d:%02d:%02d",
                        tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
                        tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
    std::cout << time_str_local << " local" << std::endl;
    return 0;
}