//
// Created by etders on 27.04.2026.
//

#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include "FastBin.hpp"

#define COUNT 100000000

struct Foo {
    uint32_t id;
    double timestamp;
};

#pragma pack(push, 1)
struct MassiveData {
    uint64_t count;
    Foo items[COUNT];
};
#pragma pack(pop)

int main() {
    const char* filename = "data.bin";
    auto ms = [](auto start, auto end) {
        return std::chrono::duration<double, std::milli>(end - start).count();
    };

    // Save
    auto* data = new MassiveData();
    data->count = COUNT;
    std::mt19937 gen(42);
    for (size_t i = 0; i < COUNT; ++i) data->items[i].id = gen();

    auto s_start = std::chrono::high_resolution_clock::now();
    FastBin::FastBin::save(filename, *data);
    auto s_end = std::chrono::high_resolution_clock::now();
    delete data;

    // Load
    FastBin::FileMapper mapper;
    auto l_start = std::chrono::high_resolution_clock::now();
    mapper.map(filename, FastBin::MapMode::ReadOnly);
    auto [res] = FastBin::FastBin::load<MassiveData>(mapper);
    auto l_end = std::chrono::high_resolution_clock::now();

    // Checksum
    volatile uint32_t checksum = 0;
    auto c_start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < res->count; ++i) checksum += res->items[i].id;
    auto c_end = std::chrono::high_resolution_clock::now();

    // Output
    std::cout << "Save: " << ms(s_start, s_end) << " ms\n";
    std::cout << "Load: " << ms(l_start, l_end) << " ms\n";
    std::cout << "Proc: " << ms(c_start, c_end) << " ms\n";
    std::cout << "Sum:  " << checksum << std::endl;

    return 0;
}