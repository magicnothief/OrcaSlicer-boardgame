#include <catch2/catch_all.hpp>

#include <algorithm>
#include <cmath>
#include <set>
#include <vector>

#include "libslic3r/ExPolygon.hpp"
#include "libslic3r/Fill/Lightning/Ribs.hpp"
#include "libslic3r/libslic3r.h"

using namespace Slic3r;
using namespace Slic3r::FillLightning;

namespace {

RibParams rib_params(double rib_spacing, double tie_spacing, double angle = 0.)
{
    RibParams params;
    params.rib_spacing = scaled<coord_t>(rib_spacing);
    params.tie_spacing = scaled<coord_t>(tie_spacing);
    params.angle       = angle;
    params.min_length  = scaled<coord_t>(0.9);
    return params;
}

ExPolygon rectangle(double x0, double y0, double x1, double y1)
{
    return ExPolygon(Polygon{ Point::new_scale(x0, y0), Point::new_scale(x1, y0), Point::new_scale(x1, y1), Point::new_scale(x0, y1) });
}

bool is_vertical(const Polyline &rib) { return rib.first_point().x() == rib.last_point().x(); }
bool is_horizontal(const Polyline &rib) { return rib.first_point().y() == rib.last_point().y(); }

} // namespace

TEST_CASE("Ribs form a square grid at the rib spacing where the infill is wide", "[Fill][RibbedLightning]")
{
    // 61 mm across, so the lattice lines at -16, 0 and 16 mm cross it and the ones at +-32 mm do not.
    const Polylines ribs = generate_ribs({ rectangle(-30.5, -30.5, 30.5, 30.5) }, rib_params(16., 8.));

    std::set<coord_t> xs, ys;
    for (const Polyline &rib : ribs) {
        REQUIRE(rib.size() == 2);
        CHECK_THAT(unscaled(rib.length()), Catch::Matchers::WithinAbs(61., 0.01));
        if (is_vertical(rib))
            xs.insert(rib.first_point().x());
        else {
            REQUIRE(is_horizontal(rib));
            ys.insert(rib.first_point().y());
        }
    }
    // The lines of the finer tie lattice in between are left out.
    const std::set<coord_t> lattice { scaled<coord_t>(-16.), 0, scaled<coord_t>(16.) };
    CHECK(ribs.size() == 6);
    CHECK(xs == lattice);
    CHECK(ys == lattice);
}

TEST_CASE("Ribs keep clear of the walls running alongside them", "[Fill][RibbedLightning]")
{
    // The lattice lines at +-32 mm run 1 mm inside the edges, closer than a quarter of the rib spacing.
    const Polylines ribs = generate_ribs({ rectangle(-33., -33., 33., 33.) }, rib_params(16., 8.));

    std::set<coord_t> xs, ys;
    for (const Polyline &rib : ribs) {
        if (is_vertical(rib))
            xs.insert(rib.first_point().x());
        else
            ys.insert(rib.first_point().y());
    }
    const std::set<coord_t> lattice { scaled<coord_t>(-16.), 0, scaled<coord_t>(16.) };
    CHECK(xs == lattice);
    CHECK(ys == lattice);
}

TEST_CASE("Ribs cross a narrow wall at the tie spacing and leave out the lines along it", "[Fill][RibbedLightning]")
{
    // A tie spacing of 7 mm is rounded to 8 mm, half of the rib spacing.
    const double tie_spacing = GENERATE(8., 7.);
    // 6.4 mm wide and 81 mm long, with the lattice line x = 0 running along its middle.
    const Polylines ribs = generate_ribs({ rectangle(-3.2, -40.5, 3.2, 40.5) }, rib_params(16., tie_spacing));

    std::vector<double> ys;
    for (const Polyline &rib : ribs) {
        REQUIRE(is_horizontal(rib));
        CHECK_THAT(unscaled(rib.length()), Catch::Matchers::WithinAbs(6.4, 0.01));
        ys.push_back(unscaled(rib.first_point().y()));
    }
    std::sort(ys.begin(), ys.end());
    // Every 8 mm from -40 to 40 mm.
    REQUIRE(ys.size() == 11);
    for (size_t i = 1; i < ys.size(); ++i)
        CHECK_THAT(ys[i] - ys[i - 1], Catch::Matchers::WithinAbs(8., 1e-6));
}

TEST_CASE("Ribs are not placed along the edge of the infill", "[Fill][RibbedLightning]")
{
    // The lattice lines x = 0, x = 40 and y = 0 lie on the edges of this narrow area.
    const Polylines ribs = generate_ribs({ rectangle(0., 0., 40., 6.) }, rib_params(16., 8.));

    std::set<coord_t> xs;
    for (const Polyline &rib : ribs) {
        REQUIRE(is_vertical(rib));
        xs.insert(rib.first_point().x());
    }
    CHECK(xs == std::set<coord_t>{ scaled<coord_t>(8.), scaled<coord_t>(16.), scaled<coord_t>(24.), scaled<coord_t>(32.) });
}

TEST_CASE("Ribs follow the direction of the lattice", "[Fill][RibbedLightning]")
{
    const double angle = GENERATE(0.3, -1.1);
    ExPolygon wall = rectangle(-3.2, -40.5, 3.2, 40.5);
    wall.rotate(angle);
    const Polylines ribs = generate_ribs({ wall }, rib_params(16., 8., angle));

    // The wall turned with the lattice, so its ties still cross it square on.
    REQUIRE(ribs.size() == 11);
    const Vec2d across = Vec2d(std::cos(angle), std::sin(angle));
    for (const Polyline &rib : ribs) {
        const Vec2d direction = (rib.last_point() - rib.first_point()).cast<double>().normalized();
        CHECK_THAT(std::abs(direction.dot(across)), Catch::Matchers::WithinAbs(1., 1e-6));
        CHECK_THAT(unscaled(rib.length()), Catch::Matchers::WithinAbs(6.4, 0.01));
    }
}
