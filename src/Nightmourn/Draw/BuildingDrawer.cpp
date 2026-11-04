// =============================================================================
// Draw/BuildingDrawer.cpp
// =============================================================================

#include "Nightmourn/Draw/BuildingDrawer.hpp"
#include "Nezumi/Render/2D/Renderer2D.hpp"
#include "Nezumi/Render/Core/Color/Rgba8.hpp"
#include "Nightmourn/Draw/DrawLayers.hpp"
#include "Nightmourn/Draw/DrawUtils.hpp"
#include <algorithm> // clamp, max, min
#include <array>     // array
#include <cassert>   // assert
#include <cmath>     // ceil
#include <limits>    // numeric_limits

namespace Nightmourn {

namespace NR = Nezumi::Render;

// Healthbar: this tall and this far above the picture, tiles
static constexpr float HEALTHBAR_HEIGHT = 0.15f;
static constexpr float HEALTHBAR_GAP    = 0.10f;
static constexpr float HEALTHBAR_BORDER = 0.04f;
static constexpr float HEALTHBAR_MAX_W  = 1.5f;

// Big buildings are drawn as vertical strips this wide (view units), each
// sorted by its own footprint depth (see draw_sliced_)
static constexpr float STRIP_WIDTH = 0.25f;

// View y of the footprint's front (lower) edge at view x `u`: the lower
// outline of the projected footprint `c`, clamped to its ends
static float front_y(const std::array<glm::vec2, 4>& c, float u) {
    float lo = c[0].x, hi = c[0].x;
    for (const glm::vec2& p : c) { lo = std::min(lo, p.x); hi = std::max(hi, p.x); }
    u = std::clamp(u, lo, hi);

    float y = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < c.size(); i++) {
        const glm::vec2 a = c[i], b = c[(i + 1) % c.size()];
        if (a.x == b.x) {
            if (a.x == u) y = std::min({ y, a.y, b.y });
            continue;
        }
        if (u < std::min(a.x, b.x) || u > std::max(a.x, b.x)) continue;
        y = std::min(y, a.y + (b.y - a.y) * (u - a.x) / (b.x - a.x));
    }
    return y;
}

// Where a picture's pivot goes: the footprint's center
static glm::vec2 anchor_of(const TileRect& footprint) {
    return footprint.center().to_vec2();
}

// -----------------------------------------------------------------------------
// Construction & Destruction
// -----------------------------------------------------------------------------

BuildingDrawer::BuildingDrawer(const Config& config)
    : sheet_(config.sheet)
    , faction_colors_(config.faction_colors)
{
    assert(sheet_ && "BuildingDrawer: config.sheet is null");
    pictures_.resize(config.pictures.size());
    for (std::size_t i = 0; i < pictures_.size(); i++) {
        const Picture& p = config.pictures[i];
        if (p.animation.empty()) continue;
        const NR::AnimationID* id = sheet_->find_animation(p.animation.c_str());
        if (!id) continue;
        const NR::Animation& a = sheet_->anim(*id);
        pictures_[i] = { a.first, a.frames * a.directions, p.top };
    }

    if (!config.fill_sprite.empty()) {
        if (const NR::SpriteID* id = sheet_->find_sprite(config.fill_sprite.c_str())) {
            // The middle of the sprite, clear of its edges' filtering
            const NR::SpriteRegion& r = sheet_->region(*id);
            const glm::vec2 c = { (r.tex_box.x + r.tex_box.z) * 0.5f, (r.tex_box.y + r.tex_box.w) * 0.5f };
            const glm::vec2 h = { (r.tex_box.z - r.tex_box.x) * 0.25f, (r.tex_box.w - r.tex_box.y) * 0.25f };
            fill_tex_   = { c - h, c + h };
            fill_layer_ = r.layer;
            has_fill_   = true;
        }
    }
}

void BuildingDrawer::fill_ground(DrawContext& ctx, const Area2D& box, glm::vec4 color, std::uint8_t layer) const {
    if (!has_fill_) return;
    NR::DrawParams params = ctx.layer(layer);
    params.material = sheet_->material();
    ctx.renderer.draw_quad(
        NR::Quad{
            .corners = ctx.proj.box_to_view(box.to_vec4()),
            .tex_box = fill_tex_,
            .tint    = color,
            .layer   = fill_layer_,
        },
        params);
}

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

const BuildingDrawer::Frames* BuildingDrawer::frames_(const BuildingConfig& config) const {
    if (config.index >= pictures_.size() || pictures_[config.index].count == 0) return nullptr;
    return &pictures_[config.index];
}

glm::vec4 BuildingDrawer::faction_color(EntityFactionsMask factions) const {
    if (factions == EntityFactionsMask::None) return glm::vec4(1.0f);
    const glm::vec4 c = faction_colors_[faction_index(factions)];
    return c.a > 0.0f ? c : glm::vec4(1.0f);
}

