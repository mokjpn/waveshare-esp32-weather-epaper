// Case for Waveshare 7.5inch e-Paper (B) raw panel plus
// Waveshare e-Paper ESP32 Driver Board Rev.3.
//
// Units: millimeters.
//
// Export one printable part at a time:
//   openscad -D 'part="back"'         -o back.stl         cad/waveshare_7in5b_esp32_case.scad
//   openscad -D 'part="back_battery"' -o back_battery.stl cad/waveshare_7in5b_esp32_case.scad
//   openscad -D 'part="front"'        -o front.stl        cad/waveshare_7in5b_esp32_case.scad

$fn = 36;

part = "assembly"; // "assembly", "assembly_battery", "back", "back_battery", "front", "panel", "board"

// Waveshare published dimensions.
panel_w = 170.20;
panel_h = 111.20;
panel_t = 1.18;
display_w = 163.20;
display_h = 97.92;

driver_w = 48.25;
driver_h = 29.46;
driver_t = 1.60;

adapter_w = 31.75;
adapter_h = 17.50;
adapter_t = 1.60;

// cheero Canvas 3200mAh IoT USB-C Ver. (CHE-061C / CHE-061-WH-IOT2)
// published dimensions, installed landscape in the battery-back case.
battery_w = 85.00;
battery_h = 50.00;
battery_t = 16.00;

// Case tuning.
xy_clearance = 0.50;
z_clearance = 0.35;
wall = 2.40;
floor_t = 2.20;
front_t = 2.00;
corner_r = 4.00;
bezel_overlap = 2.10;
front_lip_depth = 1.20;
front_lip_overlap = 0.15;
front_panel_top_clearance = 0.35;
front_skirt_width = 1.20;
front_skirt_panel_gap = 0.60;
front_skirt_case_clearance = 0.40;
case_screw_frame_extra = 8.00;
display_window_clearance = 0.80;
case_screw_d = 2.20;       // M2 self-tapping pilot hole.
case_screw_post_d = 5.20;
case_screw_margin = 5.50;
board_screw_d = 2.20;
board_standoff_d = 5.00;
board_standoff_h = 3.00;
panel_support_w = 8.00;
panel_support_h = 1.20;
battery_clearance = 3.00;
battery_wall = 1.80;
battery_floor_t = 1.80;
battery_retainer_h = 2.00;
battery_wire_slot_w = 20.00;
battery_wire_slot_h = 8.00;
battery_wire_channel_w = 13.00;
battery_wire_channel_h = 5.50;
battery_access_window_center_extra = 2.00;
battery_access_window_w = battery_w + battery_access_window_center_extra;
battery_access_window_h = battery_h;
battery_access_finger_d = 18.00;
battery_rear_retention_lip = 3.00;
guide_rail_clearance = 1.20;
guide_rail_wall = 1.40;
guide_rail_h = 3.00;

// Back hanging holes for push pins/thumbtacks.
hook_spacing = 92.00;
hook_y = 55.00;
hook_head_d = 9.00;
hook_slot_w = 3.20;
hook_slot_l = 8.00;

// Active-area placement inside the raw panel. Waveshare gives the active size;
// measure your panel and adjust these if the display window is not centered.
display_center_x_offset = 0.00;
display_center_y_offset = 0.00;

// Driver-board position on the back, with USB-C opening on the right edge.
board_x_from_right_inner = 1.20;
board_y_from_bottom_inner = 14.00;
battery_board_center_x = -12.00;
battery_board_y_from_bottom_inner = 1.60;
battery_center_x = -38.00;
battery_center_y = 10.00;
board_usb_z_center = floor_t + board_standoff_h + driver_t + 1.50;
usb_cutout_w = 11.00;
usb_cutout_h = 5.60;
usb_cutout_extra_depth = 4.00;

// Cable openings. The raw-panel FPC exits near the lower center.
panel_fpc_center_x = 0.00;
panel_fpc_notch_w = 24.00;
panel_fpc_notch_h = 7.50;
internal_cable_slot_w = 14.00;
internal_cable_slot_h = 4.00;

back_inner_clearance = front_skirt_panel_gap + front_skirt_width + front_skirt_case_clearance;
outer_frame_wall = wall + case_screw_frame_extra;
outer_w = panel_w + 2 * (wall + back_inner_clearance + case_screw_frame_extra);
outer_h = panel_h + 2 * (wall + back_inner_clearance + case_screw_frame_extra);
back_total_h = floor_t + board_standoff_h + driver_t + z_clearance + 3.20;
battery_back_total_h = floor_t + battery_t + battery_clearance + 2.80;
panel_seat_z = back_total_h - panel_t - front_panel_top_clearance;
front_z = back_total_h;

