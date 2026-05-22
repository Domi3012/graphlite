#include <iostream>
#include <cassert>
#include <string>
#include <cstdio> // Để dùng std::remove xóa file sau khi test xong

// Include các module của bạn (Giả định đúng đường dẫn)
#include "../src/engine/BitcaskEngine.h"
#include "../src/utils/ByteBuffer.h"

using namespace graphlite;

// --- HÀM HELPER GIÚP TEST NHANH ---

// Đổi std::string thành ByteBuffer
utils::ByteBuffer stringToBuffer(const std::string& str) {
    utils::ByteBuffer buf;
    for (char c : str) {
        buf.push_back(static_cast<uint8_t>(c));
    }
    return buf;
}

// Đổi ByteBuffer thành std::string để dễ so sánh bằng assert
std::string bufferToString(const utils::ByteBuffer& buf) {
    if (buf.size() == 0) return "";
    return std::string(reinterpret_cast<const char*>(buf.data()), buf.size());
}

// --- HÀM TEST CHÍNH ---
int main() {
    std::string test_file = "test_graphlite.db";

    // 1. Dọn dẹp file cũ nếu có để test môi trường sạch
    std::remove(test_file.c_str());

    std::cout << "[*] Khoi tao BitcaskEngine..." << std::endl;
    {
        // Dùng block { } để ép gọi Destructor và đóng file khi ra khỏi scope
        graphlite::BitcaskEngine db(test_file);

        // --- TEST 1: GHI VÀ ĐỌC MỘT RECORD CƠ BẢN ---
        std::cout << "[*] Test 1: Put & Get co ban" << std::endl;
        bool put_res = db.put("Node:Khoi", stringToBuffer("Student at HCMUS"));
        assert(put_res == true);

        utils::ByteBuffer out_buf1;
        bool get_res1 = db.get("Node:Khoi", out_buf1);
        assert(get_res1 == true);
        assert(bufferToString(out_buf1) == "Student at HCMUS");


        // --- TEST 2: GHI ĐÈ DỮ LIỆU (UPDATE) ---
        std::cout << "[*] Test 2: Ghi de du lieu (Append-only Update)" << std::endl;
        db.put("Node:Khoi", stringToBuffer("Backend Developer"));
        
        utils::ByteBuffer out_buf2;
        bool get_res2 = db.get("Node:Khoi", out_buf2);
        assert(get_res2 == true);
        assert(bufferToString(out_buf2) == "Backend Developer"); // Phải ra data mới


        // --- TEST 3: GHI NHIỀU RECORD ---
        std::cout << "[*] Test 3: Them nhieu record" << std::endl;
        db.put("Node:Phat", stringToBuffer("Teammate 1"));
        db.put("Node:Khanh", stringToBuffer("Teammate 2"));

        utils::ByteBuffer out_buf3;
        db.get("Node:Phat", out_buf3);
        assert(bufferToString(out_buf3) == "Teammate 1");


        // --- TEST 4: XÓA RECORD ---
        std::cout << "[*] Test 4: Xoa record (Tombstone)" << std::endl;
        bool del_res = db.del("Node:Phat");
        assert(del_res == true);

        utils::ByteBuffer out_buf4;
        bool get_res4 = db.get("Node:Phat", out_buf4);
        assert(get_res4 == false); // Không thể tìm thấy nữa vì đã bị Tombstone che


        // --- TEST 5: ĐỌC RECORD KHÔNG TỒN TẠI ---
        std::cout << "[*] Test 5: Doc Key khong ton tai" << std::endl;
        utils::ByteBuffer out_buf5;
        bool get_res5 = db.get("Node:NguoiLa", out_buf5);
        assert(get_res5 == false);
    }
    
    // Test hoàn thành, xóa bỏ file rác
    std::remove(test_file.c_str());
    
    std::cout << "\n=======================================" << std::endl;
    std::cout << " TAT CA TEST PASS! ENGINE HOAT DONG TOT! " << std::endl;
    std::cout << "=======================================" << std::endl;

    return 0;
}