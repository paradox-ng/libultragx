#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace Ship {

// A trusted-author record. Archive signing/trust is a desktop security feature;
// libultragx does not verify signatures, so this exists only to type the untrusted
// archive handler a game registers (which is never invoked on console).
struct KeystoreEntry {
    std::string Author;
    std::string PublicKey;
    std::vector<uint8_t> Key;
};

} // namespace Ship
