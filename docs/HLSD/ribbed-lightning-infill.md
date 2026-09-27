# Ribbed Lightning infill — High Level Design

## Purpose and scope

Ribbed Lightning is a sparse infill pattern for parts whose surfaces carry loads
from above, from below and from the side, with little material inside. Board
game inserts are the typical case: solid blocks with pockets for tokens and
cards, pressed down by stacked trays and the box lid, pushed up from inside when
the box is turned over, and pushed sideways by the pieces against the pocket
walls.

Lightning holds up the top surfaces from the inside and nothing else. Its
branches grow down from a top surface and end on the nearest wall, so the rest
of the part is hollow: the two faces of a thick wall between pockets, or the
floor and the top of a block, are not tied together and flex independently.
Ribbed Lightning adds straight ribs that stack into vertical plates, tying those
faces together, and grows the Lightning branches onto the ribs rather than onto
the walls.

## Ribs

The ribs are chords of a square lattice fixed in the object's frame: the lines
`x = i * t` and `y = i * t`, where `t` is the wall tie spacing. The rib spacing is
rounded to a whole multiple `k * t` of it, so every rib also lies on the finer
lattice. The lines run along the X and Y axes, which is how the pockets of an
insert are laid out, or along the axes of the model with "Align directions to
model". Because the lattice does not move from layer to layer, a chord that
stays the same on consecutive layers prints in the same place, and the chords
stack into vertical plates without any matching between layers.

On each layer, `generate_ribs()` cuts every lattice line into chords by the
sparse infill of the regions that print the pattern, and keeps a chord if it is

- a rib: on the rib lattice, crossing a part that holds a circle of the rib
  spacing (the morphological opening of the area) for at least half a rib
  spacing or half its own length, and with a quarter of the rib spacing of
  infill on both sides along at least half its length;
- a tie: at most one and a half rib spacings long. A tie joins two walls close
  to each other, across a narrow wall between pockets or between pockets near
  each other. A lattice line running along a wall is much longer and is left
  out, as it would add nothing to what the faces of the wall already do.

Chords shorter than two infill line widths are left out, and so are chords that
do not have infill on both sides, which would lie on an edge of the area and be
printed over a wall. The result is a square grid at the rib spacing in large
areas, a ladder of ties at the tie spacing across the walls between pockets, and
ties in both directions between pockets close to each other. The clearance rule
keeps a rib from running a few millimetres inside a wall parallel to it, while
still letting a rib pass a narrow neck between two pockets.

## Lightning branches

The Lightning generator is shared with plain Lightning, one per object, and
places the ribs before it grows the trees. It subtracts the strip each rib is
printed over from the infill outlines that the overhangs and the trees are
computed from. To the trees, a rib is then a wall: they are grounded on its edge,
and every branch ends on a rib at most half a rib spacing away instead of
running to the walls of the part. The branches still hold up the top surfaces
at the spacing set by the sparse infill density.

Where a rib begins on a layer above plain infill, such as the extra ties that
start above the floor of an insert where the walls between pockets begin, its
strip is missing from the infill of the layer above and counts as an overhang,
so the branches below hold it up as well.

The ribs are placed only in the sparse infill of regions printing Ribbed
Lightning; a Lightning region of the same object still gets branches only, which
may end on the ribs of a neighbouring region. The Lightning settings of the
generator are read from the first region of the object, as for plain Lightning,
and the rib settings from the first region printing Ribbed Lightning. Without
such a region no ribs are placed, and plain Lightning slices exactly as before.

## Printing

`FillLightning::Filler` prints both patterns; a ribbed filler adds the ribs of
the layer, clipped to the surface it fills, to the branches. Only the branches
are rounded by the smooth factor. Both are widened by "Fill multiline", and the
strip cut out for the trees widens with it. The lines are then connected to the
walls by the usual sparse infill anchors: with the default anchor length, the
ties of a wall join into one zigzag running along it.

## Settings

- `sparse_infill_density`: how closely the branches hold up the top surfaces,
  as for Lightning.
- `ribbed_lightning_rib_spacing`, 16 mm by default: the grid of ribs in wide areas.
- `ribbed_lightning_tie_spacing`, 8 mm by default: the ties.
- The Lightning overhang, prune and straightening angles apply to the branches.
