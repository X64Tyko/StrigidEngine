#include <cstdint>

union EntityID {
    uint64_t Value;
    
    // The "Bit Slice" View
    struct {
        uint64_t Index      : 20; // 1 Million Entities (The Array Slot)
        uint64_t Generation : 16; // 65k Recycles (The Safety Lock)
        uint64_t TypeID     : 12; // 4k Code Classes (The OOP Bridge)
        uint64_t OwnerID    : 8;  // 256 Unique Owners (Network Routing)
        uint64_t MetaFlags  : 8;  // Extra room (Layer? System?)
    };

    // --- Helper Methods (Fast Register Math) ---

    // Is this entity owned by the server? (Owner 0)
    inline bool IsServer() const { return OwnerID == 0; }

    // Is this entity owned by the local client?
    inline bool IsLocal(uint8_t localClientID) const { return OwnerID == localClientID; }

    // Equality Check (Fastest possible comparison)
    inline bool operator==(const EntityID& other) const { return Value == other.Value; }
    inline bool operator!=(const EntityID& other) const { return Value != other.Value; }
    
    // The "Null" Handle
    static EntityID Invalid() { EntityID id; id.Value = 0; return id; }
};