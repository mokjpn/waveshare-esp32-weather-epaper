# 7.5inch e-Paper Case Design

## Target hardware
- Waveshare `7.5inch e-Paper (B)` raw panel.
- Waveshare `e-Paper ESP32 Driver Board` Rev.3 with USB Type-C.
- Standard back: no internal battery. Power is supplied externally through USB Type-C.
- Battery back variant: cheero Canvas 3200mAh IoT USB-C Ver. class USB power bank is retained inside the back case; the driver board is powered by a short internal USB cable.

## Source dimensions
- Raw panel outline: `170.2 x 111.2 x 1.18 mm`.
- Active display area: `163.2 x 97.92 mm`.
- ESP32 driver board outline: `48.25 x 29.46 mm`.
- Driver-board adapter outline: `31.75 x 17.50 mm`.
- cheero Canvas 3200mAh IoT USB-C Ver. outline: `50 x 85 x 16 mm`; the battery-back model installs it landscape as `85 x 50 x 16 mm`.

Sources:
- https://www.waveshare.com/7.5inch-e-paper-b.htm
- https://www.waveshare.com/product/e-paper-esp32-driver-board.htm
- https://www.waveshare.com/wiki/E-Paper_ESP32_Driver_Board

## OpenSCAD model
The parametric model is:

```text
cad/waveshare_7in5b_esp32_case.scad
```

It defines a two-piece printed case:
- `back`: rear tray with panel seat, driver-board standoffs, FPC notch, cable slot, and side USB-C opening.
- `back_battery`: deeper rear tray with internal cheero CHE-061C-sized battery pocket and no external USB-C opening.
- Both back variants include two rear keyhole slots for push-pin hanging.
- The battery back includes an internal wire slot from the battery pocket to the ESP32 driver-board area for a short USB-A-to-USB-C power lead.
- The battery back includes a rear access window and finger notch so the battery can be removed without removing the screw-fastened front bezel. The opening is shifted upward so a `3 mm` uncut rear lip remains below the battery to reduce fall-out risk.
- `front`: screw-on front bezel that captures the raw panel outside the active display area.
- `assembly`: translucent preview with panel, driver board, and adapter placeholders.
- `assembly_battery`: translucent preview of the battery-back variant.

Example exports:

```sh
openscad -D 'part="back"' -o back.stl cad/waveshare_7in5b_esp32_case.scad
openscad -D 'part="back_battery"' -o back_battery.stl cad/waveshare_7in5b_esp32_case.scad
openscad -D 'part="front"' -o front.stl cad/waveshare_7in5b_esp32_case.scad
```

## Parameters to verify on real parts
- `display_center_y_offset`: adjust if the front window is not aligned to the active display area.
- `front_lip_depth`, `front_lip_overlap`, `front_panel_top_clearance`, `front_skirt_width`, `front_skirt_panel_gap`, and `front_skirt_case_clearance`: tune the front bezel fit. The downward skirt now sits outside the EPD panel edge and inside the back-case wall; the front face holds the EPD from above with `front_panel_top_clearance`.
- `case_screw_frame_extra`: expands the outer frame so the four case screws sit outside the EPD panel outline. The default `8.0 mm` keeps the M2 screw posts clear of the panel corners.
- `outer_frame_wall`: derived from `wall + case_screw_frame_extra`; USB and panel-FPC cutouts use this effective wall thickness so they still penetrate after the screw frame is widened.
- `board_x_from_right_inner` and `board_y_from_bottom_inner`: move the driver board so the USB-C connector lines up with the case aperture.
- `battery_board_center_x` and `battery_board_y_from_bottom_inner`: move the ESP32 driver board in the battery-back variant. The default aligns the board right edge close to the panel-FPC opening right edge, matching the test-fit photo, with the USB connector side open for cable access.
- `usb_cutout_w`, `usb_cutout_h`, and `board_usb_z_center`: tune for the exact USB-C connector and cable shell.
- `panel_fpc_center_x`, `panel_fpc_notch_w`, and `internal_cable_slot_w`: tune to avoid bending or pinching the raw panel FPC and extension cable.
- `front_skirt_case_clearance`: tune for printer accuracy around the front skirt; start at `0.40 mm`.
- `battery_w`, `battery_h`, `battery_t`, and `battery_clearance`: adjust if using a battery other than cheero CHE-061C. Defaults are landscape installation dimensions.
- `battery_center_x` and `battery_center_y`: tune the battery pocket location. The default keeps the landscape cheero battery above the lower ESP32 board guide while avoiding overlap with the push-pin holes.
- `battery_access_window_center_extra`, `battery_access_window_w`, `battery_access_window_h`, `battery_access_finger_d`, and `battery_rear_retention_lip`: tune the rear battery replacement opening and bottom fall-prevention lip. The default widens only the case-center side of the opening by `2.0 mm` for easier battery removal.
- `guide_rail_clearance`, `guide_rail_wall`, and `guide_rail_h`: tune the loose guide rails used for the battery-back board and battery retention. The battery top/right rails and board right rail are omitted for battery insertion and USB cable access.
- `battery_wire_slot_w`, `battery_wire_slot_h`, `battery_wire_channel_w`, and `battery_wire_channel_h`: reserved for internal cable tuning. The current battery-back layout does not cut a lower cable pass-through; the USB-A lead exits from the open right side of the battery pocket.
- `hook_spacing`, `hook_y`, `hook_head_d`, `hook_slot_w`, and `hook_slot_l`: adjust for the actual push pins/thumbtacks. The default `hook_y = 55.0 mm` keeps the holes clear of the landscape battery window and finger notch.

## Print notes
- Print the `back` with the rear floor on the bed.
- Print `back_battery` with the rear floor on the bed. It is deeper than the standard back because it includes the battery pocket.
- Print the `front` with the outside face on the bed if the bezel face should be clean.
- Use short M2 self-tapping screws for the case corners.
- The raw panel is thin and fragile; add foam tape or thin gasket material only around the non-display border.
- The battery variant intentionally has no outside charging opening. Remove the battery through the rear access window for charging unless a separate charging aperture or external charge connector is added later.
- Do not rely on printed plastic alone to restrain the battery during a fall. Use thin foam tape or a removable strap after confirming thermal clearance.
- The default battery pocket clearance is `3.0 mm` per side to allow real-part variation, cable shells, foam tape, and ordinary FDM printer tolerance.
- The cheero CHE-061C USB-C port is input only; power output is USB-A. Plan the internal power lead as a short USB-A to USB-C cable from the battery to the ESP32 driver board.
