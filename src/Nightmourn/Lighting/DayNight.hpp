// =============================================================================
// Lighting/DayNight.hpp
//
// The time of day, on world ticks, and the ambient light it gives. Night is
// meant to be dangerous: the ambient falls to near black, so outside a light
// the player sees only shapes. Tune `night` up if it is too oppressive.
//
//   time 0.00  midnight     0.25 dawn     0.50 noon     0.75 dusk
//
//   day_night.tick();                              // once per world tick
//   lighting.update(day_night.ambient(), debug);   // once per frame
//
// The ambient is LINEAR light (see Nezumi::Render::PostProcess): 1 shows the
// art as drawn, 0.02 is a moonless night.
// =============================================================================

#pragma once
#include "Nezumi/Core/Tick.hpp"
#include <cstddef>     // size_t
#include <glm/glm.hpp> // vec3

namespace Nightmourn {

struct DayNightConfig {
    Nezumi::Tick day_ticks = 60 * 60 * 12; // a whole day: 12 minutes at 60 ticks/s
    float        start     = 0.5f;          // time of day at construction (noon)

    // Ambient keys (linear)
    glm::vec3 day   = { 1.000f, 1.000f, 1.000f };
    glm::vec3 dusk  = { 0.620f, 0.360f, 0.240f }; // also dawn
    glm::vec3 night = { 0.012f, 0.015f, 0.034f };

    // Lengths, as fractions of a day: the full night either side of
    // midnight, and each dusk/dawn transition
    float night_half = 0.21f; // night: 0.79 .. 0.21
    float twilight   = 0.12f; // night -> dawn color -> day over this

    // Lights' strength at noon (light_strength): torches barely show by day
    float day_light  = 0.15f;
};

class DayNight {
public:

    explicit DayNight(const DayNightConfig& config = {});

    // Advances one world tick (times `speed`)
    void tick();

    // Time of day: 0 midnight, 0.5 noon; wraps
    float get_time() const { return time_; }
    void  set_time(float time);

    // Multiplier on how fast time passes: 0 pauses
    float get_speed() const { return speed_; }
    void  set_speed(float speed) { speed_ = speed < 0.0f ? 0.0f : speed; }

    // The ambient light now (linear), and how dark it is: 0 noon, 1 midnight
    glm::vec3 ambient() const;
    float     darkness() const;
    bool      is_night() const { return darkness() > 0.5f; }

    // How much of their strength lights show: `day_light` at noon, 1 at
    // night (LightDrawer's strength)
    float light_strength() const { return config_.day_light + (1.0f - config_.day_light) * darkness(); }

    // "HH:MM" of a 24-hour clock
    void format_clock(char* out, std::size_t size) const;

    const DayNightConfig& config() const { return config_; }

private:
    DayNightConfig config_;
    float          time_  = 0.5f;
    float          speed_ = 1.0f;
};

} // namespace Nightmourn
