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

    enum class MapMode {
        ReadOnly,
        ReadWrite
    };

    class FileMapper {
    private:
        void* m_data = nullptr;
        size_t m_size = 0;
        MapMode m_mode = MapMode::ReadOnly;

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
        ~FileMapper() { unmap(); }

        bool map(const char* path, MapMode mode = MapMode::ReadOnly, size_t size = 0) {
            unmap();
            m_mode = mode;

#if defined(_WIN32)
            DWORD access = (mode == MapMode::ReadWrite) ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_READ;
            DWORD share  = FILE_SHARE_READ;
            DWORD disp   = (mode == MapMode::ReadWrite) ? OPEN_ALWAYS : OPEN_EXISTING;

            m_file = CreateFileA(path, access, share, NULL, disp, FILE_ATTRIBUTE_NORMAL, NULL);
            if (m_file == INVALID_HANDLE_VALUE) return false;

            if (mode == MapMode::ReadWrite && size > 0) {
                LARGE_INTEGER li; li.QuadPart = size;
                if (!SetFilePointerEx(m_file, li, NULL, FILE_BEGIN) || !SetEndOfFile(m_file)) {
                    CloseHandle(m_file); return false;
                }
                m_size = size;
            } else {
                LARGE_INTEGER fs;
                GetFileSizeEx(m_file, &fs);
                m_size = static_cast<size_t>(fs.QuadPart);
            }

            DWORD flProtect = (mode == MapMode::ReadWrite) ? PAGE_READWRITE : PAGE_READONLY;
            m_mapping = CreateFileMapping(m_file, NULL, flProtect, 0, 0, NULL);
            if (!m_mapping) { CloseHandle(m_file); return false; }

            DWORD mapAccess = (mode == MapMode::ReadWrite) ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ;
            m_data = MapViewOfFile(m_mapping, mapAccess, 0, 0, 0);
#else
            int flags = (mode == MapMode::ReadWrite) ? (O_RDWR | O_CREAT) : O_RDONLY;
            m_fd = ::open(path, flags, 0644);
            if (m_fd == -1) return false;

            if (mode == MapMode::ReadWrite && size > 0) {
                if (ftruncate(m_fd, size) == -1) { ::close(m_fd); return false; }
                m_size = size;
            } else {
                struct stat st;
                fstat(m_fd, &st);
                m_size = st.st_size;
            }

            int prot = (mode == MapMode::ReadWrite) ? (PROT_READ | PROT_WRITE) : PROT_READ;
            m_data = mmap(NULL, m_size, prot, MAP_SHARED, m_fd, 0);
            if (m_data == MAP_FAILED) { ::close(m_fd); m_data = nullptr; return false; }
#endif
            return isValid();
        }

        FORCE_INLINE char* data() { return static_cast<char*>(m_data); }
        FORCE_INLINE const char* data() const { return static_cast<const char*>(m_data); }
        FORCE_INLINE size_t size() const { return m_size; }
        FORCE_INLINE bool isValid() const { return m_data != nullptr; }
    };
} // namespace FastBin

#endif // FASTBIN_FILEMAPPER_HPP