#pragma once

#include <cstdint>
#include <unordered_map>

// Lean GameCube/Wii ControlDeck base. Upstream libultraship's ControlDeck owns the
// whole desktop input stack (SDL device managers, per-button mapping tables,
// keyboard/mouse routing, the input-editor UI). On console the concrete deck reads
// PAD/WPAD directly, so this base keeps only what the game touches through
// Context::GetControlDeck(): connected-port tracking, the pure-virtual WriteToPad,
// and the input-block flags the (stripped) UI would otherwise toggle.

namespace Ship {

class ControlDeck {
  public:
    virtual ~ControlDeck() = default;

    // controllerBits points at the byte the game uses as a bitmask of connected
    // ports; the deck sets/keeps it current.
    void Init(uint8_t* controllerBits);

    // Reads the connected controllers and writes their state into the game's pad
    // buffer (OSContPad[] on N64). Implemented by the concrete LUS::ControlDeck.
    virtual void WriteToPad(void* pad) = 0;

    uint8_t* GetControllerBits();

    // Input gating the dev UI uses; harmless no-ops kept so game code links.
    void BlockGameInput(int32_t blockId);
    void UnblockGameInput(int32_t blockId);
    bool GamepadGameInputBlocked();
    bool KeyboardGameInputBlocked();
    bool MouseGameInputBlocked();
    bool AllGameInputBlocked();

  protected:
    uint8_t* mControllerBits = nullptr;
    std::unordered_map<int32_t, bool> mGameInputBlockers;
};

} // namespace Ship