inner_w = panel_w + 2 * back_inner_clearance;
inner_h = panel_h + 2 * back_inner_clearance;

module rounded_box(size, r) {
    hull() {
        for (x = [-size[0] / 2 + r, size[0] / 2 - r])
            for (y = [-size[1] / 2 + r, size[1] / 2 - r])
                translate([x, y, 0])
                    cylinder(h = size[2], r = r);
    }
}

module rect_cut(size, center = true) {
    cube(size, center = center);
}

module screw_post(x, y, h, post_d, hole_d) {
    translate([x, y, 0])
        difference() {
            cylinder(h = h, d = post_d);
            translate([0, 0, -0.1])
                cylinder(h = h + 0.2, d = hole_d);
        }
}

module screw_hole_at(x, y, h, d) {
    translate([x, y, -0.1])
        cylinder(h = h + 0.2, d = d);
}

module keyhole_cut(h) {
    translate([0, 0, -0.1])
        union() {
            cylinder(h = h + 0.2, d = hook_head_d);
            translate([0, hook_slot_l / 2, (h + 0.2) / 2])
                cube([hook_slot_w, hook_slot_l, h + 0.2], center = true);
        }
}

module panel_model() {
    color([0.08, 0.08, 0.08, 0.35])
        translate([0, 0, 0])
            rect_cut([panel_w, panel_h, panel_t]);
    color([0.92, 0.92, 0.88, 0.60])
        translate([display_center_x_offset, display_center_y_offset, panel_t / 2 + 0.02])
            rect_cut([display_w, display_h, 0.08]);
    color([0.95, 0.55, 0.05, 0.55])
        translate([panel_fpc_center_x, -panel_h / 2 - 9.0, panel_t / 2])
            rect_cut([18.0, 18.0, 0.20]);
}

module driver_board_model() {
    color([0.02, 0.18, 0.34, 0.55])
        rect_cut([driver_w, driver_h, driver_t]);
    color([0.80, 0.80, 0.75, 0.70])
        translate([driver_w / 2 - 2.2, 0, driver_t / 2 + 1.2])
            rect_cut([4.5, 9.5, 2.4]);
    color([0.94, 0.94, 0.86, 0.65])
        translate([6.0, driver_h / 2 - 2.6, driver_t / 2 + 1.0])
            rect_cut([28.0, 5.0, 2.0]);
}

module adapter_board_model() {
    color([0.02, 0.33, 0.48, 0.50])
        rect_cut([adapter_w, adapter_h, adapter_t]);
}

module back_case(total_h = back_total_h, usb_cutout_enabled = true, battery_enabled = false, hook_holes_enabled = true) {
    seat_z = total_h - panel_t - front_panel_top_clearance;
    screw_x = outer_w / 2 - case_screw_margin;
    screw_y = outer_h / 2 - case_screw_margin;
    standard_board_cx = outer_w / 2 - wall - board_x_from_right_inner - driver_w / 2;
    standard_board_cy = -outer_h / 2 + wall + board_y_from_bottom_inner + driver_h / 2;
    close_fpc_board_cy = -outer_h / 2 + wall + battery_board_y_from_bottom_inner + driver_h / 2;
    board_cx = battery_enabled ? battery_board_center_x : standard_board_cx;
    board_cy = battery_enabled ? close_fpc_board_cy : standard_board_cy;
    battery_cx = battery_center_x;
    battery_cy = battery_center_y;
    battery_window_cx = battery_cx + battery_access_window_center_extra / 2;
    battery_window_cy = battery_cy + battery_rear_retention_lip;

