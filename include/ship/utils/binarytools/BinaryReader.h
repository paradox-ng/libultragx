#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include "endianness.h"
#include "Stream.h"

namespace Ship {

// Sequential reader over a Stream with runtime-configurable byte order. Multi-byte
// reads are byte-swapped when the configured endianness differs from the target's
// native order (so on big-endian PPC, reading Big-endian data is swap-free).
class BinaryReader {
  public:
    BinaryReader(char* buffer, size_t bufferSize);
    BinaryReader(Stream* stream);
    BinaryReader(std::shared_ptr<Stream> stream);

    void Close();

    void SetEndianness(Endianness endianness);
    Endianness GetEndianness() const;

    void Seek(int32_t offset, SeekOffsetType seekType);
    uint32_t GetBaseAddress();

    void Read(int32_t length); // advance past `length` bytes
    void Read(char* buffer, int32_t length);

    char ReadChar();
    int8_t ReadInt8();
    int16_t ReadInt16();
    int32_t ReadInt32();
    int64_t ReadInt64();
    uint8_t ReadUByte();
    uint16_t ReadUInt16();
    uint32_t ReadUInt32();
    uint64_t ReadUInt64();
    float ReadFloat();
    double ReadDouble();

    std::string ReadString();  // uint32 length prefix, then bytes
    std::string ReadCString(); // bytes up to a NUL terminator

  private:
    std::shared_ptr<Stream> mStream;
    Endianness mEndianness = Endianness::Native;
};

} // namespace Ship