void BuildingDrawer::draw(DrawContext& ctx, const Building& building) const {
    const Frames* frames = frames_(building.config());
    if (!frames) return;

    const TileRect  footprint = building.footprint();
    const glm::vec2 anchor    = anchor_of(footprint);
    const std::uint32_t frame = building.config().connects ? (building.connections % frames->count) : 0u;

    const glm::vec4 color = faction_color(building.factions_mask);
    if (ctx.proj.is_top_down()) {
        // Sorted by the footprint's bottom edge: a figure south of it draws
        // over it, one north of it under it. One sprite, whatever the size.
        NR::DrawParams params = ctx.layer(DrawLayer::Entities);
        params.material = sheet_->material();
        params.order    = y_sort_order(static_cast<float>(footprint.min.y));
        const float scale = ctx.upright_scale() / SPRITE_PIXELS_PER_TILE;
        NR::Sprite s = sheet_->sprite_at(frames->first + frame, ctx.to_view(anchor), scale);
        s.tint = NR::rgba8(color);
        ctx.renderer.draw_sprite(s, params);
    } else if (footprint.width() > 1 || footprint.height() > 1) {
        draw_sliced_(ctx, frames->first + frame, footprint, color);
    } else {
        draw_sprite_at(ctx, *sheet_, frames->first + frame, anchor, false, color);
    }

    // Healthbar over the picture, as wide as the footprint (up to a limit)
    if (!building.is_full_health()) {
        const float     scale = ctx.upright_scale();
        const glm::vec2 a     = ctx.to_view(anchor);
        const float     half  = std::min(static_cast<float>(footprint.width()) * 0.4f, HEALTHBAR_MAX_W * 0.5f) * scale;
        const float     y0    = a.y + (frames->top + HEALTHBAR_GAP) * scale;
        const float     y1    = y0 + HEALTHBAR_HEIGHT * scale;
        const float     b     = HEALTHBAR_BORDER * scale;

        const glm::vec4 back = { a.x - half, y0, a.x + half, y1 };
        glm::vec4 fill = back + glm::vec4{ b, b, -b, -b };
        fill.z = fill.x + (fill.z - fill.x) * building.health_ratio();

        const NR::DrawParams params = ctx.layer(DrawLayer::EntityOverlay);
        ctx.renderer.draw_rectangle(NR::Rectangle{ .pos_box = back, .color = { 0.0f, 0.0f, 0.0f, 1.0f } }, params);
        ctx.renderer.draw_rectangle(NR::Rectangle{ .pos_box = fill, .color = { 1.0f, 1.0f, 1.0f, 1.0f } }, params);
    }

    if (ctx.show(DebugFlag::ShowCollisionRectangles)) draw_collision_box(ctx, building);
}

// Footprint depth: a figure is y-sorted by its feet, and a big building by
// one point can only be right for figures on one side of that point. So the
// picture is cut into narrow vertical strips, each sorted by the footprint's
// front edge in its column (the highest point of that edge across the
// strip). A figure behind the building (its feet above the back edge) draws
// before every strip; one in front draws after the strips it stands in
// front of. It is the CPU version of a per-column footprint depth.
void BuildingDrawer::draw_sliced_(DrawContext& ctx, NR::SpriteID sprite, const TileRect& footprint,
                                  glm::vec4 color) const
{
    NR::DrawParams params = ctx.layer(DrawLayer::Entities);
    params.material = sheet_->material();

    const float scale = ctx.upright_scale() / SPRITE_PIXELS_PER_TILE;
    NR::Sprite s = sheet_->sprite_at(sprite, ctx.to_view(anchor_of(footprint)), scale);
    s.tint = NR::rgba8(color);

    const std::array<glm::vec2, 4> c = ctx.proj.box_to_view(footprint.bounds().to_vec4());

    const glm::vec4 pos = s.pos_box, tex = s.tex_box;
    const float     w   = pos.z - pos.x;
    const int       n   = std::clamp(static_cast<int>(std::ceil(w / STRIP_WIDTH)), 1, 128);

    for (int i = 0; i < n; i++) {
        const float t0 = static_cast<float>(i) / static_cast<float>(n);
        const float t1 = static_cast<float>(i + 1) / static_cast<float>(n);
        const float x0 = pos.x + w * t0, x1 = pos.x + w * t1;

        // The edge's highest point over the strip: at an end, or at a
        // corner inside it
        float y = std::max(front_y(c, x0), front_y(c, x1));
        for (const glm::vec2& p : c)
            if (p.x > x0 && p.x < x1) y = std::max(y, front_y(c, p.x));

        NR::Sprite strip = s;
        strip.pos_box = { x0, pos.y, x1, pos.w };
        strip.tex_box = { tex.x + (tex.z - tex.x) * t0, tex.y, tex.x + (tex.z - tex.x) * t1, tex.w };
        params.order  = y_sort_order(y);
        ctx.renderer.draw_sprite(strip, params);
    }
}

void BuildingDrawer::draw_ghost(DrawContext& ctx, const BuildingConfig& config, TileCoord origin,
                                glm::vec4 tint, std::uint8_t connections) const
{
    const TileRect footprint = TileRect::from_size(origin, config.footprint);

    // The footprint, filled faintly and outlined, over the lighting
    fill_ground(ctx, footprint.bounds(), { tint.r, tint.g, tint.b, tint.a * 0.35f }, DrawLayer::EntityOverlay);
    draw_outline(ctx, footprint.bounds(), { tint.r, tint.g, tint.b, 1.0f }, 2.0f, DrawLayer::EntityOverlay);

    const Frames* frames = frames_(config);
    if (!frames) return;

    // The picture, over everything so it's never hidden behind a tree
    const std::uint32_t frame = config.connects ? (connections % frames->count) : 0u;
    NR::DrawParams params = ctx.layer(DrawLayer::EntityOverlay);
    params.material = sheet_->material();
    const float scale = ctx.upright_scale() / SPRITE_PIXELS_PER_TILE;
    NR::Sprite s = sheet_->sprite_at(frames->first + frame, ctx.to_view(anchor_of(footprint)), scale);
    s.tint = NR::rgba8(tint);
    ctx.renderer.draw_sprite(s, params);
}

} // namespace Nightmourn
