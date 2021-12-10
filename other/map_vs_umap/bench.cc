#include <array> 
#include <chrono> 
#include <functional> 
#include <iostream> 
#include <map> 
#include <string> 
#include <unordered_map> 
 
static volatile int dummy; 
 
static void run_benchmark(std::function<void()> benchmark, std::string desc) { 
    const auto start = std::chrono::high_resolution_clock::now(); 
    benchmark(); 
    const auto end = std::chrono::high_resolution_clock::now(); 
 
    const auto elapsed = std::chrono::duration<double>(end - start); 
    std::cout << desc << ":  Elapsed time: " << elapsed.count() << " seconds\n"; 
} 
 
 
int main() { 
    auto map_1  = std::map<std::string, int>{ }; 
    auto map_2  = std::map<std::string, int>{ }; 
    auto umap_1 = std::unordered_map<std::string, int>{ }; 
    auto umap_2 = std::unordered_map<std::string, int>{ }; 
    auto str1   = std::array<std::string, 26>{ }; 
    auto str2   = std::array<std::string, 26>{ }; 
 
    for (int i = 0; i < 26; i++) { 
        auto s1 = std::string(999999, 'a') + std::string(1, 'a' + i); 
        auto s2 = std::string(1000000, 'a' + i); 
 
        str1[i] = s1; 
        str2[i] = s2; 
        map_1[s1] = i; 
        map_2[s2] = i; 
        umap_1[s1] = i; 
        umap_2[s2] = i; 
    } 
 
    run_benchmark([&map_1,&str1]() { 
            for (auto i = 0; i < 100; i++) { 
                dummy += map_1[str1[i % 26]]; 
            } 
        }, " map_1"); 
 
    run_benchmark([&umap_1,&str1]() { 
            for (auto i = 0; i < 100; i++) { 
                dummy += umap_1[str1[i % 26]]; 
            } 
        }, "umap_1"); 
 
    run_benchmark([&map_2,&str2]() { 
            for (auto i = 0; i < 100; i++) { 
                dummy += map_2[str2[i % 26]]; 
            } 
        }, " map_2"); 
 
    run_benchmark([&umap_2,&str2]() { 
            for (auto i = 0; i < 100; i++) { 
                dummy += umap_2[str2[i % 26]]; 
            } 
        }, "umap_2"); 
} 