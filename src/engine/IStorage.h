#pragma once
#include <string>
#include <cstdint>
#include <graphlite/MiniVector.h>

namespace graphlite {

class IStorage {
public:
    virtual ~IStorage() = default;
    
    // Graph Database chỉ được phép gọi 3 hàm này
    virtual bool put(const uint32_t& key, const utils::MiniVector<uint8_t>& value) = 0;
    virtual bool get(const uint32_t& key, utils::MiniVector<uint8_t>& value) = 0;
    virtual bool del(const uint32_t& key) = 0;
};

} // namespace graphlite