#pragma once
#include <string>
#include <cstdint>
#include "../utils/ByteBuffer.h"

namespace graphlite {

class IStorage {
public:
    virtual ~IStorage() = default;
    
    // Graph Database chỉ được phép gọi 3 hàm này
    virtual bool put(const std::string& key, const utils::ByteBuffer& value) = 0;
    virtual bool get(const std::string& key, utils::ByteBuffer& value) = 0;
    virtual bool del(const std::string& key) = 0;
};

} // namespace graphlite