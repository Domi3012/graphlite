# GraphLite Architecture Refactoring Summary

## Tổng quan
GraphLite đã được tái cấu trúc từ mô hình **Hybrid (Header-Only / Compiled)** thành mô hình **Thư viện C++ Tiêu chuẩn (Static / Shared)**. 

Mục tiêu chính của đợt refactoring này là:
1. Xóa bỏ hoàn toàn sự phụ thuộc vào các macro rườm rà (`GRAPHLITE_FUNC`, `GRAPHLITE_HEADER_ONLY`, `GRAPHLITE_IMPL_GUARD`, ...).
2. Tách biệt rõ ràng ranh giới giữa Public API và Internal Implementation.
3. Ẩn hoàn toàn các cấu trúc dữ liệu và logic nội bộ khỏi người dùng thư viện thông qua mô hình thiết kế **PIMPL (Pointer to Implementation)**.

## Chi tiết thay đổi

### 1. Cấu trúc thư mục mới
Thư mục dự án đã được phân chia lại:

* **`include/graphlite/` (Public API):**
  * Chứa các interface duy nhất mà người dùng nhìn thấy: `graphlite.h`, `GraphDB.h`, `Schema.h`, `traversal.h`, `types.h`, `MiniVector.h`.
* **`src/` (Internal Implementation):**
  * `src/utils/`: Các tiện ích cấu trúc dữ liệu (`StringPoolDictionary`, `ClockCache`, `NumericHashMap`, `BinaryUtils`).
  * `src/storage/`: Lớp thao tác bộ nhớ/đĩa (`Platform`, `MmapFile`, `NodeStore`, `PageManager`).
  * `src/core/`: Nơi thực thi implementation của các public APIs (`GraphDB.cpp`, `Schema.cpp`, `traversal.cpp`).
  * `src/engine/`: Chứa backend của Storage Engine (như `BitcaskEngine.cpp`).

### 2. Áp dụng PIMPL Idiom
Để tránh rò rỉ (leak) các header nội bộ ra `include/graphlite/`, các class core như `GraphDB` và `Schema` đã được áp dụng PIMPL:

* Trước đây `GraphDB.h` include trực tiếp `StringPoolDictionary.h`. Giờ đây nó chỉ chứa một pointer: `std::unique_ptr<Impl> pimpl_;`
* Sự phức tạp của class được giấu hoàn toàn vào `GraphDB.cpp` và `Schema.cpp`.
* Việc này giúp thời gian compile của ứng dụng dùng thư viện GraphLite nhanh hơn rất nhiều (giảm thiểu header dependency).

### 3. Cập nhật CMake
File `CMakeLists.txt` đã được thay đổi:
* Bỏ option `GRAPHLITE_HEADER_ONLY`.
* Thêm hỗ trợ option tiêu chuẩn `BUILD_SHARED_LIBS` để quyết định xây dựng Static hay Shared Library.
* Tự động quét (glob) toàn bộ source trong `src/` để compile library.

### 4. Kết quả kiểm thử
Toàn bộ test suites (62 tests) của giai đoạn 1 đã được biên dịch lại với thiết kế library mới và **PASS 100%**. Không có hồi quy (regression) nào được ghi nhận.
