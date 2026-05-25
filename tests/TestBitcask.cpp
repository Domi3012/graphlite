#include <iostream>
#include <string>
#include <cstring>
#include "../src/engine/BitcaskEngine.h"
#include "../src/utils/MiniVector.h"

using namespace graphlite;
using namespace graphlite::utils;

// --- CÁC HÀM HELPER ĐỂ TEST ---

// Chuyển chuỗi std::string thành MiniVector<uint8_t> (Đóng gói Payload)
MiniVector<uint8_t> stringToPayload(const std::string& str) {
    MiniVector<uint8_t> vec;
    for (char c : str) {
        vec.push_back(static_cast<uint8_t>(c));
    }
    return vec;
}

// Chuyển MiniVector<uint8_t> ngược lại thành std::string (Giải mã Payload)
std::string payloadToString(const MiniVector<uint8_t>& vec) {
    std::string str;
    for (size_t i = 0; i < vec.size(); ++i) {
        str += static_cast<char>(vec[i]);
    }
    return str;
}

// Hàm in kết quả test
void printTestResult(const std::string& test_name, bool passed) {
    if (passed) {
        std::cout << "[PASS] " << test_name << "\n";
    } else {
        std::cout << "[FAIL] " << test_name << "\n";
    }
}

// --- HÀM MAIN ---

int main() {
    std::cout << "=== KHOI DONG GRAPHLITE BITCASK TEST ===\n\n";

    // Khởi tạo Engine, file dữ liệu sẽ được tạo ở thư mục hiện tại
    std::string db_file = "test_data.db";
    BitcaskEngine db(db_file);

    // ==========================================
    // TEST 1: Ghi dữ liệu mới (INSERT)
    // ==========================================
    uint32_t node_id_1 = 1001;
    std::string data_1 = "Toi la Khoi. Toi thich dung Arch Linux!";
    MiniVector<uint8_t> payload_1 = stringToPayload(data_1);
    
    bool put_result = db.put(node_id_1, payload_1);
    printTestResult("Test 1 - Ghi du lieu (put)", put_result);

    // ==========================================
    // TEST 2: Đọc dữ liệu lên (GET)
    // ==========================================
    MiniVector<uint8_t> retrieved_payload;
    bool get_result = db.get(node_id_1, retrieved_payload);
    std::string retrieved_str = payloadToString(retrieved_payload);
    
    bool test_2_passed = get_result && (retrieved_str == data_1);
    printTestResult("Test 2 - Doc du lieu (get)", test_2_passed);
    if (test_2_passed) {
        std::cout << "       -> Payload doc duoc: " << retrieved_str << "\n";
    }

    // ==========================================
    // TEST 3: Cập nhật dữ liệu đè lên Key cũ (UPDATE)
    // ==========================================
    std::string data_1_updated = "Toi la Khoi. Toi dang code GraphLite Engine.";
    MiniVector<uint8_t> payload_1_updated = stringToPayload(data_1_updated);
    
    db.put(node_id_1, payload_1_updated);
    
    MiniVector<uint8_t> retrieved_updated;
    db.get(node_id_1, retrieved_updated);
    std::string retrieved_updated_str = payloadToString(retrieved_updated);

    bool test_3_passed = (retrieved_updated_str == data_1_updated);
    printTestResult("Test 3 - Cap nhat du lieu (update)", test_3_passed);
    if (test_3_passed) {
        std::cout << "       -> Payload moi: " << retrieved_updated_str << "\n";
    }

    // ==========================================
    // TEST 4: Xóa dữ liệu (DELETE)
    // ==========================================
    bool del_result = db.del(node_id_1);
    printTestResult("Test 4 - Xoa du lieu (del) thanh cong", del_result);

    // ==========================================
    // TEST 5: Đọc lại dữ liệu đã xóa (TOMBSTONE CHECK)
    // ==========================================
    MiniVector<uint8_t> deleted_payload;
    bool get_deleted_result = db.get(node_id_1, deleted_payload);
    
    // Yêu cầu trả về false vì dữ liệu đã bị đánh dấu xóa (Tombstone = 1)
    bool test_5_passed = (get_deleted_result == false);
    printTestResult("Test 5 - Kiem tra Tombstone (tu choi doc du lieu da xoa)", test_5_passed);

    // ==========================================
    // TEST 6: Ép ổ cứng chốt dữ liệu (SYNC)
    // ==========================================
    std::cout << "\n[INFO] Dang goi sync() de flush xuong o cung...\n";
    db.sync();
    std::cout << "[INFO] Sync hoan tat. Kiem tra thu muc xem file test_data.db da co chua.\n";

    std::cout << "\n=== KET THUC TEST ===\n";
    return 0;
}