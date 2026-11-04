"""AK-47 部件表：木托木护木 / 弯弹匣 / 导气管 / 准星柱 / 斜切枪口 / 防尘盖。"""

from __future__ import annotations

from .meshkit import WeaponBuilder


def build_ak47(b: WeaponBuilder) -> None:
    wood = "gun_wood"
    metal = "gun_metal"
    poly = "gun_polymer"
    # 机匣组
    b.box("receiver", (0.0, 0.0, 0.005), (0.062, 0.260, 0.058), metal, bevel=0.002)
    b.box("dust_cover", (0.0, -0.045, 0.044), (0.056, 0.215, 0.018), metal, bevel=0.003)
    b.box("dust_cover_ridge", (0.0, -0.145, 0.052), (0.030, 0.022, 0.006), metal)
    b.box("rear_trunnion", (0.0, -0.132, 0.005), (0.064, 0.022, 0.050), metal)
    b.box("rear_sight_block", (0.0, 0.085, 0.050), (0.052, 0.048, 0.024), metal, bevel=0.002)
    b.box("rear_sight_leaf", (0.0, 0.105, 0.064), (0.018, 0.030, 0.006), metal, bevel=0.001)
    b.box("rear_sight_notch", (0.0, 0.112, 0.069), (0.006, 0.008, 0.005), metal)
    b.box("safety_lever", (0.033, -0.060, 0.012), (0.005, 0.070, 0.018), metal)
    b.box("charging_handle", (0.037, 0.020, 0.024), (0.018, 0.040, 0.012), metal)
    b.box("ejection_port", (0.032, 0.000, 0.020), (0.003, 0.050, 0.022), metal)
    b.box("selector_lever", (0.034, -0.090, 0.000), (0.005, 0.052, 0.010), metal)
    b.box("top_cover_latch", (0.0, -0.155, 0.038), (0.022, 0.014, 0.010), metal)
    b.box("rear_sling_loop", (0.0, -0.300, -0.055), (0.012, 0.022, 0.016), metal)
    # 扳机组 / 握把
    b.box("trigger_guard", (0.0, -0.055, -0.042), (0.014, 0.105, 0.008), metal, bevel=0.001)
    b.box("trigger_guard_front", (0.0, -0.105, -0.028), (0.014, 0.008, 0.028), metal)
    b.box("trigger", (0.0, -0.058, -0.028), (0.008, 0.012, 0.024), metal, rot=(-8.0, 0, 0))
    b.box("magazine_catch", (0.0, -0.012, -0.035), (0.030, 0.020, 0.012), metal)
    b.box("pistol_grip", (0.0, -0.144, -0.070), (0.038, 0.058, 0.112), poly,
          rot=(12.0, 0, 0), bevel=0.003)
    b.box("grip_cap", (0.0, -0.156, -0.125), (0.040, 0.058, 0.012), metal,
          rot=(12.0, 0, 0))
    b.cyl("grip_screw", (0.019, -0.150, -0.095), 0.006, 0.005, metal, axis="X",
          segments=8)
    # 木托
    b.prism("buttstock", -0.400, -0.120, (0.0, -0.020), (0.022, 0.062),
            (0.0, 0.005), (0.026, 0.038), wood, bevel=0.004)
    b.box("butt_plate", (0.0, -0.412, -0.020), (0.048, 0.016, 0.126), metal, bevel=0.003)
    b.cyl("butt_screw_upper", (0.0, -0.421, 0.020), 0.006, 0.005, metal, axis="Y",
          segments=8)
    b.cyl("butt_screw_lower", (0.0, -0.421, -0.060), 0.006, 0.005, metal, axis="Y",
          segments=8)
    b.box("stock_tang", (0.0, -0.135, 0.030), (0.030, 0.040, 0.020), metal)
    # 护木 / 导气管 / 枪管
    b.cyl("hg_retainer", (0.0, 0.125, 0.018), 0.014, 0.030, metal, axis="Y", segments=12)
    b.cyl("gas_tube", (0.0, 0.225, 0.048), 0.200, 0.012, metal, axis="Y", segments=12)
    b.box("gas_tube_clamp_rear", (0.0, 0.140, 0.048), (0.040, 0.018, 0.018), metal)
    b.box("gas_tube_clamp_front", (0.0, 0.315, 0.048), (0.040, 0.018, 0.018), metal)
    b.box("upper_handguard", (0.0, 0.225, 0.065), (0.050, 0.185, 0.026), wood,
          bevel=0.004)
    b.box("lower_handguard", (0.0, 0.225, 0.006), (0.054, 0.195, 0.040), wood,
          bevel=0.005)
    b.box("lower_handguard_rib_l", (0.027, 0.225, 0.000), (0.004, 0.150, 0.018), wood)
    b.box("lower_handguard_rib_r", (-0.027, 0.225, 0.000), (0.004, 0.150, 0.018), wood)
    b.box("hg_front_retainer", (0.0, 0.322, 0.018), (0.048, 0.016, 0.050), metal)
    b.box("gas_block", (0.0, 0.345, 0.028), (0.036, 0.050, 0.050), metal, bevel=0.002)
    b.cyl("barrel", (0.0, 0.360, 0.014), 0.180, 0.011, metal, axis="Y", segments=12)
    b.box("front_sight_base", (0.0, 0.402, 0.016), (0.032, 0.036, 0.028), metal)
    b.box("front_sight_ear_l", (0.013, 0.402, 0.050), (0.006, 0.020, 0.038), metal)
    b.box("front_sight_ear_r", (-0.013, 0.402, 0.050), (0.006, 0.020, 0.038), metal)
    b.cyl("front_sight_post", (0.0, 0.402, 0.060), 0.032, 0.0035, metal, axis="Z",
          segments=8)
    b.cyl("muzzle_brake_ring", (0.0, 0.425, 0.014), 0.010, 0.017, metal, axis="Y",
          segments=12)
    b.slant_cyl("muzzle_brake", (0.0, 0.4525, 0.014), 0.060, 0.016, metal,
                cut_co=(0.0, 0.019, 0.0), cut_no=(0.0, 0.7071, 0.7071))
    b.box("sling_loop_front", (0.0, 0.300, -0.020), (0.014, 0.012, 0.020), metal)
    # 弯弹匣：5 段沿弧线向下前方圆滑过渡（相邻段重叠，避免出现断缝）
    b.box("mag_well", (0.0, -0.005, -0.048), (0.032, 0.080, 0.030), metal, bevel=0.002)
    mag_segments = (
        ("mag_seg_0", (0.0, -0.0200, -0.0820), (0.030, 0.078, 0.058), 5.0),
        ("mag_seg_1", (0.0, -0.0165, -0.1219), (0.030, 0.077, 0.058), 17.0),
        ("mag_seg_2", (0.0, -0.0048, -0.1601), (0.030, 0.075, 0.058), 29.0),
        ("mag_seg_3", (0.0, 0.0146, -0.1951), (0.030, 0.073, 0.058), 41.0),
        ("mag_seg_4", (0.0, 0.0408, -0.2253), (0.030, 0.071, 0.058), 53.0),
    )
    for name, center, size, angle in mag_segments:
        b.box(name, center, size, metal, rot=(angle, 0, 0), bevel=0.0015)
    b.box("mag_floor", (0.0, 0.0664, -0.2446), (0.034, 0.072, 0.012), metal,
          rot=(53.0, 0, 0))
    b.box("mag_rib_l", (0.017, -0.016, -0.100), (0.004, 0.052, 0.034), metal,
          rot=(5.0, 0, 0))
    b.box("mag_rib_r", (-0.017, -0.016, -0.100), (0.004, 0.052, 0.034), metal,
          rot=(5.0, 0, 0))
    b.box("mag_rib_l2", (0.017, 0.006, -0.178), (0.004, 0.050, 0.034), metal,
          rot=(35.0, 0, 0))
    b.box("mag_rib_r2", (-0.017, 0.006, -0.178), (0.004, 0.050, 0.034), metal,
          rot=(35.0, 0, 0))
    # 机匣铆钉
    for index, (x, y, z) in enumerate(((0.032, -0.100, -0.005), (0.032, -0.060, -0.005),
                                      (-0.032, -0.100, -0.005), (-0.032, -0.060, -0.005))):
        b.cyl(f"receiver_rivet_{index}", (x, y, z), 0.005, 0.0045, metal, axis="X",
              segments=8)
