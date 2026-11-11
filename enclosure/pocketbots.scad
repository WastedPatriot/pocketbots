// pocketbots — a soft squircle for the Waveshare ESP32-S3-Touch-AMOLED-1.64.
//
// The look to chase is the Amoeba One: a moulded pebble, glass close to
// the edge, and a small loop at one top corner rather than a big tab.
// Two PLA parts. They snap. Nothing on the face is a screw.
// The loop is part of the back and runs the full thickness, so the hole
// is a ring of perimeters.
//
// Print the front face-down. Flip the back so its outside face is on
// the bed. No supports.
//
//   openscad -o stl/front.stl   -D 'part="front"'   pocketbots.scad
//   openscad -o stl/back.stl    -D 'part="back"'    pocketbots.scad
//   openscad -o stl/plunger.stl -D 'part="plunger"' pocketbots.scad
//   openscad -o stl/bumper.stl  -D 'part="bumper"'  pocketbots.scad
//
// The module outline, the glass and the hole pitch are from the
// Waveshare drawing. Thickness, and where USB-C and the buttons sit,
// are not. Those are the parameters below.

part = "preview"; // preview | front | back | plunger | bumper
face = "glass";   // glass | cyber   (preview only)

/* [Board — from the drawing] */
module_w = 29.12;
module_l = 44.00;
active_w = 22.34;
active_l = 36.06;

/* [Board — measure yours] */
board_t = 5.2;
usb_w = 9.0;
usb_h = 3.2;
boot_from_centre = -2;  // right edge. Negative is toward the USB end.

/* [Shell] */
outer_w = 34.8;
outer_l = 48.4;
corner_r = 14.4;        // large on purpose. It should look squashed, not machined.
bezel = 6.3;            // even margin of plastic around the glass
fillet = 1.85;          // the edge round that makes it look moulded
nest_gap = 0.30;
recess = 0.85;          // glass sits this far behind the face
front_h = 3.6;
back_skin = 1.55;
bat_w = 26;             // a 302535, 30 mm along the length
bat_l = 31;
bat_t = 3.7;
air = 0.40;

/* [Loop — top right corner, small enough for a split ring] */
loop_id = 5.2;
loop_od = 9.6;

/* [Button] */
plunger_d = 2.4;
tunnel_d = 3.2;
flange_d = 5.2;
flange_h = 1.4;

/* [Hidden] */
eps = 0.04;
fn = 56;
board_back = recess + board_t;
total_h = board_back + air + bat_t + back_skin;
win_w = outer_w - 2 * bezel;
win_l = outer_l - 2 * bezel;
win_r = 7.2;
nest_w = module_w + 2 * nest_gap;
nest_l = module_l + 2 * nest_gap;
// Top-right bail. The body corner is round, so the hole sits in the
// bite the corner radius leaves, and a little past it.
loop_cx = 15.6;
loop_cy = 22.4;
neck_x = 12.4;
neck_y = 18.6;

module rounded_rect(w, h, r) {
    rad = min(r, min(w, h) / 2 - 0.05);
    hull()
        for (sx = [-1, 1], sy = [-1, 1])
            translate([sx * (w / 2 - rad), sy * (h / 2 - rad)])
                circle(r = rad, $fn = fn);
}

module pebble() {
    // Final size is outer_w x outer_l x total_h, faces and sides rounded.
    translate([0, 0, fillet])
        minkowski() {
            linear_extrude(total_h - 2 * fillet)
                rounded_rect(outer_w - 2 * fillet, outer_l - 2 * fillet, corner_r - fillet);
            sphere(r = fillet, $fn = 48);
        }
}

module slab(z0, z1) {
    translate([-80, -80, z0]) cube([160, 160, z1 - z0]);
}

module window_2d() {
    rounded_rect(win_w, win_l, win_r);
}

module nest_2d() {
    rounded_rect(nest_w, nest_l, 2.2);
}

module cell_2d() {
    rounded_rect(bat_w, bat_l, 2.2);
}

