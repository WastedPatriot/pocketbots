# Assembly

Two PLA shells, four screws, one loose plunger. The loop is part of the back, and it is as thick as the whole device so the hole is a ring of perimeters rather than a stack of layers waiting to snap.

![Front](images/case-front.png)
![Back](images/case-back.png)

## Print

Open `enclosure/pocketbots.scad`. The STLs in `enclosure/stl/` match the defaults.

| Part | File | Orientation |
| --- | --- | --- |
| Front | `stl/front.stl` | Outside face on the bed, cavity up |
| Back | `stl/back.stl` | Outside face on the bed. The STL comes out of OpenSCAD with that face pointing up, so flip it |
| Plunger | `stl/plunger.stl` | Flange down |

0.2 mm layers, 4 perimeters, 30% infill is plenty for the shells. Give the loop 6 perimeters or solid infill. No supports. The window, the USB slot and the screw counterbores are all open toward the bed or cut from the side.

`stl/bumper.stl` is a TPU jacket for later. Leave it.

## Numbers that are guesses

The module drawing gives the outline, the glass and the mounting-hole pitch. It does not give the thickness, and it does not say where the USB-C plug or the two buttons sit. Those are parameters at the top of the scad file:

- `board_t` — module thickness, screen included. Default 5.2.
- `usb_w`, `usb_h`, `usb_z` — the slot in the bottom edge. It is wide on purpose.
- `boot_from_centre`, `rst_from_centre` — along the right edge, with the glass facing you and the USB at the bottom. Negative is toward the USB.

Print the front, sit the board in the nest, and only then melt the inserts. A wrong button position is a ten-minute reprint of one shell, not a ruined module. The screws clamp the two shells together. They do not go through the PCB's own holes.

## Putting it together

1. Test-fit the board in the front. Screen toward the window. The nest is 0.3 mm larger than the module on each side.
2. Melt the four M2 inserts into the front, from the cavity side, until they sit flush. The holes are blind, so you cannot push one out through the face.
3. Battery in the pocket on the inside of the back. Leads go through the notch toward the USB end, then into the PicoBlade socket. Read the silkscreen for polarity before that last step.
4. Plunger in from the inside. The flange stays in the pocket; the stem sticks out of the right side. Then the board.
5. Close the shells. The lip on the front sinks into a groove on the back, with about 0.2 mm of slack.
6. Four M2 screws from the back. Don't reef them down; PLA creeps, and the lip is doing the locating.
7. A split ring through the loop. The hole is 12 mm.

The charge LED is not on a GPIO. If it is glowing, the cell is charging, and the firmware cannot tell you that on the glass. A reading under 2.5 V on the sense pin is shown as USB rather than 0%, which is what you get with no cell plugged in.
