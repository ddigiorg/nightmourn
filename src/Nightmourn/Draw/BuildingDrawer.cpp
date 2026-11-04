// =============================================================================
// Draw/BuildingDrawer.cpp
// =============================================================================

#include "Nightmourn/Draw/BuildingDrawer.hpp"
#include "Nezumi/Render/2D/Renderer2D.hpp"
#include "Nezumi/Render/Core/Color/Rgba8.hpp"
#include "Nightmourn/Draw/DrawLayers.hpp"
#include "Nightmourn/Draw/DrawUtils.hpp"
#include <cassert> // assert

namespace Nightmourn {

namespace NR = Nezumi::Render;

// Healthbar: this tall and this far above the picture, tiles
static constexpr float HEALTHBAR_HEIGHT = 0.15f;
static constexpr float HEALTHBAR_GAP    = 0.10f;
static constexpr float HEALTHBAR_BORDER = 0.04f;
static constexpr float HEALTHBAR_MAX_W  = 1.5f;

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

    draw_sprite_at(ctx, *sheet_, frames->first + frame, anchor, false,
                   faction_color(building.factions_mask));

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