module lip_2d() {
    union() {
        difference() {
            offset(delta = -1.05) rounded_rect(outer_w, outer_l, corner_r);
            offset(delta = -1.85) rounded_rect(outer_w, outer_l, corner_r);
        }
        // Bumps that click into the groove. Part of the lip, so they can't float off.
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
    // Soft ring, full thickness, necked into the back of the body.
    translate([0, 0, 1.05])
        minkowski() {
            linear_extrude(total_h - 2.1)
                hull() {
                    translate([loop_cx, loop_cy]) circle(d = loop_od - 2.1, $fn = fn);
                    translate([neck_x, neck_y]) circle(r = 2.6, $fn = 36);
                }
            sphere(r = 1.05, $fn = 28);
        }
}

module front_solid() {
    difference() {
        union() {
            intersection() {
                pebble();
                slab(-1, front_h);
            }
            // Lip the back closes onto. Sits in the wall, clear of the glass.
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
                // Keep the bail off the front shell. 0.2 mm of air.
                translate([0, 0, -1])
                    linear_extrude(front_h + 1)
                        offset(delta = 0.2) rounded_rect(outer_w, outer_l, corner_r);
            }
        }
        // Groove for the lip.
        translate([0, 0, front_h - eps])
            linear_extrude(1.55)
                difference() {
                    offset(delta = -0.85) rounded_rect(outer_w, outer_l, corner_r);
                    offset(delta = -2.15) rounded_rect(outer_w, outer_l, corner_r);
                }
        // The catches land in shallow pits.
        // Board, down to the ledge the cell starts under.
        translate([0, 0, front_h - eps])
            linear_extrude(board_back - front_h + eps)
                nest_2d();
        // Cell. The pocket is smaller than the board, so the board can't fall in.
        translate([0, -1.5, board_back + air])
            linear_extrude(bat_t + 0.15)
                cell_2d();
        // Leads, toward the USB end.
        translate([0, -bat_l / 2 - 1, board_back + air])
            cube([4.5, 7, bat_t], center = true);
        // USB-C, in the back half of the edge.
        translate([0, -outer_l / 2 + 0.4, board_back - usb_h * 0.35])
            cube([usb_w, 6.5, usb_h], center = true);
        // BOOT. Right side, a short tunnel.
        translate([nest_w / 2 - 0.4, boot_from_centre, board_back - 1.7])
            rotate([0, 90, 0])
                cylinder(d = tunnel_d, h = outer_w, $fn = 28);
        translate([nest_w / 2 + nest_gap, boot_from_centre, board_back - 1.7])
            rotate([0, 90, 0])
                cylinder(d = flange_d + 0.5, h = 2.4, $fn = 28);
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
                sphere(r = skin, $fn = 20);
            }
        translate([0, 0, -0.2]) pebble();
        translate([0, 0, -skin - 1]) linear_extrude(skin + recess + 2) window_2d();
        translate([loop_cx, loop_cy, -skin - 1])
            cylinder(d = loop_id + 0.8, h = total_h + 4 * skin, $fn = 32);
        translate([0, -outer_l / 2 - 2, board_back - usb_h])
            cube([usb_w + 1.4, 16, usb_h + 1.2], center = true);
    }
}

module glass() {
    translate([0, 0, 0.58])
        color("#10141c")
            linear_extrude(0.28)
                rounded_rect(win_w - 0.55, win_l - 0.55, win_r - 0.3);
}

module cyber_mark() {
    // Flat blue hex, two dark eyes. Same family as the firmware mark.
    // Sits in the recess, in front of the dark glass.
    translate([0, 0.3, 0.36]) color("#1E88FF")
        linear_extrude(0.16)
            polygon([for (i = [0:5])
                let (a = i * 60 + 30)
                [8.3 * cos(a), 8.3 * sin(a)]
            ]);
    // Closer to the face than the hex, so they read on top of it.
    color("#16161A")
        for (s = [-1, 1])
            translate([s * 1.75, 0.85, 0.12])
                linear_extrude(0.30)
                    hull() {
                        translate([0, -1.85]) circle(d = 2.15, $fn = 28);
                        translate([0, 1.85]) circle(d = 2.15, $fn = 28);
                    }
}

module button_cap() {
    color("#3C8DFF")
        translate([outer_w / 2 - 0.15, boot_from_centre, board_back - 1.7])
            rotate([0, 90, 0])
                cylinder(d = 3.3, h = 1.15, $fn = 28);
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
}
