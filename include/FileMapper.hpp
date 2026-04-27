#ifndef FASTBIN_FILEMAPPER_HPP
#define FASTBIN_FILEMAPPER_HPP

#include <cstdint>
#include <cstddef>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif

#if defined(_MSC_VER)
    #define FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
    #define FORCE_INLINE __attribute__((always_inline)) inline
#else
    #define FORCE_INLINE inline
#endif

namespace FastBin {

    inline void* ReserveAddressSpace(size_t size) {
#if defined(_WIN32)
        return VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
#else
        return mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
    }

    inline void CommitMemory(void* addr, size_t size) {
#if defined(_WIN32)
        VirtualAlloc(addr, size, MEM_COMMIT, PAGE_READWRITE);
#else
        mprotect(addr, size, PROT_READ | PROT_WRITE);
#endif
    }

    class FileMapper {
    private:
        void* m_data = nullptr;
        size_t m_size = 0;

#if defined(_WIN32)
        HANDLE m_file = INVALID_HANDLE_VALUE;
        HANDLE m_mapping = NULL;
#else
        int m_fd = -1;
#endif

        void unmap() {
            if (!m_data) return;
#if defined(_WIN32)
            UnmapViewOfFile(m_data);
            if (m_mapping) CloseHandle(m_mapping);
            if (m_file != INVALID_HANDLE_VALUE) CloseHandle(m_file);
            m_mapping = NULL;
            m_file = INVALID_HANDLE_VALUE;
#else
            munmap(m_data, m_size);
            if (m_fd != -1) ::close(m_fd);
            m_fd = -1;
#endif
            m_data = nullptr;
            m_size = 0;
        }

    public:
        FileMapper() = default;
        FileMapper(const char* path) { map(path); }
        ~FileMapper() { unmap(); }

        FileMapper(const FileMapper&) = delete;
        FileMapper& operator=(const FileMapper&) = delete;

        bool map(const char* path) {
            unmap();
#if defined(_WIN32)
            m_file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (m_file == INVALID_HANDLE_VALUE) return false;

            LARGE_INTEGER fileSize;
            if (!GetFileSizeEx(m_file, &fileSize)) { CloseHandle(m_file); return false; }
            m_size = static_cast<size_t>(fileSize.QuadPart);

            m_mapping = CreateFileMapping(m_file, NULL, PAGE_READONLY, 0, 0, NULL);
            if (!m_mapping) { CloseHandle(m_file); return false; }

            m_data = MapViewOfFile(m_mapping, FILE_MAP_READ, 0, 0, 0);
#else
            m_fd = ::open(path, O_RDONLY);
            if (m_fd == -1) return false;

            struct stat st;
            if (fstat(m_fd, &st) == -1) { ::close(m_fd); return false; }
            m_size = st.st_size;

            m_data = mmap(NULL, m_size, PROT_READ, MAP_PRIVATE, m_fd, 0);
            if (m_data == MAP_FAILED) { ::close(m_fd); m_fd = -1; m_data = nullptr; return false; }
#endif
            return isValid();
        }

        FORCE_INLINE const char* data() const { return static_cast<const char*>(m_data); }
        FORCE_INLINE size_t size() const { return m_size; }
        FORCE_INLINE bool isValid() const { return m_data != nullptr; }
    };

} // namespace FastBin

#endif // FASTBIN_FILEMAPPER_HPP