#pragma once

#include "Stream.h"

namespace Ship {

// In-memory Stream backed by a growable byte vector.
class MemoryStream final : public Stream {
  public:
    MemoryStream();
    MemoryStream(char* buffer, size_t size);
    ~MemoryStream() override;

    uint64_t GetLength() override;
    void Seek(int32_t offset, SeekOffsetType seekType) override;

    std::unique_ptr<char[]> Read(size_t length) override;
    void Read(char* dest, size_t length) override;
    int8_t ReadByte() override;

    void Write(char* srcBuffer, size_t length) override;
    void WriteByte(int8_t value) override;

    std::vector<char> ToVector() override;
    void Flush() override;
    void Close() override;

  private:
    std::vector<char> mBuffer;
};

} // namespace Ship
