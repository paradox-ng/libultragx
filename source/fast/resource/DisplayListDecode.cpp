#include "fast/resource/DisplayListDecode.h"

// OTR double-width opcodes (high byte of w0): each is followed by an extra command
// word pair (a hash / data) that must also be consumed.
static bool is_otr_expanded(int8_t op) {
    return op == (int8_t)0x20 || // G_SETTIMG_OTR_HASH
           op == (int8_t)0x31 || // G_DL_OTR_HASH
           op == (int8_t)0x32 || // G_VTX_OTR_HASH
           op == (int8_t)0x33 || // G_MARKER
           op == (int8_t)0x35 || // G_BRANCH_Z_OTR
           op == (int8_t)0x36 || // G_MTX_OTR
           op == (int8_t)0x42;   // G_MOVEMEM_OTR
}

static int8_t end_opcode(UcodeHandlers u) {
    // F3DEX2 / S2DEX terminate with 0xDF; the F3D family with 0xB8.
    return (u == ucode_f3dex2 || u == ucode_s2dex) ? (int8_t)0xDF : (int8_t)0xB8;
}

std::shared_ptr<Fast::DisplayList>
lugx_read_display_list(std::shared_ptr<Ship::ResourceInitData> initData,
                       std::shared_ptr<Ship::BinaryReader> reader) {
    auto dl = std::make_shared<Fast::DisplayList>(initData);

    auto ucode = (UcodeHandlers)reader->ReadInt8();
    dl->UCode = ucode;
    while (reader->GetBaseAddress() % 8 != 0) {
        reader->ReadInt8();
    }

    const int8_t endOp = end_opcode(ucode);
    while (true) {
        Gfx command;
        command.words.w0 = reader->ReadUInt32();
        command.words.w1 = reader->ReadUInt32();
        int8_t opcode = (int8_t)(command.words.w0 >> 24);

        if (is_otr_expanded(opcode)) {
            dl->Instructions.push_back(command);
            command.words.w0 = reader->ReadUInt32();
            command.words.w1 = reader->ReadUInt32();
        }

        dl->Instructions.push_back(command);

        if (opcode == endOp) {
            break;
        }
        if (dl->Instructions.size() > 200000) { // safety: malformed / unterminated
            break;
        }
    }
    return dl;
}
