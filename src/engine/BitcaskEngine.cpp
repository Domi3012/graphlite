#include "../../include/graphlite/internal/BitcaskEngine.h"
#include <stdexcept>

namespace graphlite {

BitcaskEngine::BitcaskEngine(const std::string& file_path) 
    : file_path_(file_path), key_dir_(200000) {
    
    file_.open(file_path_, std::ios::in | std::ios::out | std::ios::app | std::ios::binary);
    
    if (!file_.is_open()) {
        file_.clear();
        file_.open(file_path_, std::ios::out | std::ios::binary); 
        file_.close();
        file_.open(file_path_, std::ios::in | std::ios::out | std::ios::app | std::ios::binary);
    }

    if (!file_.is_open()) {
        throw std::runtime_error("Cannot open or create database file!");
    }

    // loadIndex(); // Sẽ cài đặt sau để đọc file dựng lại cây RAM khi restart máy
}

BitcaskEngine::~BitcaskEngine() {
    sync(); // Đảm bảo an toàn khi tắt
    if (file_.is_open()) {
        file_.close();
    }
}

void BitcaskEngine::sync() {
    if (file_.is_open()) {
        file_.flush();
    }
}

bool BitcaskEngine::put(const uint32_t& key, const utils::MiniVector<uint8_t>& value) {
    file_.clear(); 
    file_.seekp(0, std::ios::end);
    size_t offset = file_.tellp(); 

    utils::MiniVector<uint8_t> record;
    
    // HEADER LAYOUT MỚI (9 bytes cố định)
    // 1. Tombstone (1 byte: 0 = Alive, 1 = Dead)
    record.push_back(0); 

    // 2. Key (4 bytes) - Thay vì độ dài key, ta ghi luôn cái Key (ID) xuống đĩa
    record.push_back(static_cast<uint8_t>(key & 0xFF));
    record.push_back(static_cast<uint8_t>((key >> 8) & 0xFF));
    record.push_back(static_cast<uint8_t>((key >> 16) & 0xFF));
    record.push_back(static_cast<uint8_t>((key >> 24) & 0xFF));

    // 3. Value Size (4 bytes)
    uint32_t v_len = static_cast<uint32_t>(value.size());
    record.push_back(static_cast<uint8_t>(v_len & 0xFF));
    record.push_back(static_cast<uint8_t>((v_len >> 8) & 0xFF));
    record.push_back(static_cast<uint8_t>((v_len >> 16) & 0xFF));
    record.push_back(static_cast<uint8_t>((v_len >> 24) & 0xFF));

    // 4. Raw Value (Payload của mảng sự kiện)
    for (size_t i = 0; i < value.size(); ++i) {
        record.push_back(value[i]);
    }

    // Ghi xuống bộ đệm OS (KHÔNG FLUSH!)
    file_.write(reinterpret_cast<const char*>(record.data()), record.size());

    // Cập nhật tọa độ lên sổ RAM
    key_dir_.put(key, offset);

    return true;
}

bool BitcaskEngine::get(const uint32_t& key, utils::MiniVector<uint8_t>& value) {
    size_t offset = 0;
    
    if (!key_dir_.get(key, offset)) {
        return false; 
    }

    file_.clear();
    file_.seekg(offset, std::ios::beg);

    // Đọc trọn Header 9 bytes 1 lần cho lẹ
    char header[9];
    file_.read(header, 9);
    
    if (header[0] == 1) { // Tombstone == 1
        return false; 
    }

    // Dịch bit lấy độ dài Value
    uint32_t v_len = static_cast<uint8_t>(header[5]) |
                    (static_cast<uint8_t>(header[6]) << 8) |
                    (static_cast<uint8_t>(header[7]) << 16) |
                    (static_cast<uint8_t>(header[8]) << 24);

    if (v_len > 0) {
        // Dọn dẹp vector đầu vào để tránh ghi nối đuôi
        value.clear(); 
        
        char* temp_buf = new char[v_len]; 
        file_.read(temp_buf, v_len);
        
        for (uint32_t i = 0; i < v_len; ++i) {
            value.push_back(static_cast<uint8_t>(temp_buf[i]));
        }
        delete[] temp_buf; 
    }

    return true;
}

bool BitcaskEngine::del(const uint32_t& key) {
    size_t dummy_offset;
    if (!key_dir_.get(key, dummy_offset)) {
        return false; 
    }

    file_.clear();
    file_.seekp(0, std::ios::end);

    utils::MiniVector<uint8_t> record;
    
    // Ghi Record Báo Tử (9 bytes Header rỗng)
    record.push_back(1); // Tombstone = 1
    
    // Ghi Key để loadIndex() sau này biết ai bị xóa
    record.push_back(static_cast<uint8_t>(key & 0xFF));
    record.push_back(static_cast<uint8_t>((key >> 8) & 0xFF));
    record.push_back(static_cast<uint8_t>((key >> 16) & 0xFF));
    record.push_back(static_cast<uint8_t>((key >> 24) & 0xFF));

    // ValueSize = 0
    record.push_back(0); record.push_back(0); record.push_back(0); record.push_back(0);

    file_.write(reinterpret_cast<const char*>(record.data()), record.size());
    // Lệnh xóa là giao dịch nguy hiểm, ta cho phép flush() cục bộ
    file_.flush(); 

    key_dir_.remove(key);

    return true;
}

} // namespace graphlite