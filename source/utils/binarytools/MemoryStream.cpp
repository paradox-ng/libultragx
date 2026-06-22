#include "ship/utils/binarytools/MemoryStream.h"

#include <cstring>
#include <algorithm>

namespace Ship {

MemoryStream::MemoryStream() = default;

MemoryStream::MemoryStream(char* buffer, size_t size) {
    if (buffer != nullptr && size > 0) {
        mBuffer.assign(buffer, buffer + size);
    }
}

MemoryStream::~MemoryStream() = default;

uint64_t MemoryStream::GetLength() {
    return mBuffer.size();
}

void MemoryStream::Seek(int32_t offset, SeekOffsetType seekType) {
    switch (seekType) {
        case SeekOffsetType::Start:   mBaseAddress = (uint64_t)offset; break;
        case SeekOffsetType::Current: mBaseAddress += offset; break;
        case SeekOffsetType::End:     mBaseAddress = mBuffer.size() + offset; break;
    }
}

std::unique_ptr<char[]> MemoryStream::Read(size_t length) {
    auto out = std::make_unique<char[]>(length);
    Read(out.get(), length);
    return out;
}

void MemoryStream::Read(char* dest, size_t length) {
    size_t pos = (size_t)mBaseAddress;
    size_t avail = (pos < mBuffer.size()) ? (mBuffer.size() - pos) : 0;
    size_t n = std::min(length, avail);
    if (n > 0) {
        memcpy(dest, mBuffer.data() + pos, n);
    }
    if (n < length) {
        memset(dest + n, 0, length - n); // zero-fill past end rather than throw
    }
    mBaseAddress += length;
}

int8_t MemoryStream::ReadByte() {
    size_t pos = (size_t)mBaseAddress;
    if (pos >= mBuffer.size()) {
        mBaseAddress++;
        return 0;
    }
    mBaseAddress++;
    return (int8_t)mBuffer[pos];
}

void MemoryStream::Write(char* srcBuffer, size_t length) {
    size_t end = (size_t)mBaseAddress + length;
    if (end > mBuffer.size()) {
        mBuffer.resize(end);
    }
    memcpy(mBuffer.data() + (size_t)mBaseAddress, srcBuffer, length);
    mBaseAddress += length;
}

void MemoryStream::WriteByte(int8_t value) {
    size_t pos = (size_t)mBaseAddress;
    if (pos + 1 > mBuffer.size()) {
        mBuffer.resize(pos + 1);
    }
    mBuffer[pos] = (char)value;
    mBaseAddress++;
}

std::vector<char> MemoryStream::ToVector() {
    return mBuffer;
}

void MemoryStream::Flush() {}
void MemoryStream::Close() {}

} // namespace Ship
