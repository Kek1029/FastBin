# FastBin 

**A high-performance, header-only C++17 library for zero-copy binary serialization of POD data.**

`FastBin` is built for performance-critical applications where traditional serialization (JSON, XML, Protobuf) is too slow or memory-intensive. It uses memory-mapped files (`mmap` on Linux/macOS, `MapViewOfFile` on Windows) to provide **near-instant** access to your data.

## Why FastBin?

* **Zero-Copy:** Data is mapped directly into your process's address space. No parsing, no intermediate buffers.
* **Near-Zero Latency:** Loading 30MB of data (1,000,000 objects) takes **~35-45 microseconds**.
* **Cross-Platform:** Native support for Linux and Windows.
* **Type Safety:** Every data block is tagged with a 64-bit fingerprint (Type Hash + Size). You cannot accidentally load a `Texture` as a `Mesh`.
* **SIMD-Ready:** Automatic **16-byte alignment** for all data blocks to ensure maximum performance for SSE/AVX instructions.
* **Safety First:** Uses `static_assert` to prevent serialization of non-POD types (pointers, classes with virtual tables, etc.) at compile-time.

---

## Performance
Benchmark results on **i5-12400f** (Linux 6.x, NVMe SSD):
**100M objects** (1.5 GB file):
```
Save: 699.257 ms 
Load: 20.5404 ms
```

---

## Usage for trivial types

### 1. Define your POD structures
```cpp
struct Transform {
    float pos[3];
    uint32_t entity_id;
};

struct PhysicsMaterial {
    float friction;
    char label[16];
};
```

### 2. Save your data
```cpp
FastBin::FastBin::save("assets.fbin", 
    Transform{{10.0f, 0.0f, 5.0f}, 42}, 
    PhysicsMaterial{0.95f, "Concrete"}
);
```

### 3. Load with zero-copy
```cpp
FastBin::FileMapper mapper;
mapper.map("assets.fbin", FastBin::MapMode::ReadOnly);
auto [transform, material] = FastBin::FastBin::load<Transform, PhysicsMaterial>(mapper);

if (transform && material) {
    // Access data directly from the mapped file
    std::cout << "Entity ID: " << transform->entity_id << std::endl;
    std::cout << "Material: " << material->label << std::endl;
} 
```

## Usage for non-trivial types

#### If your type is not trivially_copyable (e.g., it contains dynamic arrays or needs custom logic), simply implement the CSerializable interface.
### 1. Implement the interface
```
cpp

#include <vector>
#include "FastBin.hpp"

struct PlayerData : public FastBin::ISerializabled<PlayerData> {
uint32_t level;
std::vector<uint32_t> inventory;

    // FastBin needs to know the exact size in the file
    size_t size() const {
        return sizeof(level) + sizeof(uint32_t) + (inventory.size() * sizeof(uint32_t));
    }

    // Write your data to the provided memory pointer
    void save(void* ptr) const {
        char* dst = static_cast<char*>(ptr);
        
        std::memcpy(dst, &level, sizeof(level));
        dst += sizeof(level);
        
        uint32_t count = static_cast<uint32_t>(inventory.size());
        std::memcpy(dst, &count, sizeof(count));
        dst += sizeof(count);
        
        std::memcpy(dst, inventory.data(), count * sizeof(uint32_t));
    }

    // Restore your object from the memory-mapped pointer
    void load(const void* ptr) {
        const char* src = static_cast<const char*>(ptr);
        
        std::memcpy(&level, src, sizeof(level));
        src += sizeof(level);
        
        uint32_t count;
        std::memcpy(&count, src, sizeof(count));
        src += sizeof(count);
        
        inventory.resize(count);
        std::memcpy(inventory.data(), src, count * sizeof(uint32_t));
    }
};
```

### 2. Save and Load

```
cpp

// Saving is the same!
PlayerData player{80, {101, 102, 505}};
FastBin::FastBin::save("player.bin", player);

// Loading (returns a copy for non-trivial types)
FastBin::FileMapper mapper;
mapper.map("player.bin", FastBin::MapMode::ReadOnly);

auto [loaded_player] = FastBin::FastBin::load<PlayerData>(mapper);
std::cout << "Player Level: " << loaded_player.level << std::endl;
```

---

## Technical Details

### Binary Layout
`FastBin` produces a tightly packed binary file designed for fast pointer arithmetic:
1.  **File Header (16 bytes):** Magic signature (`FASTBIN`), version, and type count.
2.  **Metadata Table:** Sequential 64-bit fingerprints for type validation.
3.  **Offset Table:** Absolute offsets to each data segment.
4.  **Data Segments:** Raw binary blobs, each padded to 16-byte boundaries.


### Requirements
* **C++ Standard:** C++20 or higher.
* **Dependencies:** None (Header-only).
* **OS:** Linux, Windows.