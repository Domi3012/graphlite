#pragma once
#include "IStorage.h"
#include "../utils/ByteBuffer.h"
#include "../utils/HashMap.h"
#include <fstream>
#include <string>

namespace graphlite {

class BitcaskEngine : public IStorage {
private:
    std::fstream file_;
    std::string file_path_;
    
    // Hash Map lưu cấu trúc: Key -> File Offset (Tọa độ trên đĩa)
    utils::HashMap key_dir_;

    // Hàm nội bộ để quét file xây lại index khi khởi động DB
    void loadIndex(); 

public:
    explicit BitcaskEngine(const std::string& file_path);
    ~BitcaskEngine() override;

    // Implement 3 hàm của IStorage
    bool put(const std::string& key, const utils::ByteBuffer& value) override;
    bool get(const std::string& key, utils::ByteBuffer& value) override;
    bool del(const std::string& key) override;
};

} // namespace graphlite