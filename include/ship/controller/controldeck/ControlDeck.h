#pragma once

#include <cstdint>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>

// Lean GameCube/Wii ControlDeck base. Upstream libultraship's ControlDeck owns the
// whole desktop input stack (SDL device managers, per-button mapping tables,
// keyboard/mouse routing, the input-editor UI). On console the concrete deck reads
// PAD/WPAD directly, so this base keeps only what the game touches through
// Context::GetControlDeck(): connected-port tracking, the pure-virtual WriteToPad,
// and the input-block flags the (stripped) UI would otherwise toggle.

namespace Ship {

// Inert controller-LED plumbing. Upstream exposes per-port Controller objects whose
// LED colour games set (Ocarina of Time tints it per tunic); GameCube/Wii pads have
// no addressable LED, so these accept and discard.
class ControllerLED {
  public:
    template <typename C> void SetLEDColor(C) {}
};

class ControllerRumble {
  public:
    // Upstream returns the per-device rumble mapping table; console rumble is fixed.
    std::unordered_map<std::string, std::shared_ptr<int>> GetAllRumbleMappings() { return {}; }
    void StartRumble() {}
    void StopRumble() {}
    template <typename T> void SetRumbleStrength(T) {}
};

class Controller {
  public:
    std::shared_ptr<ControllerRumble> GetRumble() {
        static auto sRumble = std::make_shared<ControllerRumble>();
        return sRumble;
    }

    std::shared_ptr<ControllerLED> GetLED() {
        static auto sLed = std::make_shared<ControllerLED>();
        return sLed;
    }
};

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

    // Upstream tracks hot-plugged SDL devices here; console ports are fixed.
    class ConnectedPhysicalDeviceManager {
      public:
        template <typename T> void RefreshConnectedPhysicalDevices(T) {}
        std::vector<int> GetConnectedPhysicalDeviceIndices() { return {}; }
        // Upstream lists the SDL gamepads bound to a port; console ports are the pads
        // themselves, so this is empty and callers treat the port as unbound.
        std::vector<int> GetConnectedSDLGamepadsForPort(int32_t) { return {}; }
    };
    std::shared_ptr<ConnectedPhysicalDeviceManager> GetConnectedPhysicalDeviceManager() {
        static auto sMgr = std::make_shared<ConnectedPhysicalDeviceManager>();
        return sMgr;
    }

    std::shared_ptr<Controller> GetControllerByPort(uint8_t port) {
        (void)port;
        static auto sController = std::make_shared<Controller>();
        return sController;
    }

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
