// =============================================================================
// Draw/WorldDrawer.hpp
//
// Draws what the camera sees of one surface: visits the chunks in view, has
// ChunkDrawer draw their ground, and hands each entity to the drawer for its
// type. The game calls it once per frame:
//
//   void on_draw(Engine& engine) override {
//       DrawContext ctx = DrawContext::make(*engine.renderer2d(), engine.world(),
//                                           *world_camera_,
//                                           DrawContext::projection(debug_flags_),
//                                           debug_flags_);
//       world_drawer_->draw(ctx, *surface);
//   }
//
// This is the seam for a multithreaded draw stage: the chunks in view are
// independent, so they can be split across workers, each drawing its share
// into its own command queue. Every drawer is const and reads the world
// through const references, so they are safe to share between workers.
// =============================================================================

#pragma once
#include "Nightmourn/Draw/CharacterDrawer.hpp"
#include "Nightmourn/Draw/ChunkDrawer.hpp"
#include "Nightmourn/Draw/DrawContext.hpp"
#include "Nightmourn/Draw/ResourceDrawer.hpp"
#include "Nightmourn/Draw/UnitDrawer.hpp"
#include "Nightmourn/Draw/UnitGroupDrawer.hpp"

namespace Nezumi {
    class Entity;
    class Surface;
}

namespace Nightmourn {

class WorldDrawer {
public:

    // -------------------------------------------------------------------------
    // Config
    // -------------------------------------------------------------------------

    struct Config {
        ChunkDrawer::Config     chunks      = {};
        UnitDrawer::Config      units       = {};
        UnitGroupDrawer::Config unit_groups = {};
        CharacterDrawer::Config characters  = {};
        ResourceDrawer::Config  resources   = {};
    };

    // -------------------------------------------------------------------------
    // Construction & Destruction
    // -------------------------------------------------------------------------

    explicit WorldDrawer(const Config& config);

    // -------------------------------------------------------------------------
    // Draw
    // -------------------------------------------------------------------------

    void draw(DrawContext& ctx, const Nezumi::Surface& surface) const;

    // -------------------------------------------------------------------------
    // Drawers
    // -------------------------------------------------------------------------

    const ChunkDrawer&     chunks()      const { return chunks_;      }
    const UnitDrawer&      units()       const { return units_;       }
    const UnitGroupDrawer& unit_groups() const { return unit_groups_; }
    const CharacterDrawer& characters()  const { return characters_;  }
    const ResourceDrawer&  resources()   const { return resources_;   }

private:

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    // The drawer for the entity's type; entities with no drawer are skipped
    void draw_entity_(DrawContext& ctx, const Nezumi::Entity& entity) const;

    // UnitGroups near the view (ShowUnitGroupInfo)
    void draw_unit_groups_(DrawContext& ctx, const Nezumi::Surface& surface) const;

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    ChunkDrawer     chunks_;
    UnitDrawer      units_;
    UnitGroupDrawer unit_groups_;
    CharacterDrawer characters_;
    ResourceDrawer  resources_;
};

} // namespace Nightmourn
