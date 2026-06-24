#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "endianness.h"
#include "Stream.h"

namespace Ship {

// Sequential writer over a Stream with runtime-configurable byte order. Mirrors
// BinaryReader: multi-byte writes are byte-swapped when the configured endianness
// differs from the target's native order.
class BinaryWriter {
  public:
    BinaryWriter();                            // writes into an owned MemoryStream
    BinaryWriter(Stream* stream);
    BinaryWriter(std::shared_ptr<Stream> stream);

    void SetEndianness(Endianness endianness);
    uint64_t GetBaseAddress();
    uint64_t GetLength();
    void Seek(int32_t offset, SeekOffsetType seekType);
    void Close();

    void Write(int8_t value);
    void Write(uint8_t value);
    void Write(int16_t value);
    void Write(uint16_t value);
    void Write(int32_t value);
    void Write(int32_t valueA, int32_t valueB);
    void Write(uint32_t value);
    void Write(int64_t value);
    void Write(uint64_t value);
    void Write(float value);
    void Write(double value);
    void Write(const std::string& str); // uint32 length prefix, then bytes
    void Write(char* srcBuffer, size_t length);

    std::shared_ptr<Stream> GetStream();
    std::vector<char> ToVector(); // raw copy of everything written so far

  private:
    std::shared_ptr<Stream> mStream;
    Endianness mEndianness = Endianness::Native;
};

} // namespace Ship
