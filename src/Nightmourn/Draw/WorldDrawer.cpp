// =============================================================================
// Draw/WorldDrawer.cpp
// =============================================================================

#include "Nightmourn/Draw/WorldDrawer.hpp"
#include "Nightmourn/Chunk/Chunk.hpp"
#include "Nightmourn/Chunk/ChunkSpan.hpp"
#include "Nightmourn/Resource/Resource.hpp"
#include "Nightmourn/Unit/Unit.hpp"
#include "Nightmourn/Unit/UnitGroup.hpp"
#include "Nightmourn/Surface/Surface.hpp"
#include "Nightmourn/Character/Character.hpp"

namespace Nightmourn {

// -----------------------------------------------------------------------------
// Construction & Destruction
// -----------------------------------------------------------------------------

WorldDrawer::WorldDrawer(const Config& config)
    : chunks_     (config.chunks)
    , units_      (config.units)
    , unit_groups_(config.unit_groups)
    , characters_ (config.characters)
    , resources_  (config.resources)
{}

// -----------------------------------------------------------------------------
// Draw
// -----------------------------------------------------------------------------

void WorldDrawer::draw(DrawContext& ctx, const Surface& surface) const {
    // Chunks in the world box of the view, minus the ones whose projected
    // bounds miss it (in iso the box's corners are off screen). The view is
    // grown by DrawContext::VIEW_OVERHANG, so figures standing in a skipped
    // chunk never reach into the screen. Inactive (sleeping) entities are
    // still on screen, so both lists.
    surface.each_chunk_in(ChunkSpan::span_from_box(ctx.world_view), [&](const Chunk& chunk) {
        const glm::vec4 b = ctx.proj.box_to_view_bounds(chunk.bounds().to_vec4());
        if (!Area2D(b.x, b.y, b.z, b.w).overlaps(ctx.view)) return;

        chunks_.draw(ctx, chunk);
        chunk.visit_entities([&](const Entity& entity) {
            if (entity.is_alive() && entity.is_drawable()) draw_entity_(ctx, entity);
            return true;
        });
    });

    if (ctx.show(DebugFlag::ShowUnitGroupInfo)) draw_unit_groups_(ctx, surface);
}

void WorldDrawer::draw_entity_(DrawContext& ctx, const Entity& entity) const {
    // TODO: a table of drawers indexed by EntityTypeID once there are more
    //       than a handful of types
    if      (entity.is_type<Unit    >()) units_     .draw(ctx, *entity.as_type<Unit    >());
    else if (entity.is_type<Character       >()) characters_.draw(ctx, *entity.as_type<Character       >());
    else if (entity.is_type<Resource>()) resources_ .draw(ctx, *entity.as_type<Resource>());
}

// Groups aren't drawable (no sprite), and one sits in its anchor's chunk
// while its members can stand in the next one over, so these get their own
// pass over a wider span, only while the overlay is on
void WorldDrawer::draw_unit_groups_(DrawContext& ctx, const Surface& surface) const {
    const Area2D area = Area2D(ctx.world_view).grow(UnitGroupDrawer::VIEW_MARGIN);

    surface.each_chunk_in(ChunkSpan::span_from_box(area), [&](const Chunk& chunk) {
        chunk.visit_entities([&](const Entity& entity) {
            if (entity.is_alive() && entity.is_type<UnitGroup>())
                unit_groups_.draw(ctx, *entity.as_type<UnitGroup>());
            return true;
        });
    });
}

} // namespace Nightmourn
