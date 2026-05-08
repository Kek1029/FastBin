add_executable(FastBinExample main.cpp)

target_link_libraries(FastBinExample PRIVATE FastBin)

target_compile_options(FastBinExample PRIVATE -O3 -g)