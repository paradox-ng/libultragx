#include "ship/utils/binarytools/BinaryWriter.h"
#include "ship/utils/binarytools/MemoryStream.h"

#include <cstring>

namespace Ship {

BinaryWriter::BinaryWriter() {
    mStream = std::make_shared<MemoryStream>();
}

BinaryWriter::BinaryWriter(Stream* stream) {
    mStream = std::shared_ptr<Stream>(stream, [](Stream*) {});
}

BinaryWriter::BinaryWriter(std::shared_ptr<Stream> stream) : mStream(std::move(stream)) {}

void BinaryWriter::SetEndianness(Endianness endianness) {
    mEndianness = endianness;
}

uint64_t BinaryWriter::GetBaseAddress() {
    return mStream->GetBaseAddress();
}

uint64_t BinaryWriter::GetLength() {
    return mStream->GetLength();
}

void BinaryWriter::Seek(int32_t offset, SeekOffsetType seekType) {
    mStream->Seek(offset, seekType);
}

void BinaryWriter::Close() {
    mStream->Close();
}

void BinaryWriter::Write(int8_t value) {
    mStream->WriteByte(value);
}

void BinaryWriter::Write(uint8_t value) {
    mStream->WriteByte((int8_t)value);
}

void BinaryWriter::Write(uint16_t value) {
    if (mEndianness != Endianness::Native) value = BSWAP16(value);
    mStream->Write((char*)&value, sizeof(value));
}

void BinaryWriter::Write(int16_t value) {
    Write((uint16_t)value);
}

void BinaryWriter::Write(uint32_t value) {
    if (mEndianness != Endianness::Native) value = BSWAP32(value);
    mStream->Write((char*)&value, sizeof(value));
}

void BinaryWriter::Write(int32_t value) {
    Write((uint32_t)value);
}

void BinaryWriter::Write(int32_t valueA, int32_t valueB) {
    Write(valueA);
    Write(valueB);
}

void BinaryWriter::Write(uint64_t value) {
    if (mEndianness != Endianness::Native) value = BSWAP64(value);
    mStream->Write((char*)&value, sizeof(value));
}

void BinaryWriter::Write(int64_t value) {
    Write((uint64_t)value);
}

void BinaryWriter::Write(float value) {
    uint32_t u;
    memcpy(&u, &value, sizeof(u));
    Write(u);
}

void BinaryWriter::Write(double value) {
    uint64_t u;
    memcpy(&u, &value, sizeof(u));
    Write(u);
}

void BinaryWriter::Write(const std::string& str) {
    Write((uint32_t)str.size());
    if (!str.empty()) {
        mStream->Write((char*)str.data(), str.size());
    }
}

void BinaryWriter::Write(char* srcBuffer, size_t length) {
    mStream->Write(srcBuffer, length);
}

std::shared_ptr<Stream> BinaryWriter::GetStream() {
    return mStream;
}

std::vector<char> BinaryWriter::ToVector() {
    return mStream->ToVector();
}

} // namespace Ship
