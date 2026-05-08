//
// Created by etders on 27.04.2026.
//

#ifndef FASTBIN_FASTBIN_HPP
#define FASTBIN_FASTBIN_HPP

#include <string_view>
#include <tuple>
#include <fstream>
#include <cstring>
#include <concepts>
#include <type_traits>

#include "FileMapper.hpp"
#include "Utils.hpp"

#define MAGIC "FASTBIN"
#define VERSION "003"

namespace FastBin {

    template <typename T>
    concept CSerializabled = requires(T v, void* ptr) {
        { v.save(ptr) } -> std::same_as<void>;
        { v.load(ptr) } -> std::same_as<void>;
        { v.size() } -> std::convertible_to<size_t>;
    };

    template <typename T>
    concept CanSavedData = std::is_trivially_copyable_v<T> || CSerializabled<T>;

    template<typename T>
    using LoadResult_t = std::conditional_t<std::is_trivially_copyable_v<T>, const T*, T>;

    template <typename T>
    struct ISerializabled {
    public:
        void save(void* ptr) const {
            static_assert(CSerializabled<T>, "T must have methods size() and save(void*)!");
            static_cast<T&>(*this).save(ptr);
        }
    };

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

    template<typename T>
    FORCE_INLINE size_t getSize(const T& obj) {
        if constexpr (CSerializabled<T>) {
            return obj.size();
        } else {
            return sizeof(T);
        }
    }

    template <typename T>
    constexpr uint64_t GetMetaData() {
        return (static_cast<uint64_t>(GetTypeID<T>()) << 32) | (sizeof(T) & 0xFFFFFFFF);
    }

    class FastBin {
    private:
        template<typename T>
        static auto make_default() {
            if constexpr (std::is_pointer_v<T>) {
                return static_cast<T>(nullptr);
            } else {
                return T{};
            }
        }
    public:

        template<typename... Types>
        static void save(const std::string_view path, const Types&... args) {
            static_assert((CanSavedData<Types> && ...), "Current type can't be save by FastBin");

            const uint32_t count = sizeof...(Types);
            const size_t meta_size = count * sizeof(uint64_t);
            const size_t offset_size = count * sizeof(uint64_t);

            size_t current_data_offset = 0;
            uint64_t calculated_offsets[sizeof...(Types)];
            size_t idx = 0;

            auto calc_offsets = [&](const auto& obj) {
                calculated_offsets[idx++] = current_data_offset;
                size_t s = getSize(obj);
                current_data_offset += (s + 15) & ~15;
            };
            (calc_offsets(args), ...);

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
                char* target_ptr = data_start + calculated_offsets[idx++];
                if constexpr (CSerializabled<std::remove_cvref_t<decltype(obj)>>) {
                    obj.save(target_ptr);
                } else {
                    std::memcpy(target_ptr, &obj, sizeof(obj));
                }
            };
            (write_data(args), ...);

        }

        template<typename... Types>
        static auto load(const FileMapper& mapper) {
            using TupleType = std::tuple<LoadResult_t<Types>...>;

            if (!mapper.isValid()) {
                return TupleType{ make_default<LoadResult_t<Types>>()... };
            }

            const char* base_ptr = mapper.data();
            auto* header = reinterpret_cast<const FileHeader*>(base_ptr);

            if (*reinterpret_cast<const uint64_t*>(header->magic) != *reinterpret_cast<const uint64_t*>(MAGIC)) {
                return TupleType{ make_default<LoadResult_t<Types>>()... };
            }

            const uint64_t* file_meta = reinterpret_cast<const uint64_t*>(base_ptr + sizeof(FileHeader));
            const uint64_t* file_offsets = file_meta + header->count;
            const char* data_start = reinterpret_cast<const char*>(file_offsets + header->count);

            auto extract = [&](auto dummy) -> LoadResult_t<std::remove_pointer_t<decltype(dummy)>> {
                using T = std::remove_pointer_t<decltype(dummy)>;
                uint64_t target_meta = GetMetaData<T>();

                for (uint32_t i = 0; i < header->count; ++i) {
                    if (file_meta[i] == target_meta) {
                        const void* raw_ptr = data_start + file_offsets[i];
                        if constexpr (std::is_trivially_copyable_v<T>) {
                            return reinterpret_cast<const T*>(raw_ptr);
                        } else {
                            T obj;
                            obj.load(raw_ptr);
                            return obj;
                        }
                    }
                }
                return make_default<LoadResult_t<T>>();
            };

            return std::make_tuple(extract(static_cast<Types*>(nullptr))...);
        }
    };
}

#endif