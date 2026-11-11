// pocketbots — PLA shell for the Waveshare ESP32-S3-Touch-AMOLED-1.64.
//
// Two parts, plus a loose button plunger. Screws come in from the back
// into heat-set inserts in the front, so the face stays plain.
// The keyring loop is part of the back shell and is as thick as the
// whole device, so the hole is wrapped in perimeters.
//
// Print both shells outside-face down. No supports.
//
//   openscad -o stl/front.stl   -D 'part="front"'   pocketbots.scad
//   openscad -o stl/back.stl    -D 'part="back"'    pocketbots.scad
//   openscad -o stl/plunger.stl -D 'part="plunger"' pocketbots.scad
//   openscad -o stl/bumper.stl  -D 'part="bumper"'  pocketbots.scad
//
// Numbers marked "drawing" come from the Waveshare module drawing
// (module 29.12 x 44.00, PCB 28.60 x 43.50, active area 22.34 x 36.06,
// mounting holes 22.00 x 38.50). Board thickness, and where the USB-C
// socket and the BOOT/RST buttons actually sit, are not on that drawing.
// Those are parameters. Print one, see, change a number, print the bit
// that was wrong.

part = "preview"; // preview | front | back | plunger | bumper

/* [Board — from the drawing] */
board_w = 28.60;
board_l = 43.50;
module_w = 29.12;
module_l = 44.00;
active_w = 22.34;
active_l = 36.06;

/* [Board — measure yours] */
board_t = 5.2;          // module thickness, screen included
usb_w = 10.0;           // USB-C cutout. Generous on purpose.
usb_h = 3.8;
usb_z = 0.8;            // how far the cutout sits behind the front skin
boot_from_centre = -8;  // BOOT, along the right edge. Negative is toward USB.
rst_from_centre = 6;    // RST pinhole, same edge. A paperclip, not a plunger.

/* [Shell] */
outer_w = 48;
outer_l = 66;
n_squircle = 4.5;
skin = 1.6;             // front, over the bezel
back_skin = 1.8;
nest_gap = 0.30;        // around the module
seam = 0.20;            // air between the loop and the front
lip_h = 1.4;
lip_t = 1.5;
bat_w = 25;             // a 302535-class cell, leads toward the USB end
bat_l = 35;
bat_t = 4.0;
air = 0.6;              // between the cell and the back of the module

/* [Loop] */
loop_id = 12.0;
loop_wall = 4.6;

/* [Screws — M2, from the back] */
boss_r = 3.1;
boss_x = 18.5;
boss_y = 21.2;
insert_d = 3.2;         // hole the insert melts into
insert_h = 3.4;
screw_d = 2.4;          // clearance
head_d = 4.4;           // socket-head counterbore
head_h = 2.2;

/* [Button] */
plunger_d = 2.5;
tunnel_d = 3.3;
flange_d = 5.6;
flange_h = 1.6;
flange_room = 2.2;

/* [Hidden] */
eps = 0.02;
front_h = skin + board_t;
back_h = back_skin + bat_t + air;
total_h = front_h + back_h;
loop_od = loop_id + 2 * loop_wall;
loop_cy = outer_l / 2 + loop_id / 2 + 1.4;

function sx(t, a, e) = a * (cos(t) >= 0 ? 1 : -1) * pow(max(abs(cos(t)), 1e-6), e);
function sy(t, b, e) = b * (sin(t) >= 0 ? 1 : -1) * pow(max(abs(sin(t)), 1e-6), e);

module squircle(w, h, nn = n_squircle, steps = 120) {
    e = 2 / nn;
    polygon([
        for (i = [0:steps - 1])
            let (t = i * 360 / steps)
            [sx(t, w / 2, e), sy(t, h / 2, e)]
    ]);
}

module rounded_rect(w, h, r) {
    hull() {
        for (x = [-1, 1], y = [-1, 1])
            translate([x * (w / 2 - r), y * (h / 2 - r)])
                circle(r = r, $fn = 32);
    }
}

module loop_tab() {
    // The bit of the loop that sticks out past the body.
    difference() {
        hull() {
            squircle(outer_w, outer_l);
            translate([0, loop_cy]) circle(d = loop_od, $fn = 80);
        }
        offset(delta = seam) squircle(outer_w, outer_l);
        translate([0, loop_cy]) circle(d = loop_id, $fn = 80);
    }
}

module loop_bridge() {
    // A tongue across the seam. loop_tab() stops 0.2mm short of the
    // body so it doesn't grind on the front shell; this overlaps both
    // sides, and only in the back's thickness.
    intersection() {
        offset(delta = 2.4) loop_tab();
        offset(delta = 1.2) squircle(outer_w, outer_l);
    }
}

module lip_ring() {
    difference() {
        offset(delta = -2.0) squircle(outer_w, outer_l);
        offset(delta = -(2.0 + lip_t)) squircle(outer_w, outer_l);
    }
}

module window_2d() {
    // A hair smaller than the active area, so the bezel covers the edge.
    rounded_rect(active_w - 0.8, active_l - 0.8, 3.2);
}

module nest_2d() {
    rounded_rect(module_w + 2 * nest_gap, module_l + 2 * nest_gap, 2.0);
}

