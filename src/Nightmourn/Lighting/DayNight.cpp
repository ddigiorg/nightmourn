// =============================================================================
// Lighting/DayNight.cpp
// =============================================================================

#include "Nightmourn/Lighting/DayNight.hpp"
#include <algorithm> // clamp
#include <cmath>     // floor, abs
#include <cstdio>    // snprintf

namespace Nightmourn {

namespace {
    float smooth(float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    float wrap(float t) { return t - std::floor(t); }

    float luminance(glm::vec3 c) { return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b; }
}

DayNight::DayNight(const DayNightConfig& config)
    : config_(config)
    , time_  (wrap(config.start))
{}

void DayNight::tick() {
    if (config_.day_ticks == 0) return;
    time_ = wrap(time_ + speed_ / static_cast<float>(config_.day_ticks));
}

void DayNight::set_time(float time) {
    time_ = wrap(time);
}

glm::vec3 DayNight::ambient() const {
    // Distance from midnight, 0 .. 0.5: symmetric around noon, so dawn
    // mirrors dusk
    const float d  = std::abs(time_ > 0.5f ? 1.0f - time_ : time_);
    const float t0 = config_.night_half;           // night ends
    const float t1 = t0 + config_.twilight * 0.5f; // dawn color peaks
    const float t2 = t0 + config_.twilight;        // day begins

    if (d <= t0) return config_.night;
    if (d <= t1) return glm::mix(config_.night, config_.dusk, smooth((d - t0) / (t1 - t0)));
    if (d <= t2) return glm::mix(config_.dusk,  config_.day,  smooth((d - t1) / (t2 - t1)));
    return config_.day;
}

float DayNight::darkness() const {
    const float day   = luminance(config_.day);
    const float night = luminance(config_.night);
    if (day <= night) return 0.0f;
    return std::clamp((day - luminance(ambient())) / (day - night), 0.0f, 1.0f);
}

void DayNight::format_clock(char* out, std::size_t size) const {
    const int minutes = static_cast<int>(time_ * 24.0f * 60.0f);
    std::snprintf(out, size, "%02d:%02d", (minutes / 60) % 24, minutes % 60);
}

} // namespace Nightmourn
