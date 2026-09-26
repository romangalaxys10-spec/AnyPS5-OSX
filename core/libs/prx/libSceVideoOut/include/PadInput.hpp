#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_PADINPUT_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_PADINPUT_HPP

#include "SDL_events.h"
#include "prx/libScePad/include/InputMapping.hpp"
#include <array>
#include <chrono>

class DisplayWindow;

class PadInput {
public:
    void HandleEvent(const SDL_Event& event, DisplayWindow& window);
    void Update();

private:
    void publish();
    void setMouseMode(bool enabled);
    std::array<bool, Pad::InputMapping.size()> pressed{};
    std::array<std::chrono::steady_clock::time_point, Pad::InputMapping.size()> wheelReleaseTimes{};
    std::array<std::uint8_t, 2> mouseStick{128, 128};
    std::chrono::steady_clock::time_point nextMousePoll{};
    bool mouseEnabled = false;
};

#endif
