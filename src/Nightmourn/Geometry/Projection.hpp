// =============================================================================
// Geometry/Projection.hpp
//
// Maps world positions (tiles, square grid, y up) to view positions (what a
// world camera looks at, y up), and back. The world never sees it: the sim
// stays on the square grid, and only drawers, picking and culling go through
// the projection.
//
//   top_down()  identity: view space is world space
//   iso_2to1()  Age of Empires-style 2:1 dimetric: a tile is a diamond 2 view
//               units wide and 1 tall, and world +x+y points up the screen
//
// For iso:
//
//   view.x = x - y          world.x = view.y + view.x / 2
//   view.y = (x + y) / 2    world.y = view.y - view.x / 2
//
// so a CameraConfig::pixels_per_unit of 32 draws 64x32 px tiles.
//
// Drawers project on the CPU and submit in view space, so the renderer and
// camera stay plain 2D: upright things (figures, healthbars, text) are drawn
// screen-aligned at their projected anchor, and ground things (tiles, grid,
// footprints) project every corner. The map is linear, so straight world
// lines stay straight; projecting the endpoints of a line is exact.
//
//   const glm::vec2 feet = proj.to_view(unit.position.to_vec2());
//   const glm::vec2 tile = proj.to_world(camera.screen_to_world(mouse));
//
// Camera2D::screen_to_world returns a VIEW position when the world is drawn
// through a projection; to_world() turns it into a world position.
// =============================================================================

#pragma once
#include <array>       // array
#include <cmath>       // sqrt
#include <glm/glm.hpp> // vec2, vec4, mat2

namespace Nightmourn {

class Projection {
public:

    // -------------------------------------------------------------------------
    // Construction & Destruction
    // -------------------------------------------------------------------------

    // Top-down
    Projection() = default;

    static Projection top_down() { return Projection(); }

    static Projection iso_2to1() {
        // Columns: where world +x and world +y land in view space
        return Projection(glm::mat2{ { 1.0f, 0.5f }, { -1.0f, 0.5f } }, { 2.0f, 1.0f });
    }

    // -------------------------------------------------------------------------
    // Positions and Directions
    //
    // The map has no translation, so positions and directions share the
    // matrices. The dir_ functions are there to say which one is meant.
    // -------------------------------------------------------------------------

    glm::vec2 to_view (glm::vec2 world) const { return to_view_  * world; }
    glm::vec2 to_world(glm::vec2 view ) const { return to_world_ * view;  }

    glm::vec2 dir_to_view (glm::vec2 world_dir) const { return to_view_  * world_dir; }
    glm::vec2 dir_to_world(glm::vec2 view_dir ) const { return to_world_ * view_dir;  }

    // The 4 corners of a world box { min_x, min_y, max_x, max_y } in view
    // space: min-min, max-min, max-max, min-max (counter-clockwise from the
    // box's min corner, so a Quad's bottom-left first)
    std::array<glm::vec2, 4> box_to_view(glm::vec4 box) const {
        return {
            to_view({ box.x, box.y }),
            to_view({ box.z, box.y }),
            to_view({ box.z, box.w }),
            to_view({ box.x, box.w }),
        };
    }

    // Axis-aligned bounds { min_x, min_y, max_x, max_y } of a box mapped into
    // the other space. For culling: the view-space bounds of a world box, or
    // the world-space bounds of a view rectangle.
    glm::vec4 box_to_view_bounds (glm::vec4 world_box) const { return bounds_(to_view_,  world_box); }
    glm::vec4 box_to_world_bounds(glm::vec4 view_box ) const { return bounds_(to_world_, view_box ); }

    // The way a world direction faces in the art: its view direction with
    // the vertical squash undone, normalized (zero stays zero). Directions
    // 45 degrees apart in the world stay 45 degrees apart, which is how
    // pre-rendered figures are drawn, so pick a figure's frame from this
    // rather than from the on-screen angle. For iso it is the world
    // direction turned 45 degrees: world (-1, -1) faces screen down.
    glm::vec2 dir_to_heading(glm::vec2 world_dir) const {
        glm::vec2 v = dir_to_view(world_dir);
        v.y *= tile_size_.x / tile_size_.y;
        const float len2 = v.x * v.x + v.y * v.y;
        return len2 > 0.0f ? v / std::sqrt(len2) : v;
    }

    // -------------------------------------------------------------------------
    // Observers
    // -------------------------------------------------------------------------

    // View-space size of a tile's bounds: { 1, 1 } top-down, { 2, 1 } iso.
    // Upright things (figures, healthbars) scale by tile_size().x so they keep
    // their size next to the tiles they stand on.
    glm::vec2 tile_size() const { return tile_size_; }

    bool is_top_down() const { return to_view_ == glm::mat2(1.0f); }

    bool operator==(const Projection& other) const {
        return to_view_ == other.to_view_ && tile_size_ == other.tile_size_;
    }

private:

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    Projection(const glm::mat2& to_view, glm::vec2 tile_size)
        : to_view_  (to_view)
        , to_world_ (glm::inverse(to_view))
        , tile_size_(tile_size)
    {}

    static glm::vec4 bounds_(const glm::mat2& m, glm::vec4 box) {
        const glm::vec2 a = m * glm::vec2{ box.x, box.y };
        const glm::vec2 b = m * glm::vec2{ box.z, box.y };
        const glm::vec2 c = m * glm::vec2{ box.z, box.w };
        const glm::vec2 d = m * glm::vec2{ box.x, box.w };
        return { glm::min(glm::min(a, b), glm::min(c, d)),
                 glm::max(glm::max(a, b), glm::max(c, d)) };
    }

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------

    glm::mat2 to_view_    = glm::mat2(1.0f);
    glm::mat2 to_world_   = glm::mat2(1.0f);
    glm::vec2 tile_size_  = { 1.0f, 1.0f };
};

} // namespace Nightmourn