    difference() {
        union() {
            difference() {
                union() {
                    rounded_box([outer_w, outer_h, total_h], corner_r);

                    for (x = [-screw_x, screw_x])
                        for (y = [-screw_y, screw_y])
                            screw_post(x, y, total_h, case_screw_post_d, case_screw_d);

                    if (!battery_enabled) {
                        translate([board_cx - driver_w / 2 + 4.0, board_cy - driver_h / 2 + 4.0, floor_t])
                            screw_post(0, 0, board_standoff_h, board_standoff_d, board_screw_d);
                        translate([board_cx + driver_w / 2 - 4.0, board_cy - driver_h / 2 + 4.0, floor_t])
                            screw_post(0, 0, board_standoff_h, board_standoff_d, board_screw_d);
                        translate([board_cx - driver_w / 2 + 4.0, board_cy + driver_h / 2 - 4.0, floor_t])
                            screw_post(0, 0, board_standoff_h, board_standoff_d, board_screw_d);
                        translate([board_cx + driver_w / 2 - 4.0, board_cy + driver_h / 2 - 4.0, floor_t])
                            screw_post(0, 0, board_standoff_h, board_standoff_d, board_screw_d);
                    }
                }

                // Main internal cavity.
                translate([0, 0, floor_t])
                    rounded_box([inner_w, inner_h, total_h + 0.4], max(corner_r - wall, 1.0));

                // FPC exit at the raw panel bottom edge.
                translate([panel_fpc_center_x, -outer_h / 2 + outer_frame_wall / 2, seat_z - 0.2])
                    rect_cut([panel_fpc_notch_w, outer_frame_wall + 1.0, panel_fpc_notch_h], center = true);

                // Internal cable pass-through toward the driver board.
                translate([board_cx - driver_w / 2 - 2.0, board_cy + driver_h / 2 + 1.0, floor_t + 2.0])
                    rect_cut([internal_cable_slot_w, internal_cable_slot_h, 4.2]);

                if (usb_cutout_enabled) {
                    // USB-C side aperture. It deliberately has extra size for cable shells.
                    translate([outer_w / 2 - outer_frame_wall / 2, board_cy, board_usb_z_center])
                        rotate([0, 90, 0])
                            rect_cut([usb_cutout_h, usb_cutout_w, outer_frame_wall + usb_cutout_extra_depth], center = true);
                }
            }

            // Thin ledges support the raw panel on its non-display border.
            translate([0, inner_h / 2 - panel_support_w / 2, seat_z - panel_support_h / 2])
                rect_cut([panel_w - 2 * case_screw_post_d, panel_support_w, panel_support_h], center = true);
            translate([0, -inner_h / 2 + panel_support_w / 2, seat_z - panel_support_h / 2])
                rect_cut([panel_w - 2 * case_screw_post_d, panel_support_w, panel_support_h], center = true);
            translate([-inner_w / 2 + panel_support_w / 2, 0, seat_z - panel_support_h / 2])
                rect_cut([panel_support_w, panel_h - 2 * case_screw_post_d, panel_support_h], center = true);
            translate([inner_w / 2 - panel_support_w / 2, 0, seat_z - panel_support_h / 2])
                rect_cut([panel_support_w, panel_h - 2 * case_screw_post_d, panel_support_h], center = true);

            if (battery_enabled) {
                // Loose battery guide rails. The top and right sides stay open so the
                // battery can be replaced from the rear and the USB-A lead can exit.
                translate([battery_cx, battery_cy - (battery_h + battery_clearance) / 2 - battery_wall / 2, floor_t + battery_t / 2])
                    rect_cut([battery_w + 2 * battery_clearance + 2 * battery_wall, battery_wall, battery_t + battery_retainer_h], center = true);
                translate([battery_cx - (battery_w + battery_clearance) / 2 - battery_wall / 2, battery_cy, floor_t + battery_t / 2])
                    rect_cut([battery_wall, battery_h + 2 * battery_clearance, battery_t + battery_retainer_h], center = true);

                // Loose guide rails for the ESP32 driver board; no board screws required.
                translate([board_cx, board_cy + (driver_h + guide_rail_clearance) / 2 + guide_rail_wall / 2, floor_t + guide_rail_h / 2])
                    rect_cut([driver_w + 2 * guide_rail_clearance + 2 * guide_rail_wall, guide_rail_wall, guide_rail_h], center = true);
                translate([board_cx, board_cy - (driver_h + guide_rail_clearance) / 2 - guide_rail_wall / 2, floor_t + guide_rail_h / 2])
                    rect_cut([driver_w + 2 * guide_rail_clearance + 2 * guide_rail_wall, guide_rail_wall, guide_rail_h], center = true);
                translate([board_cx - (driver_w + guide_rail_clearance) / 2 - guide_rail_wall / 2, board_cy, floor_t + guide_rail_h / 2])
                    rect_cut([guide_rail_wall, driver_h + 2 * guide_rail_clearance, guide_rail_h], center = true);
            }
        }

        // Keep support ledges clear of the visible display area.
        translate([display_center_x_offset, display_center_y_offset, seat_z - panel_support_h / 2])
            rect_cut([display_w + 2.0, display_h + 2.0, panel_support_h + 0.4], center = true);

        if (battery_enabled) {
            // Rear access window for replacing the battery without removing the front
            // bezel. The window is shifted upward so 3 mm of uncut back remains below
            // the battery as a fall-prevention lip.
            translate([battery_window_cx, battery_window_cy, floor_t / 2])
                rect_cut([battery_access_window_w, battery_access_window_h, floor_t + 0.5], center = true);
            translate([battery_window_cx, battery_window_cy + battery_access_window_h / 2, floor_t / 2])
                cylinder(h = floor_t + 0.5, d = battery_access_finger_d, center = true);

            // No lower cable pass-through is cut here. The cheero USB-A lead exits
            // from the open right side of the battery pocket and board guide.
        }

        if (hook_holes_enabled) {
            for (x = [-hook_spacing / 2, hook_spacing / 2])
                translate([x, hook_y, 0])
                    keyhole_cut(floor_t + 0.8);
        }
    }
}

