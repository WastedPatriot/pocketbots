// pocketbots v2 — a soft squircle for a XIAO ESP32-S3 Sense and a
// Waveshare 1.69" 240×280 touch LCD (module 33.13 × 41.13 mm).
//
// The 1.64 shell is still in enclosure/v1/. This file is the one to print
// for the camera build.
//
// Same idea as the Amoeba One: a moulded pebble, an even bezel, a small
// loop at the top-right corner. Two PLA parts, a snap, no screws. The
// loop is part of the back. There is a lens hole in the back, with a
// shallow ring around it.
//
// The Sense stack is about 15 mm before the case skin, so this pebble is
// thicker than the 1.64 one. The outline is still the pebble.
//
// Print the front face-down. Flip the back so its outside face is on the
// bed. No supports.
//
//   openscad -o stl/front.stl   -D 'part="front"'   pocketbots.scad
//   openscad -o stl/back.stl    -D 'part="back"'    pocketbots.scad
//   openscad -o stl/plunger.stl -D 'part="plunger"' pocketbots.scad
//   openscad -o stl/bumper.stl  -D 'part="bumper"'  pocketbots.scad
//
// Module outline and the active area are from the Waveshare drawing.
// Panel thickness, the Sense stack, where USB-C and BOOT sit, and the
// exact lens centre are not. Those are the parameters below. Measure
// the board you have and change the numbers before you trust a print.

part = "preview"; // preview | front | back | plunger | bumper
face = "glass";   // glass | cyber   (preview only)

/* [Display — from the drawing] */
module_w = 33.13;
module_l = 41.13;
active_w = 27.972;
active_l = 32.634;

/* [Stack — measure yours] */
panel_t = 3.3;          // display module, glass included
xiao_t = 15.4;          // Sense with the camera expansion, lens included
usb_w = 9.2;
usb_h = 3.4;
boot_from_centre = -13; // right edge. Negative is toward the USB end.

/* [Shell] */
wall = 2.2;             // plastic outside the module, on the long axis
// Even margin around the active area. The width grows to match.
outer_l = module_l + 2 * wall;
bezel = (outer_l - active_l) / 2;
outer_w = active_w + 2 * bezel;
corner_r = 16.8;
fillet = 2.05;
nest_gap = 0.35;
recess = 0.80;
front_h = recess + panel_t;
air = 0.40;
back_skin = 1.70;

/* [Cell — a 26 × 21 × 4 mm pack, above the XIAO, not under the lens.
   That is the Pi Hut 150 mAh cell. Cut the plug off and solder the leads. */
bat_w = 26;
bat_l = 21;
bat_t = 4.2;
cell_y = 9.7;

/* [Lens — centre of the Sense board, USB toward the bottom] */
lens_x = 0;
lens_y = -10.2;
lens_d = 4.8;
ring_d = 10.4;
ring_deep = 0.55;

/* [Loop — top right, small enough for a split ring] */
loop_id = 5.2;
loop_od = 9.6;
loop_cx = outer_w / 2 - 2.4;
loop_cy = outer_l / 2 - 2.6;
neck_x = loop_cx - 3.4;
neck_y = loop_cy - 3.6;

/* [Button] */
plunger_d = 2.4;
tunnel_d = 3.2;
flange_d = 5.2;
flange_h = 1.4;

/* [Hidden] */
eps = 0.04;
fn = 56;
board_back = front_h + xiao_t;
total_h = board_back + air + back_skin;
win_w = active_w;
win_l = active_l;
win_r = 8.4;
nest_w = module_w + 2 * nest_gap;
nest_l = module_l + 2 * nest_gap;

module rounded_rect(w, h, r) {
    rad = min(r, min(w, h) / 2 - 0.05);
    hull()
        for (sx = [-1, 1], sy = [-1, 1])
            translate([sx * (w / 2 - rad), sy * (h / 2 - rad)])
                circle(r = rad, $fn = fn);
}

module pebble() {
    translate([0, 0, fillet])
        minkowski() {
            linear_extrude(total_h - 2 * fillet)
                rounded_rect(outer_w - 2 * fillet, outer_l - 2 * fillet, corner_r - fillet);
            sphere(r = fillet, $fn = 40);
        }
}

module slab(z0, z1) {
    translate([-80, -80, z0]) cube([160, 160, z1 - z0]);
}

module window_2d() {
    rounded_rect(win_w, win_l, win_r);
}

module nest_2d() {
    rounded_rect(nest_w, nest_l, 3.0);
}

module cell_2d() {
    rounded_rect(bat_w, bat_l, 2.0);
}

module lip_2d() {
    union() {
        difference() {
            offset(delta = -1.05) rounded_rect(outer_w, outer_l, corner_r);
            offset(delta = -1.85) rounded_rect(outer_w, outer_l, corner_r);
        }
        for (p = [
            [outer_w / 2 - 1.45, 0],
            [-outer_w / 2 + 1.45, 0],
            [0, outer_l / 2 - 1.45],
            [0, -outer_l / 2 + 1.45]
        ])
            translate(p) circle(d = 1.55, $fn = 18);
    }
}

module bail_positive() {
    translate([0, 0, 1.05])
        minkowski() {
            linear_extrude(total_h - 2.1)
                hull() {
                    translate([loop_cx, loop_cy]) circle(d = loop_od - 2.1, $fn = fn);
                    translate([neck_x, neck_y]) circle(r = 2.6, $fn = 32);
                }
            sphere(r = 1.05, $fn = 24);
        }
}

