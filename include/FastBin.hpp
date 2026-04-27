//
// Created by etders on 27.04.2026.
//

#ifndef FASTBIN_FASTBIN_HPP
#define FASTBIN_FASTBIN_HPP

#include <string_view>
#include <tuple>
#include <fstream>
#include <vector>

#include "FileMapper.hpp"
#include "Utils.hpp"

#define MAGIC "FASTBIN"
#define VERSION "001"

namespace FastBin {

    #pragma pack(push, 1)
    struct FileHeader {
        char magic[8];
        char version[4];
        uint32_t count;
        uint32_t reserved;
    };
    #pragma pack(pop)

    template <typename T>
    constexpr uint32_t GetTypeID() {
        return fnv1_a(__PRETTY_FUNCTION__);
    }

    template <typename T>
    constexpr uint64_t GetMetaData() {
        return (static_cast<uint64_t>(GetTypeID<T>()) << 32) | (sizeof(T) & 0xFFFFFFFF);
    }

    class FastBin {
    private:
        inline static FileMapper mapper;
    public:

        template<typename... Types>
        static void save(const std::string_view path, const Types&... args) {
            static_assert((std::is_trivially_copyable_v<Types> && ...),
                "FastBin only supports trivally copyable types");
            std::ofstream file(path.data(), std::ios::binary);
            if (!file.is_open()) return;

            FileHeader header = { MAGIC, VERSION, sizeof...(Types), 0 };
            file.write(reinterpret_cast<const char*>(&header), sizeof(FileHeader));

            uint64_t fingerprints[] = { GetMetaData<Types>()... };
            file.write(reinterpret_cast<const char*>(fingerprints), sizeof(fingerprints));

            uint64_t offsets[sizeof...(Types)];
            uint64_t current_offset = 0;
            size_t idx = 0;

            auto calc_offsets = [&](size_t s) {
                offsets[idx++] = current_offset;
                current_offset += (s + 15) & ~15;
            };
            (calc_offsets(sizeof(Types)), ...);
            file.write(reinterpret_cast<const char*>(offsets), sizeof(offsets));

            auto write_data = [&](const auto& obj) {
                file.write(reinterpret_cast<const char*>(&obj), sizeof(obj));
                size_t pad = ((sizeof(obj) + 15) & ~15) - sizeof(obj);
                if (pad > 0) {
                    char zeroes[16] = {0};
                    file.write(zeroes, pad);
                }
            };
            (write_data(args), ...);
        }

        template<typename... Types>
        static auto load(const std::string_view path) {
            if (!mapper.map(path.data())) {
                return std::make_tuple(static_cast<const Types*>(nullptr)...);
            }

            const char* base_ptr = mapper.data();
            auto* header = reinterpret_cast<const FileHeader*>(base_ptr);

            if (std::string_view(header->magic, 7) != MAGIC) {
                return std::make_tuple(static_cast<const Types*>(nullptr)...);
            }

            const uint64_t* file_meta = reinterpret_cast<const uint64_t*>(base_ptr + sizeof(FileHeader));
            const uint64_t* file_offsets = file_meta + header->count;
            const char* data_start = reinterpret_cast<const char*>(file_offsets + header->count);

            auto get_ptr = [&](auto dummy) {
                using T = std::remove_pointer_t<decltype(dummy)>;
                uint64_t target_meta = GetMetaData<T>();

                for (uint32_t i = 0; i < header->count; ++i) {
                    if (file_meta[i] == target_meta) {
                        return reinterpret_cast<const T*>(data_start + file_offsets[i]);
                    }
                }
                return static_cast<const T*>(nullptr);
            };

            return std::make_tuple(get_ptr(static_cast<Types*>(nullptr))...);
        }
    };
}

#endif