module front_bezel() {
    screw_x = outer_w / 2 - case_screw_margin;
    screw_y = outer_h / 2 - case_screw_margin;
    window_w = display_w + 2 * display_window_clearance;
    window_h = display_h + 2 * display_window_clearance;
    skirt_outer_w = panel_w + 2 * (front_skirt_panel_gap + front_skirt_width);
    skirt_outer_h = panel_h + 2 * (front_skirt_panel_gap + front_skirt_width);
    skirt_inner_w = panel_w + 2 * front_skirt_panel_gap;
    skirt_inner_h = panel_h + 2 * front_skirt_panel_gap;

    difference() {
        union() {
            rounded_box([outer_w, outer_h, front_t], corner_r);

            // Perimeter skirt sits outside the EPD panel edge, inside the back case.
            // It locates the front bezel without occupying the EPD top surface.
            translate([0, 0, -front_lip_depth])
                difference() {
                    rounded_box([skirt_outer_w, skirt_outer_h, front_lip_depth + front_lip_overlap], max(corner_r - wall - 0.8, 0.8));
                    translate([0, 0, -0.1])
                        rect_cut([skirt_inner_w, skirt_inner_h, front_lip_depth + front_lip_overlap + 0.30]);
                }
        }

        translate([display_center_x_offset, display_center_y_offset, front_t / 2])
            rect_cut([window_w, window_h, front_t + 1.8]);

        for (x = [-screw_x, screw_x])
            for (y = [-screw_y, screw_y])
                screw_hole_at(x, y, front_t + 1.0, case_screw_d + 0.35);
    }
}

module assembly() {
    color([0.12, 0.12, 0.12, 0.28])
        back_case();

    translate([0, 0, panel_seat_z + panel_t / 2])
        panel_model();

    translate([0, 0, front_z + 0.25])
        color([0.12, 0.12, 0.12, 0.45])
            front_bezel();

    board_cx = outer_w / 2 - wall - board_x_from_right_inner - driver_w / 2;
    board_cy = -outer_h / 2 + wall + board_y_from_bottom_inner + driver_h / 2;
    translate([board_cx, board_cy, floor_t + board_standoff_h])
        driver_board_model();

    translate([panel_fpc_center_x, -outer_h / 2 + wall + adapter_h / 2 + 2.0, floor_t + 1.0])
        adapter_board_model();
}

module battery_model() {
    color([0.05, 0.05, 0.06, 0.45])
        rounded_box([battery_w, battery_h, battery_t], 5.0);
}

module assembly_battery() {
    color([0.12, 0.12, 0.12, 0.28])
        back_case(battery_back_total_h, false, true);

    local_panel_seat_z = battery_back_total_h - panel_t - front_panel_top_clearance;
    translate([0, 0, local_panel_seat_z + panel_t / 2])
        panel_model();

    translate([0, 0, battery_back_total_h + 0.25])
        color([0.12, 0.12, 0.12, 0.45])
            front_bezel();

    board_cx = battery_board_center_x;
    board_cy = -outer_h / 2 + wall + battery_board_y_from_bottom_inner + driver_h / 2;
    translate([board_cx, board_cy, floor_t + board_standoff_h])
        driver_board_model();

    translate([battery_center_x, battery_center_y, floor_t])
        battery_model();
}

if (part == "back") {
    back_case();
} else if (part == "back_battery") {
    back_case(battery_back_total_h, false, true);
} else if (part == "front") {
    front_bezel();
} else if (part == "panel") {
    panel_model();
} else if (part == "board") {
    driver_board_model();
} else if (part == "assembly_battery") {
    assembly_battery();
} else {
    assembly();
}
