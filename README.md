# GraphLite Engine v1.0

> **A high-performance Hybrid Graph Database for C++17.**  
> Memory-mapped storage · Index-free adjacency · CLOCK cache · Cross-platform

GraphLite is a lightweight, embeddable Property Graph Database designed for applications that need to handle **millions of nodes and edges** without consuming gigabytes of RAM. It uses a hybrid storage architecture: **hot data is served from an in-memory CLOCK cache**, while the full graph lives on **memory-mapped files** on disk.

## 🚀 Key Features

| Feature | Description |
|---------|-------------|
| **Hybrid Storage** | Combines mmap-backed on-disk storage with an intelligent CLOCK cache for hot nodes |
| **Index-Free Adjacency** | Edges form a linked list directly on disk — neighbor traversal is O(degree) with zero index lookups |
| **32-Byte Edge Alignment** | Each `DiskEdge` is exactly 32 bytes — 2 edges per CPU cache line, maximizing L1/L2 throughput |
| **23-Byte Opaque Payload** | Every edge carries a customizable binary payload — cast your own structs with `reinterpret_cast` |
| **Cross-Platform** | Runs on Linux, macOS, and Windows via a compile-time platform abstraction layer |
| **Dual Distribution** | Use as a **compiled static library** (fast builds) or **header-only** (zero build steps) |
| **DFS + BFS Traversal** | Built-in traversal engines with Strategy Pattern callbacks for runtime branch pruning |
| **Near-Zero Startup** | Memory-mapped files enable lazy loading — no deserialization step on startup |

## 🛠 System Requirements

| Requirement | Minimum |
|-------------|---------|
| **C++ Standard** | C++17 |
| **Compiler** | GCC 7+ / Clang 5+ / MSVC 2017+ |
| **Build System** | CMake 3.14+ |
| **OS** | Linux, macOS, or Windows 10+ |

---

## 📦 Installation & Build

### Option A: Compiled Static Library (Recommended)

This is the **default and recommended** mode. GraphLite compiles once into a `.a` (Linux/macOS) or `.lib` (Windows) file, giving your project the **fastest possible compile times**.

#### Linux / macOS

```bash
git clone https://github.com/Domi3012/graphlite.git
cd GraphLite
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Output: `build/libgraphlite.a`

#### Windows (Visual Studio)

```powershell
git clone https://github.com/Domi3012/graphlite.git
cd GraphLite
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Output: `build\Release\graphlite.lib`

#### Windows (MinGW)

```powershell
git clone https://github.com/Domi3012/graphlite.git
cd GraphLite
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Output: `build/libgraphlite.a`

---

### Option B: Header-Only Mode

If you want **zero build steps** — just copy the `include/` folder and go. Enable with `GRAPHLITE_HEADER_ONLY`:

#### Via CMake

```bash
cmake -B build -DGRAPHLITE_HEADER_ONLY=ON
cmake --build build   # Only compiles your app, not the library
```

#### Without CMake (Manual)

Copy the `include/graphlite/` folder into your project, then compile with:

```bash
# Linux / macOS
g++ -std=c++17 -O3 -DGRAPHLITE_HEADER_ONLY -I/path/to/include your_app.cpp -o your_app

# Windows (MSVC)
cl /std:c++17 /O2 /DGRAPHLITE_HEADER_ONLY /I"C:\path\to\include" your_app.cpp
```

> ⚠️ **Trade-off**: Header-only mode increases compile time for your project since every `.cpp` file that includes GraphLite will recompile the entire library. Use compiled mode for large projects.

---

## 🔗 Integrating Into Your Project

### Method 1: CMake `add_subdirectory` (Easiest)

Add GraphLite as a subdirectory in your project (e.g., via git submodule):

```bash
# In your project root:
git submodule add https://github.com/Domi3012/graphlite.git external/graphlite
```

Then in your `CMakeLists.txt`:

```cmake
# Compiled mode (default):
add_subdirectory(external/graphlite)
target_link_libraries(my_app PRIVATE graphlite)

