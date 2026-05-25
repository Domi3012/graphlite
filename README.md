# GraphLite Engine v0.1

GraphLite is a high-performance In-Memory Property Graph Database written entirely in modern C++11. The system utilizes a Log-Structured storage architecture (Bitcask) to ensure maximum I/O throughput.

## 🚀 Key Features

* **O(1) Access Speed:** Manages the graph using an Adjacency List architecture on RAM via a flattened array, allowing neighbor traversal at raw CPU memory access speeds.
* **Zero Garbage (Manual Memory Management):** Completely manual memory allocation utilizing a custom `StringPoolDictionary` (Arena Allocator) and `MiniVector` to eliminate runtime memory fragmentation.
* **Bitcask I/O:** Synchronizes data to the hard drive using an ultra-fast Append-Only Log model, highly optimized for massive data Bulk Loading.
* **Flexible Graph Traversal:** The built-in DFS engine integrates the Strategy Pattern (`ITraversalCallback`), empowering the application layer to perform dynamic branch pruning during traversal.
* **Cache Optimization:** The Edge structure is strictly 16-byte aligned, making it highly friendly to L1/L2 CPU Caches.

## 🛠 System Requirements
* C++11 Compiler (GCC / Clang)
* CMake 3.10+

## 📦 Build Instructions

GraphLite is designed to be compiled as a Static Library.

```bash
git clone https://github.com/Domi3012/graphlite.git
cd GraphLite
mkdir build && cd build
cmake ..
make
```

Upon a successful build, the `libgraphlite-0.1.a` file will be generated, ready to be linked into your applications.

## 🧪 Running Tests

The project comes with a built-in test suite to verify the graph traversal algorithms and I/O mechanisms.
Bash

```bash
cd build
./run_test
```

## 📚 API Documentation

The project uses Doxygen with industrial-standard comments. To generate the internal HTML documentation:
Bash

```bash
doxygen Doxyfile
# Open docs/html/index.html in your web browser
```

## 📄 License

This project is licensed under the Apache-2.0 License - see the LICENSE file for details.