module usb_slot(grow = 0) {
    translate([0, -outer_l / 2 - 2, skin + usb_z + (usb_h + grow) / 2])
        cube([usb_w + grow, outer_l, usb_h + grow], center = true);
}

module side_tunnel(y, d) {
    translate([module_w / 2 - 1, y, skin + board_t * 0.55])
        rotate([0, 90, 0])
            cylinder(d = d, h = outer_w, $fn = 32);
}

module bosses_2d() {
    for (x = [-1, 1], y = [-1, 1])
        translate([x * boss_x, y * boss_y])
            circle(r = boss_r, $fn = 40);
}

module front_solid() {
    difference() {
        union() {
            linear_extrude(front_h) squircle(outer_w, outer_l);
            translate([0, 0, front_h]) linear_extrude(lip_h) lip_ring();
            // Bosses stand in the nest so the insert has plastic all round.
            linear_extrude(front_h) intersection() {
                nest_2d();
                bosses_2d();
            }
        }
        // Glass opening, through the skin only.
        translate([0, 0, -1]) linear_extrude(skin + 1) window_2d();
        // The module sits in here.
        translate([0, 0, skin]) linear_extrude(board_t + lip_h + 1) difference() {
            nest_2d();
            bosses_2d();
        }
        // Inserts, from the mating face, blind.
        for (x = [-1, 1], y = [-1, 1])
            translate([x * boss_x, y * boss_y, front_h + lip_h - insert_h])
                cylinder(d = insert_d, h = insert_h + lip_h + 1, $fn = 28);
        usb_slot(0.4);
        side_tunnel(boot_from_centre, tunnel_d);
        // A wider pocket just inside, so the plunger flange has somewhere to live.
        translate([module_w / 2 + nest_gap - flange_room, boot_from_centre, skin + board_t * 0.55])
            rotate([0, 90, 0])
                cylinder(d = flange_d + 0.8, h = flange_room + 0.4, $fn = 32);
        side_tunnel(rst_from_centre, 1.5);
    }
}

module back_solid() {
    difference() {
        union() {
            translate([0, 0, front_h]) linear_extrude(back_h) squircle(outer_w, outer_l);
            linear_extrude(total_h) loop_tab();
            translate([0, 0, front_h]) linear_extrude(back_h) loop_bridge();
        }
        // Groove the lip sinks into.
        translate([0, 0, front_h - eps]) linear_extrude(lip_h + 0.25) difference() {
            offset(delta = -1.8) squircle(outer_w, outer_l);
            offset(delta = -(2.0 + lip_t + 0.45)) squircle(outer_w, outer_l);
        }
        // Cell pocket, open toward the module, skin left at the back.
        translate([0, -1, front_h - eps])
            linear_extrude(air + bat_t + eps)
                rounded_rect(bat_w, bat_l, 2.5);
        // A way out for the leads, toward the USB end.
        translate([0, -bat_l / 2, front_h])
            cube([4, 8, air + bat_t], center = true);
        // Screw clearance and a counterbore for the head, from the outside.
        for (x = [-1, 1], y = [-1, 1]) {
            translate([x * boss_x, y * boss_y, front_h - 1])
                cylinder(d = screw_d, h = back_h + 2, $fn = 24);
            translate([x * boss_x, y * boss_y, total_h - head_h])
                cylinder(d = head_d, h = head_h + 1, $fn = 28);
        }
        usb_slot(0.8);
        side_tunnel(boot_from_centre, tunnel_d);
        side_tunnel(rst_from_centre, 1.5);
    }
}

module plunger() {
    rotate([0, 90, 0]) union() {
        cylinder(d = plunger_d, h = 8.0, $fn = 28);
        translate([0, 0, 8.0]) cylinder(d = flange_d, h = flange_h, $fn = 32);
    }
}

module bumper() {
    // Optional later TPU skin. Not part of the PLA build.
    jacket = 1.4;
    difference() {
        linear_extrude(total_h + 2 * jacket) offset(delta = jacket) union() {
            squircle(outer_w, outer_l);
            hull() {
                squircle(outer_w, outer_l);
                translate([0, loop_cy]) circle(d = loop_od, $fn = 64);
            }
        }
        translate([0, 0, jacket]) linear_extrude(total_h + jacket) union() {
            squircle(outer_w, outer_l);
            loop_tab();
        }
        translate([0, 0, -1]) linear_extrude(skin + jacket + 2) window_2d();
        translate([0, loop_cy, -1]) cylinder(d = loop_id - 0.6, h = total_h + 4, $fn = 64);
        usb_slot(1.6);
    }
}

module glass() {
    translate([0, 0, 0.35])
        color("#07111f")
            linear_extrude(0.7)
                rounded_rect(active_w - 1.4, active_l - 1.4, 3.0);
}

module placed_plunger() {
    color("#3d4148")
        translate([outer_w / 2 - 7.2, boot_from_centre, skin + board_t * 0.55])
            plunger();
}

if (part == "front") front_solid();
else if (part == "back") back_solid();
else if (part == "plunger") plunger();
else if (part == "bumper") bumper();
else {
    color("#2c3038") front_solid();
    color("#1a1c21") back_solid();
    glass();
    placed_plunger();
}
