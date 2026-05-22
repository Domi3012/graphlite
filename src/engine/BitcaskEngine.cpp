#include "BitcaskEngine.h"
#include "../utils/BinaryUtils.h"
#include "../utils/HashMap.h" // Nhúng thư viện tự trồng
#include <stdexcept>

namespace graphlite {

// 1. CONSTRUCTOR: Khởi tạo DB
BitcaskEngine::BitcaskEngine(const std::string& file_path) : file_path_(file_path) {
    file_.open(file_path_, std::ios::in | std::ios::out | std::ios::app | std::ios::binary);
    
    if (!file_.is_open()) {
        file_.clear();
        file_.open(file_path_, std::ios::out | std::ios::binary); // Tạo file
        file_.close();
        file_.open(file_path_, std::ios::in | std::ios::out | std::ios::app | std::ios::binary);
    }

    if (!file_.is_open()) {
        throw std::runtime_error("Khong the mo hoac tao file database!");
    }

    // loadIndex(); 
}

// 2. DESTRUCTOR: Đóng DB an toàn
BitcaskEngine::~BitcaskEngine() {
    if (file_.is_open()) {
        file_.flush(); 
        file_.close();
    }
}

// 3. PUT: Ghi dữ liệu xuống đĩa 
bool BitcaskEngine::put(const std::string& key, const utils::ByteBuffer& value) {
    file_.clear(); 

    // Nhảy xuống EOF để ghi nối đuôi
    file_.seekp(0, std::ios::end);
    size_t offset = file_.tellp(); 

    utils::ByteBuffer record;
    
    // Byte 1: Tombstone (0 = Tồn tại, 1 = Bị xóa)
    record.push_back(0); 

    // Byte 2 -> 5: Độ dài Key (Tự dịch bit, không xài hàm writeUint32 để né std::vector)
    uint32_t k_len = static_cast<uint32_t>(key.length());
    record.push_back(static_cast<uint8_t>(k_len & 0xFF));
    record.push_back(static_cast<uint8_t>((k_len >> 8) & 0xFF));
    record.push_back(static_cast<uint8_t>((k_len >> 16) & 0xFF));
    record.push_back(static_cast<uint8_t>((k_len >> 24) & 0xFF));

    // Byte 6 -> 9: Độ dài Value 
    uint32_t v_len = static_cast<uint32_t>(value.size());
    record.push_back(static_cast<uint8_t>(v_len & 0xFF));
    record.push_back(static_cast<uint8_t>((v_len >> 8) & 0xFF));
    record.push_back(static_cast<uint8_t>((v_len >> 16) & 0xFF));
    record.push_back(static_cast<uint8_t>((v_len >> 24) & 0xFF));

    // Ghi Raw Key 
    for (char c : key) {
        record.push_back(static_cast<uint8_t>(c));
    }

    // Ghi Raw Value 
    for (size_t i = 0; i < value.size(); ++i) {
        record.push_back(value.data()[i]);
    }

    // Nã mảng byte xuống đĩa
    file_.write(reinterpret_cast<const char*>(record.data()), record.size());
    file_.flush();

    // Cập nhật lên RAM (Sử dụng cú pháp put của HashMap tự chế)
    key_dir_.put(key, offset);

    return true;
}

// 4. GET: Đọc dữ liệu lên RAM bằng quyền năng của O(1)
bool BitcaskEngine::get(const std::string& key, utils::ByteBuffer& value) {
    size_t offset = 0;
    
    // Bước 1: Tra sổ RAM xem Key có tồn tại không
    if (!key_dir_.get(key, offset)) {
        return false; // Không tồn tại
    }

    file_.clear();
    
    // Bước 2: Chỉ huy kim đọc đĩa nhảy tót xuống đúng tọa độ đó
    file_.seekg(offset, std::ios::beg);

    // Bước 3: Đọc 1 byte Tombstone
    char tombstone;
    file_.read(&tombstone, 1);
    
    // Dù trên RAM có offset, nhưng nếu dưới đĩa đánh dấu 1 thì là đã xóa (phòng hờ)
    if (tombstone == 1) {
        return false; 
    }

    // Bước 4: Đọc 8 bytes tiếp theo (4 bytes KeySize + 4 bytes ValueSize)
    char header[8];
    file_.read(header, 8);

    // Dịch ngược bit (Deserialize) để lấy lại số nguyên
    // Lưu ý ép kiểu về uint8_t trước để tránh bị Sign Extension (rác số âm)
    uint32_t k_len = static_cast<uint8_t>(header[0]) |
                    (static_cast<uint8_t>(header[1]) << 8) |
                    (static_cast<uint8_t>(header[2]) << 16) |
                    (static_cast<uint8_t>(header[3]) << 24);

    uint32_t v_len = static_cast<uint8_t>(header[4]) |
                    (static_cast<uint8_t>(header[5]) << 8) |
                    (static_cast<uint8_t>(header[6]) << 16) |
                    (static_cast<uint8_t>(header[7]) << 24);

    // Bước 5: Bỏ qua cái Key (vì ta biết nó là gì rồi, đọc lên làm gì cho phí RAM)
    file_.seekg(k_len, std::ios::cur); // Lệnh nhảy cóc siêu việt

    // Bước 6: Lấy Value
    if (v_len > 0) {
        char* temp_buf = new char[v_len]; // Cấp phát tạm vùng nhớ
        file_.read(temp_buf, v_len);
        
        // Copy từng byte vào ByteBuffer trả về cho Graph
        for (uint32_t i = 0; i < v_len; ++i) {
            value.push_back(static_cast<uint8_t>(temp_buf[i]));
        }
        delete[] temp_buf; // Nhớ dọn rác
    }

    return true;
}

// 5. DEL: "Xóa" dữ liệu (Bản chất là Ghi đè Tombstone)
bool BitcaskEngine::del(const std::string& key) {
    size_t dummy_offset;
    
    // Nếu nó không tồn tại, khỏi mất công xóa
    if (!key_dir_.get(key, dummy_offset)) {
        return false; 
    }

    file_.clear();
    file_.seekp(0, std::ios::end);

    utils::ByteBuffer record;
    
    // --- GHI RECORD RÁC ĐỂ LẤP ---
    record.push_back(1); // Tombstone = 1 (BÁO TỬ)

    // KeySize (4 bytes)
    uint32_t k_len = static_cast<uint32_t>(key.length());
    record.push_back(static_cast<uint8_t>(k_len & 0xFF));
    record.push_back(static_cast<uint8_t>((k_len >> 8) & 0xFF));
    record.push_back(static_cast<uint8_t>((k_len >> 16) & 0xFF));
    record.push_back(static_cast<uint8_t>((k_len >> 24) & 0xFF));

    // ValueSize = 0 (Đã xóa thì không có Value)
    record.push_back(0);
    record.push_back(0);
    record.push_back(0);
    record.push_back(0);

    // Raw Key
    for (char c : key) {
        record.push_back(static_cast<uint8_t>(c));
    }
    // Không ghi Raw Value vì size = 0

    // Đẩy xuống đĩa cứng
    file_.write(reinterpret_cast<const char*>(record.data()), record.size());
    file_.flush();

    // Bước quan trọng nhất: XÓA khỏi sổ RAM!
    key_dir_.remove(key);

    return true;
}

} // namespace graphlite