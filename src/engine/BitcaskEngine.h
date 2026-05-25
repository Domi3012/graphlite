#pragma once
#include "IStorage.h"
#include "../utils/MiniVector.h"
#include "../utils/NumericHashMap.h" // Dùng đúng Bảng băm Số nguyên
#include <fstream>
#include <string>

namespace graphlite {

class BitcaskEngine : public IStorage {
private:
    std::fstream file_;
    std::string file_path_;
    
    // Cuốn sổ cái RAM: uint32_t (Node/Edge ID) -> size_t (File Offset)
    utils::NumericHashMap key_dir_;

    void loadIndex(); 

public:
    explicit BitcaskEngine(const std::string& file_path);
    ~BitcaskEngine() override;

    // Interface đã dùng số nguyên
    bool put(const uint32_t& key, const utils::MiniVector<uint8_t>& value) override;
    bool get(const uint32_t& key, utils::MiniVector<uint8_t>& value) override;
    bool del(const uint32_t& key) override;

    // Hàm cực kỳ quan trọng cho Bulk Load (Nạp 1.5 triệu dòng)
    void sync(); 
};

} // namespace graphlite