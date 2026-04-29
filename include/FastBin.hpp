//
// Created by etders on 27.04.2026.
//

#ifndef FASTBIN_FASTBIN_HPP
#define FASTBIN_FASTBIN_HPP

#include <string_view>
#include <tuple>
#include <fstream>
#include <cstring>

#include "FileMapper.hpp"
#include "Utils.hpp"

#define MAGIC "FASTBIN"
#define VERSION "002"

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
    public:

        template<typename... Types>
        static void save(const std::string_view path, const Types&... args) {
            static_assert((std::is_trivially_copyable_v<Types> && ...),
                "FastBin only supports trivially copyable types");

            const uint32_t count = sizeof...(Types);
            const size_t meta_size = count * sizeof(uint64_t);
            const size_t offset_size = count * sizeof(uint64_t);

            size_t current_data_offset = 0;
            uint64_t calculated_offsets[sizeof...(Types)];
            size_t idx = 0;

            auto calc_offsets = [&](size_t s) {
                calculated_offsets[idx++] = current_data_offset;
                current_data_offset += (s + 15) & ~15;
            };
            (calc_offsets(sizeof(Types)), ...);

            const size_t total_file_size = sizeof(FileHeader) + meta_size + offset_size + current_data_offset;

            FileMapper mapper;
            if (!mapper.map(path.data(), MapMode::ReadWrite, total_file_size)) {
                return;
            }

            char* base_ptr = mapper.data();

            auto* header = reinterpret_cast<FileHeader*>(base_ptr);
            std::memcpy(header->magic, MAGIC, 8);
            std::memcpy(header->version, VERSION, 4);
            header->count = count;
            header->reserved = 0;

            uint64_t* fingerprints_ptr = reinterpret_cast<uint64_t*>(base_ptr + sizeof(FileHeader));
            uint64_t tmp_fingerprints[] = { GetMetaData<Types>()... };
            std::memcpy(fingerprints_ptr, tmp_fingerprints, meta_size);

            uint64_t* offsets_ptr = fingerprints_ptr + count;
            std::memcpy(offsets_ptr, calculated_offsets, offset_size);

            char* data_start = reinterpret_cast<char*>(offsets_ptr + count);
            idx = 0;
            auto write_data = [&](const auto& obj) {
                std::memcpy(data_start + calculated_offsets[idx++], &obj, sizeof(obj));
            };
            (write_data(args), ...);

        }

        template<typename... Types>
        static auto load(const std::string_view path, FileMapper& mapper) {
            if (!mapper.map(path.data(), MapMode::ReadOnly)) {
                return std::make_tuple(static_cast<const Types*>(nullptr)...);
            }

            const char* base_ptr = mapper.data();
            auto* header = reinterpret_cast<const FileHeader*>(base_ptr);

            if (std::string_view(header->magic, 7) != MAGIC || std::string_view(header->version, 4) != VERSION) {
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