# Or header-only mode:
set(GRAPHLITE_HEADER_ONLY ON CACHE BOOL "" FORCE)
add_subdirectory(external/graphlite)
target_link_libraries(my_app PRIVATE graphlite)
```

### Method 2: CMake `FetchContent` (No Submodule)

```cmake
include(FetchContent)
FetchContent_Declare(
    graphlite
    GIT_REPOSITORY https://github.com/Domi3012/graphlite.git
    GIT_TAG        v1.0
)
FetchContent_MakeAvailable(graphlite)
target_link_libraries(my_app PRIVATE graphlite)
```

### Method 3: System-Wide Install

```bash
# Build and install
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
sudo cmake --install build

# In your project's CMakeLists.txt:
find_package(GraphLite 1.0 REQUIRED)
target_link_libraries(my_app PRIVATE GraphLite::graphlite)
```

### Method 4: Manual Linking (No CMake)

#### Linux / macOS

```bash
# 1. Build the library
cd GraphLite && cmake -B build && cmake --build build

# 2. Compile your app and link
g++ -std=c++17 -O3 \
    -I/path/to/GraphLite/include \
    your_app.cpp \
    -L/path/to/GraphLite/build -lgraphlite \
    -o your_app
```

#### Windows (MSVC — Developer Command Prompt)

```powershell
cl /std:c++17 /O2 /EHsc ^
    /I"C:\path\to\GraphLite\include" ^
    your_app.cpp ^
    /link /LIBPATH:"C:\path\to\GraphLite\build\Release" graphlite.lib
```

---

## ⚡ Quick Start

```cpp
#include <graphlite/graphlite.h>
#include <iostream>

// Define your edge payload (up to 23 bytes)
struct Friendship {
    uint32_t since_year;
    float    trust_score;
};

// Define a traversal strategy
class FriendFilter : public graphlite::ITraversalCallback {
public:
    bool shouldTraverse(graphlite::NodeID current, const graphlite::GenericEdge& edge) override {
        auto* f = reinterpret_cast<const Friendship*>(edge.payload);
        return f->trust_score > 0.5f;  // Only follow high-trust edges
    }

    void onNodeVisited(graphlite::NodeID node) override {
        std::cout << "Visited node: " << node << "\n";
    }
};

int main() {
    // Open (or create) a database in a directory
    graphlite::GraphDB db("./my_graph");

    // Define schema
    auto FRIEND = db.defineEdgeType("FRIEND");

    // Add nodes
    auto alice = db.addNode("Alice");
    auto bob   = db.addNode("Bob");
    auto carol = db.addNode("Carol");

    // Add edges with payload
    Friendship f1{2020, 0.9f};
    Friendship f2{2023, 0.3f};
    db.addEdge(alice, bob,   FRIEND, reinterpret_cast<uint8_t*>(&f1), sizeof(f1));
    db.addEdge(bob,   carol, FRIEND, reinterpret_cast<uint8_t*>(&f2), sizeof(f2));

    // Traverse: DFS from Alice, max depth 5
    graphlite::TraversalEngine engine(db);
    FriendFilter filter;
    engine.dfs(alice, 5, filter);
    // Output: Visited Alice → Bob (Carol is pruned: trust 0.3 < 0.5)

    // Persist to disk
    db.sync();

    return 0;
}
```

---

## 🧪 Running Tests

```bash
# Build with tests (enabled by default)
cmake -B build && cmake --build build

# Run
./build/run_test           # Linux/macOS
.\build\Release\run_test   # Windows
```

## 📊 Running Benchmarks

```bash
cmake -B build -DGRAPHLITE_BUILD_BENCHMARKS=ON
cmake --build build

./build/stress_test         # Linux/macOS
.\build\Release\stress_test # Windows
```

## 📚 API Documentation

The project uses Doxygen with detailed comments (Vietnamese). To generate HTML docs:

```bash
doxygen Doxyfile
# Open docs/html/index.html in your browser
```

## 📄 License

This project is licensed under the **Apache-2.0 License** — see the [LICENSE](LICENSE) file for details.