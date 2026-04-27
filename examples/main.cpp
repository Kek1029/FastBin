//
// Created by etders on 27.04.2026.
//

#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <memory>

#include "FastBin.hpp"

struct Foo {
    uint32_t id = 1337;
    double timestamp = 1.23456789;
};

struct Particle {
    float pos[3];
    float velocity[3];
    uint32_t color;
    bool active;
};

const size_t PARTICLE_COUNT = 1000000;

int main() {
    const char* filename = "stress_test.bin";

    Foo f;
    auto particles = std::make_unique<Particle[]>(PARTICLE_COUNT);

    for (size_t i = 0; i < PARTICLE_COUNT; ++i) {
        particles[i].pos[0] = static_cast<float>(i);
        particles[i].velocity[1] = 0.5f;
        particles[i].active = (i % 2 == 0);
    }

    auto start_save = std::chrono::high_resolution_clock::now();

    struct ParticleCloud {
        Particle data[PARTICLE_COUNT];
    };

    auto& cloud = *reinterpret_cast<ParticleCloud*>(particles.get());

    FastBin::FastBin::save(filename, f, cloud);

    auto end_save = std::chrono::high_resolution_clock::now();
    auto start_load = std::chrono::high_resolution_clock::now();
    FastBin::FileMapper mapper;
    auto [res_foo, res_cloud] = FastBin::FastBin::load<Foo, ParticleCloud>(filename, mapper);

    auto end_load = std::chrono::high_resolution_clock::now();

    if (res_foo && res_cloud) {
        std::cout << "Particle count: " << PARTICLE_COUNT << std::endl;
        std::cout << "File size: ~" << (sizeof(Particle) * PARTICLE_COUNT) / 1024 / 1024 << " MB" << std::endl;

        auto save_ms = std::chrono::duration<double, std::milli>(end_save - start_save).count();
        auto load_ms = std::chrono::duration<double, std::milli>(end_load - start_load).count();

        std::cout << "Save time: " << std::fixed << std::setprecision(3) << save_ms << " ms" << std::endl;
        std::cout << "Load time: " << std::fixed << std::setprecision(3) << load_ms << " ms" << std::endl;

        std::cout << "Check Particle[500000] pos: " << res_cloud->data[500000].pos[0] << std::endl;
        std::cout << "Check Particle[999999] active: " << (res_cloud->data[999999].active ? "Yes" : "No") << std::endl;
    } else {
        std::cerr << "Stress test failed! Data corrupted or not found." << std::endl;
    }

    return 0;
}