module front_solid() {
    difference() {
        union() {
            intersection() {
                pebble();
                slab(-1, front_h);
            }
            translate([0, 0, front_h - eps])
                linear_extrude(1.35)
                    lip_2d();
        }
        translate([0, 0, -1]) linear_extrude(recess + 1) window_2d();
        translate([0, 0, recess]) linear_extrude(front_h) nest_2d();
    }
}

module back_solid() {
    difference() {
        union() {
            intersection() {
                pebble();
                slab(front_h, total_h + 1);
            }
            difference() {
                bail_positive();
                translate([0, 0, -1])
                    linear_extrude(front_h + 1)
                        offset(delta = 0.2) rounded_rect(outer_w, outer_l, corner_r);
            }
        }
        translate([0, 0, front_h - eps])
            linear_extrude(1.55)
                difference() {
                    offset(delta = -0.85) rounded_rect(outer_w, outer_l, corner_r);
                    offset(delta = -2.15) rounded_rect(outer_w, outer_l, corner_r);
                }
        // Cavity for the display's tail, the Sense stack and the cell.
        translate([0, 0, front_h - eps])
            linear_extrude(board_back + air - front_h + eps)
                nest_2d();
        // A shallow dish so the cell does not wander onto the lens.
        translate([0, cell_y, total_h - back_skin - 0.45])
            linear_extrude(0.7)
                cell_2d();
        // USB-C, only through the bottom wall.
        translate([0, -outer_l / 2 + 0.2, front_h + 1.6])
            cube([usb_w, 6.5, usb_h], center = true);
        // BOOT. Right side, a short tunnel.
        translate([nest_w / 2 - 0.4, boot_from_centre, front_h + 2.2])
            rotate([0, 90, 0])
                cylinder(d = tunnel_d, h = outer_w, $fn = 28);
        translate([nest_w / 2 + nest_gap, boot_from_centre, front_h + 2.2])
            rotate([0, 90, 0])
                cylinder(d = flange_d + 0.5, h = 2.4, $fn = 28);
        // Recessed ring, then the hole the lens looks through.
        translate([lens_x, lens_y, total_h - ring_deep])
            cylinder(d = ring_d, h = ring_deep + 1, $fn = 48);
        translate([lens_x, lens_y, total_h - back_skin - 0.6])
            cylinder(d = lens_d, h = back_skin + 1.4, $fn = 40);
        translate([loop_cx, loop_cy, -2])
            cylinder(d = loop_id, h = total_h + 6, $fn = fn);
    }
}

module plunger() {
    rotate([0, 90, 0]) union() {
        cylinder(d = plunger_d, h = 6.2, $fn = 24);
        translate([0, 0, 6.2]) cylinder(d = flange_d, h = flange_h, $fn = 28);
    }
}

module bumper() {
    skin = 1.35;
    difference() {
        translate([0, 0, -skin])
            minkowski() {
                linear_extrude(total_h)
                    rounded_rect(outer_w, outer_l, corner_r);
                sphere(r = skin, $fn = 16);
            }
        translate([0, 0, -0.2]) pebble();
        translate([0, 0, -skin - 1]) linear_extrude(skin + recess + 2) window_2d();
        translate([loop_cx, loop_cy, -skin - 1])
            cylinder(d = loop_id + 0.8, h = total_h + 4 * skin, $fn = 32);
        translate([0, -outer_l / 2 - 2, front_h + 1.2])
            cube([usb_w + 1.4, 16, usb_h + 1.2], center = true);
        translate([lens_x, lens_y, total_h - 1])
            cylinder(d = ring_d + 1.2, h = 4, $fn = 32);
    }
}

module glass() {
    translate([0, 0, 0.55])
        color("#10141c")
            linear_extrude(0.28)
                rounded_rect(win_w - 0.5, win_l - 0.5, win_r - 0.3);
}

module cyber_mark() {
    translate([0, 0.4, 0.34]) color("#1E88FF")
        linear_extrude(0.16)
            polygon([for (i = [0:5])
                let (a = i * 60 + 30)
                [9.2 * cos(a), 9.2 * sin(a)]
            ]);
    color("#16161A")
        for (s = [-1, 1])
            translate([s * 1.95, 1.0, 0.10])
                linear_extrude(0.30)
                    hull() {
                        translate([0, -2.05]) circle(d = 2.2, $fn = 28);
                        translate([0, 2.05]) circle(d = 2.2, $fn = 28);
                    }
}

module button_cap() {
    color("#3C8DFF")
        translate([outer_w / 2 - 0.15, boot_from_centre, front_h + 2.2])
            rotate([0, 90, 0])
                cylinder(d = 3.3, h = 1.15, $fn = 28);
}

module lens_glass() {
    // Preview only. The hole is in the STL; this stops the render reading as a puncture.
    color("#14161c")
        translate([lens_x, lens_y, total_h - ring_deep - 0.15])
            cylinder(d = lens_d - 0.5, h = 0.25, $fn = 40);
    color("#3a4250")
        translate([lens_x - 0.7, lens_y + 0.5, total_h - ring_deep - 0.02])
            cylinder(d = 1.3, h = 0.08, $fn = 24);
}

if (part == "front") front_solid();
else if (part == "back") back_solid();
else if (part == "plunger") plunger();
else if (part == "bumper") bumper();
else {
    color("#F6F1E8") front_solid();
    color("#F3EEE6") back_solid();
    glass();
    if (face == "cyber") cyber_mark();
    button_cap();
    lens_glass();
}
