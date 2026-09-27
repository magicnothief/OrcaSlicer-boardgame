#ifndef slic3r_FillLightning_Ribs_hpp_
#define slic3r_FillLightning_Ribs_hpp_

#include "../../ExPolygon.hpp"
#include "../../Polyline.hpp"

namespace Slic3r::FillLightning {

// Orca: Ribbed Lightning. The ribs are straight chords of a square lattice that is fixed in space,
// so the ribs of consecutive layers stack into vertical plates running from the bottom of the infill
// to its top. The Lightning branches under the top surfaces are grown to end on them.
struct RibParams
{
    // Lattice spacing where the infill is wide enough to hold a whole cell: the ribs form a grid.
    coord_t rib_spacing { 0 };
    // Lattice spacing where the infill is narrower than that, such as the walls between the pockets
    // of an insert: only the chords crossing the wall are kept, tying its two faces together.
    // Rounded so that the rib spacing is a whole multiple of it, keeping the ribs on the finer lattice.
    coord_t tie_spacing { 0 };
    // Direction of the first family of lines, in radians. The second family is perpendicular to it.
    double  angle { 0. };
    // Chords shorter than this are left out.
    coord_t min_length { 0 };
};

Polylines generate_ribs(const ExPolygons &area, const RibParams &params);

} // namespace Slic3r::FillLightning

#endif // slic3r_FillLightning_Ribs_hpp_
