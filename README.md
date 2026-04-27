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

## Benchmarks (Stress Test)
*Based on 1,000,000 `Particle` structures (~30 MB file):*

| Operation | Time |
| :--- | :--- |
| **Save (Write to Disk)** | ~20.9 ms |
| **Load (Mapping & Lookup)** | **0.037 ms (37 µs)** |

---

## Usage

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
auto [transform, material] = FastBin::FastBin::load<Transform, PhysicsMaterial>("assets.fbin", mapper);

if (transform && material) {
    // Access data directly from the mapped file
    std::cout << "Entity ID: " << transform->entity_id << std::endl;
    std::cout << "Material: " << material->label << std::endl;
} 
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
* **C++ Standard:** C++17 or higher.
* **Dependencies:** None (Header-only).
* **OS:** Linux, Windows, macOS.