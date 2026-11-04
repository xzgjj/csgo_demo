"""M4A4 部件表：平顶导轨 / 聚合物护木 / 直弹匣 / 伸缩托 / A2 前准星。"""

from __future__ import annotations

from .meshkit import WeaponBuilder


def build_m4a4(b: WeaponBuilder) -> None:
    metal = "gun_metal"
    poly = "gun_polymer"
    # 上下机匣
    b.box("lower_receiver", (0.0, -0.028, -0.008), (0.056, 0.200, 0.060), metal,
          bevel=0.002)
    b.box("upper_receiver", (0.0, 0.012, 0.034), (0.052, 0.250, 0.044), metal,
          bevel=0.002)
    b.box("rail_base", (0.0, 0.035, 0.060), (0.030, 0.245, 0.008), metal)
    for index in range(9):
        b.box(f"rail_teeth_{index}", (0.0, -0.075 + index * 0.022, 0.066),
              (0.032, 0.008, 0.007), metal, bevel=0.0008)
    b.box("rear_sight_base", (0.0, -0.090, 0.070), (0.028, 0.022, 0.016), metal)
    b.box("rear_sight_aperture", (0.0, -0.090, 0.081), (0.018, 0.010, 0.008), metal)
    b.cyl("rear_sight_drum", (0.0, -0.082, 0.073), 0.030, 0.008, metal, axis="X",
          segments=10)
    b.box("charging_handle", (0.0, -0.125, 0.036), (0.030, 0.030, 0.014), metal)
    b.box("ejection_port", (0.028, 0.005, 0.032), (0.004, 0.050, 0.022), metal)
    b.cyl("forward_assist", (0.031, -0.022, 0.045), 0.014, 0.009, metal, axis="X",
          segments=10)
    b.box("rear_rail_end", (0.0, -0.080, 0.062), (0.030, 0.016, 0.008), metal)
    # 聚合物护木 + 四面导轨
    b.box("handguard", (0.0, 0.205, 0.028), (0.056, 0.215, 0.062), poly, bevel=0.004)
    b.box("hg_rail_base_top", (0.0, 0.205, 0.062), (0.030, 0.215, 0.008), metal)
    for index in range(7):
        b.box(f"hg_rail_teeth_{index}", (0.0, 0.115 + index * 0.028, 0.068),
              (0.032, 0.008, 0.007), metal, bevel=0.0008)
    b.box("hg_side_rail_l", (0.030, 0.205, 0.028), (0.006, 0.200, 0.032), metal)
    b.box("hg_side_rail_r", (-0.030, 0.205, 0.028), (0.006, 0.200, 0.032), metal)
    b.box("hg_bottom_rail", (0.0, 0.205, -0.004), (0.028, 0.180, 0.008), metal)
    # 枪管 / A2 前准星 / 消焰器
    b.cyl("delta_ring", (0.0, 0.312, 0.028), 0.012, 0.031, metal, axis="Y", segments=12)
    b.box("gas_block", (0.0, 0.330, 0.028), (0.028, 0.030, 0.032), metal)
    b.cyl("barrel", (0.0, 0.375, 0.028), 0.190, 0.0105, metal, axis="Y", segments=12)
    b.cyl("barrel_step", (0.0, 0.400, 0.028), 0.030, 0.013, metal, axis="Y", segments=12)
    b.box("front_sight_base", (0.0, 0.395, 0.026), (0.030, 0.032, 0.022), metal)
    b.box("front_sight_tower", (0.0, 0.395, 0.052), (0.014, 0.018, 0.030), metal)
    b.box("front_sight_ear_l", (0.013, 0.395, 0.062), (0.005, 0.016, 0.034), metal)
    b.box("front_sight_ear_r", (-0.013, 0.395, 0.062), (0.005, 0.016, 0.034), metal)
    b.cyl("front_sight_post", (0.0, 0.395, 0.073), 0.022, 0.0025, metal, axis="Z",
          segments=8)
    b.cyl("flash_hider_ring", (0.0, 0.452, 0.028), 0.008, 0.016, metal, axis="Y",
          segments=12)
    b.cyl("flash_hider", (0.0, 0.478, 0.028), 0.058, 0.014, metal, axis="Y", segments=12)
    b.box("flash_hider_slot_top", (0.0, 0.490, 0.041), (0.012, 0.030, 0.004), metal)
    b.box("flash_hider_slot_bottom", (0.0, 0.490, 0.015), (0.012, 0.030, 0.004), metal)
    b.box("sling_loop_front", (0.0, 0.280, -0.002), (0.016, 0.012, 0.020), metal)
    # 伸缩托 / 缓冲管
    b.cyl("buffer_tube", (0.0, -0.215, 0.030), 0.145, 0.016, metal, axis="Y",
          segments=12)
    b.cyl("buffer_tube_end", (0.0, -0.290, 0.030), 0.016, 0.018, metal, axis="Y",
          segments=12)
    b.prism("stock_body", -0.300, -0.190, (0.0, 0.020), (0.024, 0.050),
            (0.0, -0.005), (0.022, 0.038), poly, bevel=0.003)
    b.box("stock_cheek", (0.0, -0.255, 0.055), (0.042, 0.080, 0.016), poly,
          bevel=0.003)
    b.box("stock_buttpad", (0.0, -0.308, 0.020), (0.050, 0.020, 0.096), poly,
          bevel=0.003)
    b.box("stock_lever", (0.025, -0.235, -0.020), (0.006, 0.050, 0.014), metal)
    b.box("sling_loop_rear", (0.0, -0.120, -0.020), (0.016, 0.012, 0.020), metal)
    # 直弹匣（STANAG）
    b.box("mag_well", (0.0, 0.005, -0.045), (0.042, 0.095, 0.028), metal, bevel=0.002)
    b.box("mag_seg_0", (0.0, 0.012, -0.115), (0.028, 0.090, 0.100), poly,
          rot=(4.0, 0, 0), bevel=0.002)
    b.box("mag_seg_1", (0.0, 0.022, -0.205), (0.028, 0.088, 0.095), poly,
          rot=(9.0, 0, 0), bevel=0.002)
    b.box("mag_floor", (0.0, 0.035, -0.253), (0.034, 0.092, 0.014), metal,
          rot=(9.0, 0, 0))
    b.box("mag_rib_l1", (0.016, 0.045, -0.115), (0.004, 0.018, 0.070), poly,
          rot=(4.0, 0, 0))
    b.box("mag_rib_l2", (0.016, -0.010, -0.115), (0.004, 0.018, 0.070), poly,
          rot=(4.0, 0, 0))
    b.box("mag_rib_r1", (-0.016, 0.045, -0.115), (0.004, 0.018, 0.070), poly,
          rot=(4.0, 0, 0))
    b.box("mag_rib_r2", (-0.016, -0.010, -0.115), (0.004, 0.018, 0.070), poly,
          rot=(4.0, 0, 0))
    # 握把 / 扳机组 / 选择杆
    b.box("pistol_grip", (0.0, -0.105, -0.075), (0.036, 0.050, 0.105), poly,
          rot=(18.0, 0, 0), bevel=0.003)
    b.box("grip_cap", (0.0, -0.121, -0.125), (0.038, 0.050, 0.012), metal,
          rot=(18.0, 0, 0))
    b.box("trigger_guard", (0.0, -0.045, -0.048), (0.012, 0.100, 0.008), metal)
    b.box("trigger_guard_front", (0.0, -0.092, -0.032), (0.012, 0.008, 0.032), metal)
    b.box("trigger", (0.0, -0.050, -0.030), (0.007, 0.012, 0.024), metal,
          rot=(-8.0, 0, 0))
    b.box("selector", (0.030, -0.060, 0.006), (0.006, 0.040, 0.010), metal)
    b.box("bolt_catch", (0.029, -0.012, -0.004), (0.006, 0.022, 0.012), metal)
