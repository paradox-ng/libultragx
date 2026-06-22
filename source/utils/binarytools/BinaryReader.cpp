#include "ship/utils/binarytools/BinaryReader.h"
#include "ship/utils/binarytools/MemoryStream.h"

#include <cstring>

namespace Ship {

BinaryReader::BinaryReader(char* buffer, size_t bufferSize) {
    mStream = std::make_shared<MemoryStream>(buffer, bufferSize);
}

BinaryReader::BinaryReader(Stream* stream) {
    // Non-owning: wrap with a no-op deleter so the caller keeps ownership.
    mStream = std::shared_ptr<Stream>(stream, [](Stream*) {});
}

BinaryReader::BinaryReader(std::shared_ptr<Stream> stream) : mStream(std::move(stream)) {}

void BinaryReader::Close() {
    if (mStream) mStream->Close();
}

void BinaryReader::SetEndianness(Endianness endianness) {
    mEndianness = endianness;
}

Endianness BinaryReader::GetEndianness() const {
    return mEndianness;
}

void BinaryReader::Seek(int32_t offset, SeekOffsetType seekType) {
    mStream->Seek(offset, seekType);
}

uint32_t BinaryReader::GetBaseAddress() {
    return (uint32_t)mStream->GetBaseAddress();
}

void BinaryReader::Read(int32_t length) {
    mStream->Seek(length, SeekOffsetType::Current);
}

void BinaryReader::Read(char* buffer, int32_t length) {
    mStream->Read(buffer, (size_t)length);
}

char BinaryReader::ReadChar() {
    return (char)mStream->ReadByte();
}

int8_t BinaryReader::ReadInt8() {
    return mStream->ReadByte();
}

uint8_t BinaryReader::ReadUByte() {
    return (uint8_t)mStream->ReadByte();
}

uint16_t BinaryReader::ReadUInt16() {
    uint16_t v;
    mStream->Read((char*)&v, sizeof(v));
    if (mEndianness != Endianness::Native) v = BSWAP16(v);
    return v;
}

int16_t BinaryReader::ReadInt16() {
    return (int16_t)ReadUInt16();
}

uint32_t BinaryReader::ReadUInt32() {
    uint32_t v;
    mStream->Read((char*)&v, sizeof(v));
    if (mEndianness != Endianness::Native) v = BSWAP32(v);
    return v;
}

int32_t BinaryReader::ReadInt32() {
    return (int32_t)ReadUInt32();
}

uint64_t BinaryReader::ReadUInt64() {
    uint64_t v;
    mStream->Read((char*)&v, sizeof(v));
    if (mEndianness != Endianness::Native) v = BSWAP64(v);
    return v;
}

int64_t BinaryReader::ReadInt64() {
    return (int64_t)ReadUInt64();
}

float BinaryReader::ReadFloat() {
    uint32_t u = ReadUInt32();
    float f;
    memcpy(&f, &u, sizeof(f));
    return f;
}

double BinaryReader::ReadDouble() {
    uint64_t u = ReadUInt64();
    double d;
    memcpy(&d, &u, sizeof(d));
    return d;
}

std::string BinaryReader::ReadString() {
    uint32_t len = ReadUInt32();
    std::string s;
    s.resize(len);
    if (len > 0) {
        mStream->Read(&s[0], len);
    }
    return s;
}

std::string BinaryReader::ReadCString() {
    std::string s;
    char c;
    while ((c = (char)mStream->ReadByte()) != '\0') {
        s.push_back(c);
    }
    return s;
}

} // namespace Ship
