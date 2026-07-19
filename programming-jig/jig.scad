// ============================================================
// Flash-and-Address Programming Jig — Split Flap Driver V4
// ============================================================
// Two printed parts: a base cradle that registers the board on
// its corner holes, and a press head riding on two guide posts,
// carrying 7 spring (pogo) pins that land on the board's UPDI
// header holes and the four bus pogo-pin holes.
//
// Coordinates extracted from SplitFlapDriverATtiny1616V4.kicad_pcb.
// Board space: origin at board TOP-LEFT corner (silkscreen upright,
// component side up), X right, Y downward — same as the KiCad sheet.
//
// Choose what to render:
part = "base";      // "base" | "head" | "coupon" | "assembly"

// ---- Board facts (fixed by the PCB design) ------------------
board_w   = 30;     // X extent, mm
board_l   = 60;     // Y extent, mm
board_t   = 1.6;    // PCB thickness

// Corner registration holes: dia 3.0 at 3 mm in from each corner
corner_holes = [[3,3],[27,3],[3,57],[27,57]];

// Contact targets (all dia 1.0 plated through-holes)
// UPDI header: +3V3 is unused (board powered via 12V pad)
contacts = [
  [15.35,  6.80],   // UPDI GND
  [17.89,  6.80],   // UPDI data
  [15.00, 22.35],   // +12V
  [15.00, 28.10],   // RS-485 B
  [15.00, 31.90],   // RS-485 A
  [15.00, 37.65],   // GND
];

// ---- Tunables ----------------------------------------------
pogo_hole_d   = 1.3;   // press-fit hole for pogo barrel — CALIBRATE WITH COUPON
pogo_protrude = 4;     // pogo tip below head underside, uncompressed
pogo_travel   = 1.5;   // compression when head is bottomed on stops
peg_d         = 2.7;   // registration peg dia (clearance in 3.0 board hole)
pocket_clr    = 0.25;  // board pocket clearance per side
ledge_w       = 5;     // perimeter shelf the board rests on
base_t        = 6;     // base plate thickness
head_t        = 8;     // press-head thickness
post_d        = 6;     // guide post dia
post_hole_clr = 0.25;  // head-to-post clearance per side
plate_margin  = 6;     // base material around the pocket
post_offset   = 9;     // post center distance from board edge
$fn = 48;

// ---- Derived -----------------------------------------------
pocket_w = board_w + 2*pocket_clr;
pocket_l = board_l + 2*pocket_clr;
ledge_z  = base_t - board_t - 0.2;        // shelf height: board top sits ~0.2 below base top
plate_w  = board_w + 2*(post_offset + post_d/2 + 4);
plate_l  = board_l + 2*plate_margin;
post_x   = [-post_offset, board_w + post_offset];
post_y   = board_l/2;
board_top    = ledge_z + board_t;
stop_top     = board_top + pogo_protrude - pogo_travel;  // head underside lands here
shoulder_d   = post_d + 5;
post_h       = stop_top + head_t + 12;

// board-space -> model-space (flip Y so silkscreen-up matches print)
function bs(p) = [p[0], board_l - p[1]];

module at_each(pts) { for (p = pts) translate(bs(p)) children(); }

// ============================================================
module base() {
  difference() {
    union() {
      // plate
      translate([-(plate_w-board_w)/2, -plate_margin, 0])
        cube([plate_w, plate_l, base_t]);
      // guide posts with stop shoulders
      for (x = post_x) translate([x, post_y, 0]) {
        cylinder(d=shoulder_d, h=stop_top);
        cylinder(d=post_d, h=post_h);
      }
    }
    // board pocket
    translate([-pocket_clr, -pocket_clr, ledge_z])
      cube([pocket_w, pocket_l, base_t]);
    // central void so through-hole solder tails hang free
    translate([ledge_w, ledge_w, 1.5])
      cube([board_w - 2*ledge_w, board_l - 2*ledge_w, base_t]);
  }
  // registration pegs (on the shelf, through the board, 1 mm proud)
  at_each(corner_holes)
    cylinder(d=peg_d, h=ledge_z + board_t + 1);
}

// ============================================================
module head() {
  difference() {
    translate([-(plate_w-board_w)/2, -plate_margin, 0])
      cube([plate_w, plate_l, head_t]);
    // guide holes
    for (x = post_x) translate([x, post_y, -1])
      cylinder(d=post_d + 2*post_hole_clr, h=head_t + 2);
    // pogo press-fit holes with wire counterbores from the top
    at_each(contacts) {
      translate([0,0,-1]) cylinder(d=pogo_hole_d, h=head_t + 2);
      translate([0,0,3])  cylinder(d=3.5, h=head_t);        // solder/wire well
    }
    // peg clearance dimples
    at_each(corner_holes)
      translate([0,0,-1]) cylinder(d=4, h=3);
    // wire channel: UPDI pair across to the bus column, then out the +Y edge
    translate([13.5, bs([15.00, 6.80])[1] - 1.75, head_t-2]) cube([6, 3.5, 3]);
    translate([13.25, bs([15.00, 37.65])[1] - 1.75, head_t-2])
      cube([3.5, 37.65 + 1.75 + plate_margin + 1, 3]);   // runs past the UPDI channel and out the plate edge
  }
}

// ============================================================
// Print, then find the tightest hole your pogo barrel presses into
module coupon() {
  n = 7;
  difference() {
    cube([n*8 + 4, 14, 6]);
    for (i = [0:n-1]) {
      d = 1.0 + i*0.1;
      translate([8 + i*8, 7, -1]) cylinder(d=d, h=8);
    }
  }
  for (i = [0:n-1])
    translate([8 + i*8 - 3, 0.6, 6])
      linear_extrude(0.4) text(str(1.0 + i*0.1), size=2.6);
}

// ============================================================
if (part == "base")   base();
if (part == "head")   head();
if (part == "coupon") coupon();
if (part == "assembly") {
  base();
  %translate([0, 0, ledge_z]) cube([board_w, board_l, board_t]);
  translate([0, 0, stop_top]) head();
}
