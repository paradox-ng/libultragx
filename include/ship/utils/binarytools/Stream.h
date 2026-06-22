#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace Ship {

// Reference point for a seek operation.
enum class SeekOffsetType {
    Start,
    Current,
    End,
};

// Abstract sequential byte stream. BinaryReader/BinaryWriter operate over any
// concrete Stream (e.g. MemoryStream). All ops advance the position cursor.
class Stream {
  public:
    virtual ~Stream() = default;

    virtual uint64_t GetLength() = 0;
    uint64_t GetBaseAddress() { return mBaseAddress; }

    virtual void Seek(int32_t offset, SeekOffsetType seekType) = 0;

    virtual std::unique_ptr<char[]> Read(size_t length) = 0;
    virtual void Read(char* dest, size_t length) = 0;
    virtual int8_t ReadByte() = 0;

    virtual void Write(char* srcBuffer, size_t length) = 0;
    virtual void WriteByte(int8_t value) = 0;

    virtual std::vector<char> ToVector() = 0;
    virtual void Flush() = 0;
    virtual void Close() = 0;

  protected:
    uint64_t mBaseAddress = 0; // current position (byte offset from start)
};

} // namespace Ship
