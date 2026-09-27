#include "Ribs.hpp"

#include "../../BoundingBox.hpp"
#include "../../ClipperUtils.hpp"

#include <algorithm>
#include <cmath>

namespace Slic3r::FillLightning {

using Interval = std::pair<coord_t, coord_t>;

// Intervals of the vertical line x = c lying inside the polygons, sorted by y. The polygons must not
// overlap, so that the crossings alternate between entering and leaving them.
static std::vector<Interval> chords_at(const Polygons &polygons, coord_t c)
{
    std::vector<coord_t> ys;
    for (const Polygon &polygon : polygons)
        for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i ++) {
            const Point &p = polygon[j];
            const Point &q = polygon[i];
            // Half open, so that a vertex lying on the line is crossed once or not at all.
            if ((p.x() <= c) != (q.x() <= c))
                ys.push_back(p.y() + coord_t(std::llround(double(q.y() - p.y()) * double(c - p.x()) / double(q.x() - p.x()))));
        }
    std::sort(ys.begin(), ys.end());
    std::vector<Interval> out;
    for (size_t i = 0; i + 1 < ys.size(); i += 2)
        out.emplace_back(ys[i], ys[i + 1]);
    return out;
}

static bool contains(const std::vector<Interval> &intervals, coord_t y)
{
    return std::any_of(intervals.begin(), intervals.end(), [y](const Interval &interval) { return interval.first < y && y < interval.second; });
}

static coord_t overlap(const Interval &chord, const std::vector<Interval> &intervals)
{
    coord_t length = 0;
    for (const Interval &interval : intervals)
        length += std::max(coord_t(0), std::min(chord.second, interval.second) - std::max(chord.first, interval.first));
    return length;
}

// Where both sorted lists of intervals overlap.
static std::vector<Interval> intersect(const std::vector<Interval> &a, const std::vector<Interval> &b)
{
    std::vector<Interval> out;
    for (size_t i = 0, j = 0; i < a.size() && j < b.size();) {
        if (const coord_t lo = std::max(a[i].first, b[j].first), hi = std::min(a[i].second, b[j].second); lo < hi)
            out.emplace_back(lo, hi);
        if (a[i].second < b[j].second)
            ++ i;
        else
            ++ j;
    }
    return out;
}

static void transpose(Polygons &polygons)
{
    for (Polygon &polygon : polygons)
        for (Point &p : polygon.points)
            std::swap(p.x(), p.y());
}

Polylines generate_ribs(const ExPolygons &area, const RibParams &params)
{
    if (area.empty() || params.rib_spacing <= 0)
        return {};

    // The ribs are every k-th line of the tie lattice.
    const int     k              = std::max(1, int(std::lround(double(params.rib_spacing) / double(std::max(params.tie_spacing, coord_t(1))))));
    const coord_t tie_spacing    = std::max(coord_t(1), params.rib_spacing / k);
    const coord_t rib_spacing    = tie_spacing * k;
    // A chord crossing a wall at 45 degrees is sqrt(2) times as long as the wall is wide. Anything much
    // longer runs along the wall rather than across it, and adds nothing to what its faces already do.
    // Where the infill is wide, the chords are longer too, and only the ribs are printed.
    const coord_t max_tie_length = 3 * rib_spacing / 2;
    // A chord must have infill on both sides. One running along an edge of the area would be printed over the wall.
    const coord_t side_offset    = std::max(coord_t(SCALED_EPSILON), params.min_length / 4);
    // A rib close to a wall running alongside it would only thicken that wall. It may pass the narrow
    // necks between pockets, but needs room on both sides along most of its length.
    const coord_t rib_clearance  = rib_spacing / 4;

    // Work in a frame where the lattice lines are vertical (x = i * tie_spacing) and, transposed, horizontal.
    ExPolygons rotated = area;
    if (params.angle != 0.)
        for (ExPolygon &expoly : rotated)
            expoly.rotate(-params.angle);
    // The wide parts hold a whole lattice cell. Opening with the default miter join keeps the corners
    // of rectangular parts, which then stay wide up to the walls.
    ExPolygons wide_ex = opening_ex(rotated, 0.5f * float(rib_spacing) - float(SCALED_EPSILON));

    Polylines ribs;
    for (int family = 0; family < 2; ++ family) {
        Polygons polygons = to_polygons(rotated);
        Polygons wide     = to_polygons(wide_ex);
        if (family == 1) {
            transpose(polygons);
            transpose(wide);
        }
        const BoundingBox bbox = get_extents(polygons);
        for (coord_t i = coord_t(std::ceil(double(bbox.min.x()) / double(tie_spacing))); i * tie_spacing <= bbox.max.x(); ++ i) {
            const coord_t               c          = i * tie_spacing;
            const bool                  is_rib     = i % k == 0;
            const std::vector<Interval> chords     = chords_at(polygons, c);
            if (chords.empty())
                continue;
            const std::vector<Interval> left       = chords_at(polygons, c - side_offset);
            const std::vector<Interval> right      = chords_at(polygons, c + side_offset);
            const std::vector<Interval> wide_parts = chords_at(wide, c);
            // Where a rib would have room on both sides.
            const std::vector<Interval> rib_room   = is_rib ? intersect(chords_at(polygons, c - rib_clearance), chords_at(polygons, c + rib_clearance)) :
                                                              std::vector<Interval>();
            for (const Interval &chord : chords) {
                const coord_t length = chord.second - chord.first;
                const coord_t middle = chord.first + length / 2;
                if (length < params.min_length || ! contains(left, middle) || ! contains(right, middle))
                    continue;
                // A rib crosses a wide part and runs on to the walls on either side of it. A tie is any short
                // chord from one wall to another: across a narrow wall, or between pockets close to each other.
                const bool rib = is_rib && overlap(chord, wide_parts) >= std::min(rib_spacing / 2, length / 2) && overlap(chord, rib_room) >= length / 2;
                const bool tie = length <= max_tie_length;
                if (rib || tie)
                    ribs.emplace_back(family == 0 ? Polyline(Point(c, chord.first), Point(c, chord.second)) :
                                                    Polyline(Point(chord.first, c), Point(chord.second, c)));
            }
        }
    }

    if (params.angle != 0.)
        for (Polyline &rib : ribs)
            rib.rotate(params.angle);
    return ribs;
}

} // namespace Slic3r::FillLightning
