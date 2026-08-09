#pragma once

#include <vector>
#include <cstdint>

#include "ship/controller/controldeck/ControlDeck.h"
#include "libultraship/libultra/controller.h"

// LUS::ControlDeck: the concrete N64-pad ControlDeck the game constructs
// (`std::make_shared<LUS::ControlDeck>()`) and reads via GetPads(). libultragx's
// version maps the GameCube pad / Wii remote straight to OSContPad in WriteToPad
// (no SDL device/mapping layer). The game obtains it from Context::GetControlDeck()
// and dynamic_casts to LUS::ControlDeck for GetPads().

namespace LUS {

class ControlDeck final : public Ship::ControlDeck {
  public:
    ControlDeck();
    // Upstream libultraship takes the game's list of valid buttons here (Shipwright
    // passes one from OTRGlobals). The console deck maps PAD/WPAD to a fixed N64
    // layout and has no per-button mapping tables, so the list is accepted and
    // ignored; the overload exists so those call sites compile unchanged.
    explicit ControlDeck(std::vector<uint16_t> validButtons);
    ~ControlDeck() override;

    // OSContPad buffer, one entry per port; refreshed by WriteToPad().
    OSContPad* GetPads();

    void WriteToPad(void* pad) override;

  private:
    OSContPad* mPads;
};

} // namespace LUS
