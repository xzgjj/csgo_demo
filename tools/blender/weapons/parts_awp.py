"""AWP 部件表：长枪管 / 大瞄准镜 / 拉栓 / 绿黑枪身 / 弹匣。"""

from __future__ import annotations

from .meshkit import WeaponBuilder


def build_awp(b: WeaponBuilder) -> None:
    metal = "gun_metal"
    poly = "gun_polymer"
    # 机匣
    b.box("receiver", (0.0, 0.0, 0.005), (0.062, 0.300, 0.072), metal, bevel=0.003)
    b.box("receiver_bottom", (0.0, 0.0, -0.035), (0.055, 0.240, 0.020), metal)
    b.box("receiver_top_rail", (0.0, 0.0, 0.045), (0.030, 0.260, 0.012), metal)
    for index in range(7):
        b.box(f"rail_teeth_{index}", (0.0, -0.108 + index * 0.036, 0.053),
              (0.032, 0.010, 0.008), metal, bevel=0.0008)
    b.box("receiver_side_plate_l", (0.032, 0.0, 0.000), (0.004, 0.200, 0.040), metal)
    b.box("receiver_side_plate_r", (-0.032, 0.0, 0.000), (0.004, 0.200, 0.040), metal)
    # 长枪管（绿色枪管护套）+ 枪口
    b.cyl("barrel_shroud", (0.0, 0.360, 0.012), 0.400, 0.021, poly, axis="Y",
          segments=16)
    b.box("barrel_rib_l", (0.021, 0.350, 0.012), (0.006, 0.340, 0.012), metal)
    b.box("barrel_rib_r", (-0.021, 0.350, 0.012), (0.006, 0.340, 0.012), metal)
    b.cyl("barrel_base_ring", (0.0, 0.170, 0.012), 0.030, 0.028, metal, axis="Y",
          segments=16)
    b.cyl("muzzle_ring", (0.0, 0.552, 0.012), 0.010, 0.023, metal, axis="Y",
          segments=16)
    b.cyl("barrel_muzzle", (0.0, 0.578, 0.012), 0.050, 0.024, metal, axis="Y",
          segments=16)
    b.cyl("muzzle_face", (0.0, 0.605, 0.012), 0.010, 0.020, metal, axis="Y",
          segments=16)
    b.box("muzzle_slot_top", (0.0, 0.578, 0.036), (0.014, 0.024, 0.004), metal)
    b.box("muzzle_slot_bottom", (0.0, 0.578, -0.012), (0.014, 0.024, 0.004), metal)
    # 大瞄准镜
    b.box("scope_mount_base", (0.0, 0.0, 0.062), (0.050, 0.200, 0.022), metal)
    b.box("scope_mount_rear", (0.0, -0.065, 0.082), (0.034, 0.028, 0.030), metal)
    b.box("scope_mount_front", (0.0, 0.065, 0.082), (0.034, 0.028, 0.030), metal)
    b.box("scope_ring_rear", (0.0, -0.065, 0.098), (0.050, 0.022, 0.050), metal,
          bevel=0.002)
    b.box("scope_ring_front", (0.0, 0.065, 0.098), (0.050, 0.022, 0.050), metal,
          bevel=0.002)
    b.cyl("scope_tube", (0.0, -0.005, 0.102), 0.235, 0.023, metal, axis="Y",
          segments=16)
    b.cyl("scope_objective_taper", (0.0, 0.135, 0.102), 0.050, 0.025, metal, axis="Y",
          segments=16, radius_end=0.034)
    b.cyl("scope_objective", (0.0, 0.175, 0.102), 0.040, 0.034, metal, axis="Y",
          segments=16)
    b.cyl("scope_lens_front", (0.0, 0.196, 0.102), 0.006, 0.032, "gun_glass",
          axis="Y", segments=16)
    b.cyl("scope_eyepiece_taper", (0.0, -0.150, 0.102), 0.060, 0.023, metal, axis="Y",
          segments=16, radius_end=0.028)
    b.cyl("scope_eyepiece", (0.0, -0.190, 0.102), 0.035, 0.028, metal, axis="Y",
          segments=16)
    b.cyl("scope_lens_rear", (0.0, -0.208, 0.102), 0.006, 0.026, "gun_glass",
          axis="Y", segments=16)
    b.cyl("scope_parallax_ring", (0.0, -0.110, 0.102), 0.016, 0.026, metal, axis="Y",
          segments=16)
    b.cyl("scope_turret_top", (0.0, 0.0, 0.140), 0.028, 0.014, metal, axis="Z",
          segments=12)
    b.cyl("scope_turret_cap", (0.0, 0.0, 0.157), 0.010, 0.015, metal, axis="Z",
          segments=12)
    b.cyl("scope_turret_right", (0.042, 0.0, 0.102), 0.028, 0.013, metal, axis="X",
          segments=12)
    b.cyl("scope_turret_left", (-0.042, 0.0, 0.102), 0.028, 0.013, metal, axis="X",
          segments=12)
    # 拉栓
    b.cyl("bolt_body", (0.030, -0.100, 0.050), 0.070, 0.008, metal, axis="Y",
          segments=10)
    b.cyl("bolt_handle", (0.065, -0.078, 0.022), 0.070, 0.007, metal, axis="X",
          segments=10)
    b.cyl("bolt_knob", (0.105, -0.078, 0.022), 0.020, 0.013, metal, axis="X",
          segments=10)
    b.cyl("bolt_shroud", (0.0, -0.155, 0.040), 0.050, 0.016, metal, axis="Y",
          segments=12)
    b.cyl("bolt_shroud_cap", (0.0, -0.185, 0.040), 0.016, 0.013, metal, axis="Y",
          segments=12)
    # 枪身（绿黑）：梳形枪托 + 拇指孔
    b.box("stock_comb", (0.0, -0.295, 0.032), (0.046, 0.270, 0.056), poly, bevel=0.004)
    b.box("stock_lower", (0.0, -0.330, -0.100), (0.046, 0.150, 0.032), poly,
          bevel=0.004)
    b.box("stock_rear", (0.0, -0.440, -0.030), (0.046, 0.045, 0.170), poly,
          bevel=0.004)
    b.box("butt_pad", (0.0, -0.468, -0.030), (0.050, 0.020, 0.170), poly, bevel=0.003)
    b.box("cheek_riser", (0.0, -0.300, 0.072), (0.040, 0.160, 0.020), poly,
          bevel=0.003)
    b.box("pistol_grip", (0.0, -0.185, -0.045), (0.044, 0.058, 0.130), poly,
          rot=(20.0, 0, 0), bevel=0.004)
    b.box("grip_cap", (0.0, -0.211, -0.113), (0.046, 0.058, 0.012), metal,
          rot=(20.0, 0, 0))
    # 扳机组 / 弹匣
    b.box("trigger_guard", (0.0, -0.105, -0.048), (0.014, 0.100, 0.008), metal)
    b.box("trigger_guard_front", (0.0, -0.150, -0.032), (0.014, 0.008, 0.028), metal)
    b.box("trigger", (0.0, -0.110, -0.030), (0.008, 0.012, 0.024), metal,
          rot=(-8.0, 0, 0))
    b.box("magazine", (0.0, 0.010, -0.085), (0.046, 0.110, 0.095), metal, bevel=0.002)
    b.box("magazine_floor", (0.0, 0.010, -0.137), (0.052, 0.118, 0.014), metal)
    b.box("mag_release", (0.0, -0.025, -0.055), (0.050, 0.028, 0.018), metal)
    b.box("safety_lever", (0.031, -0.130, 0.010), (0.006, 0.040, 0.012), metal)
    for index, (x, y, z) in enumerate(((0.032, -0.080, -0.010), (0.032, 0.040, -0.010),
                                      (-0.032, -0.080, -0.010), (-0.032, 0.040, -0.010))):
        b.cyl(f"receiver_rivet_{index}", (x, y, z), 0.005, 0.0045, metal, axis="X",
              segments=8)
    # 前部两脚架导轨
    b.box("bipod_rail", (0.0, 0.400, -0.008), (0.022, 0.100, 0.008), metal)
    b.box("bipod_leg_l", (0.012, 0.400, -0.016), (0.008, 0.070, 0.030), metal,
          rot=(15.0, 0, 0))
    b.box("bipod_leg_r", (-0.012, 0.400, -0.016), (0.008, 0.070, 0.030), metal,
          rot=(15.0, 0, 0))
    b.box("sling_loop_front", (0.0, 0.300, -0.002), (0.016, 0.012, 0.020), metal)
    b.box("sling_loop_rear", (0.0, -0.110, -0.035), (0.016, 0.012, 0.020), metal)
