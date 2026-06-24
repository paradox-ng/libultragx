#pragma once

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
    ~ControlDeck() override;

    // OSContPad buffer, one entry per port; refreshed by WriteToPad().
    OSContPad* GetPads();

    void WriteToPad(void* pad) override;

  private:
    OSContPad* mPads;
};

} // namespace LUS
