#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Blender 4.2.3 LTS Headless 3D Heritage Generator for Strata 3D (Gyeongju Silla Capital)
Run via:
  blender.exe --background --python scripts/generate_gyeongju_heritage_blender.py

Generates 20 authentic 3D architectural & archaeological monument models of Gyeongju
using Blender's `bpy` and `bmesh` APIs, baking vertex normals and vertex colors, and exports:
  - data/models/gyeongju_blender_heritage.s3dmesh (fast binary mesh bundle for C++20/OpenGL)
  - data/models/gyeongju_heritage_manifest.json   (metadata & vertex/triangle counts)
  - data/models/gyeongju/<site_id>.ply            (standard Stanford PLY with vertex colors)
"""

import bpy
import bmesh
import json
import math
import os
import struct
from mathutils import Vector, Matrix


# ---------------------------------------------------------------------------
# Color Palette (RGBA 0..1)
# ---------------------------------------------------------------------------
GRANITE_LIGHT   = (0.83, 0.81, 0.76, 1.0)
GRANITE_MID     = (0.74, 0.72, 0.67, 1.0)
GRANITE_DARK    = (0.62, 0.60, 0.56, 1.0)
ANDESITE_BRICK  = (0.35, 0.37, 0.40, 1.0)
ROYAL_VERMILION = (0.78, 0.21, 0.15, 1.0)
HANOK_TIMBER    = (0.56, 0.35, 0.19, 1.0)
PLASTER_WALL    = (0.88, 0.84, 0.75, 1.0)
SILLA_ROOF_DARK = (0.19, 0.22, 0.27, 1.0)
SILLA_ROOF_RIDGE= (0.33, 0.36, 0.41, 1.0)
GOLD_SPIRE      = (0.98, 0.82, 0.18, 1.0)
BRONZE_BELL     = (0.42, 0.36, 0.22, 1.0)
POND_WATER      = (0.09, 0.43, 0.54, 1.0)
MOAT_WATER      = (0.11, 0.46, 0.58, 1.0)
MOUND_GRASS_TOP = (0.54, 0.70, 0.30, 1.0)
MOUND_GRASS_LOW = (0.34, 0.50, 0.21, 1.0)
FOREST_CANOPY   = (0.20, 0.46, 0.19, 1.0)
TREE_TRUNK      = (0.42, 0.28, 0.16, 1.0)
EARTH_RAMPART   = (0.58, 0.51, 0.40, 1.0)
DARK_WINDOW     = (0.15, 0.16, 0.19, 1.0)


def latlon_offset_to_meters(dlat: float, dlon: float, ref_lat: float = 35.835) -> tuple[float, float]:
    """Convert (dlat, dlon) in degrees relative to site anchor into local (dx_east_m, dy_north_m)."""
    m_per_deg_lat = 110950.0
    m_per_deg_lon = 111319.5 * math.cos(math.radians(ref_lat))
    return (dlon * m_per_deg_lon, dlat * m_per_deg_lat)


class BlenderSiteBuilder:
    """Builds a multi-part 3D heritage monument inside a Blender BMesh with per-vertex RGBA."""

    def __init__(self, site_id: str, name: str, category: str, subtitle: str,
                 lat: float, lon: float, label_h: float, badge_rgb: tuple[int, int, int],
                 cam_yaw: float, cam_pitch: float, cam_dist: float):
        self.site_id = site_id
        self.name = name
        self.category = category
        self.subtitle = subtitle
        self.lat = lat
        self.lon = lon
        self.label_h = label_h
        self.badge_rgb = badge_rgb
        self.cam_yaw = cam_yaw
        self.cam_pitch = cam_pitch
        self.cam_dist = cam_dist
        # Accumulate triangles directly via bmesh for exact control & speed
        self.bm = bmesh.new()
        self.color_layer = self.bm.loops.layers.color.new("Col")

    def _transform_pt(self, u: float, v: float, z: float,
                      cx: float, cy: float, cz: float, az_deg: float) -> Vector:
        rad = math.radians(az_deg)
        cos_a = math.cos(rad)
        sin_a = math.sin(rad)
        wx = cx + u * cos_a + v * sin_a
        wy = cy - u * sin_a + v * cos_a
        return Vector((wx, wy, cz + z))

    def add_quad(self, p0: Vector, p1: Vector, p2: Vector, p3: Vector, col: tuple[float, float, float, float]):
        v0 = self.bm.verts.new(p0)
        v1 = self.bm.verts.new(p1)
        v2 = self.bm.verts.new(p2)
        v3 = self.bm.verts.new(p3)
        try:
            f = self.bm.faces.new((v0, v1, v2, v3))
            for loop in f.loops:
                loop[self.color_layer] = col
        except ValueError:
            pass

    def add_tri(self, p0: Vector, p1: Vector, p2: Vector, col: tuple[float, float, float, float]):
        v0 = self.bm.verts.new(p0)
        v1 = self.bm.verts.new(p1)
        v2 = self.bm.verts.new(p2)
        try:
            f = self.bm.faces.new((v0, v1, v2))
            for loop in f.loops:
                loop[self.color_layer] = col
        except ValueError:
            pass

    def add_box(self, cx: float, cy: float, z0: float, h: float,
                w_u: float, d_v: float, az_deg: float, col: tuple[float, float, float, float]):
        hu, hv = 0.5 * w_u, 0.5 * d_v
        b0 = self._transform_pt(-hu, -hv, 0.0, cx, cy, z0, az_deg)
        b1 = self._transform_pt(+hu, -hv, 0.0, cx, cy, z0, az_deg)
        b2 = self._transform_pt(+hu, +hv, 0.0, cx, cy, z0, az_deg)
        b3 = self._transform_pt(-hu, +hv, 0.0, cx, cy, z0, az_deg)
        t0 = self._transform_pt(-hu, -hv, h, cx, cy, z0, az_deg)
        t1 = self._transform_pt(+hu, -hv, h, cx, cy, z0, az_deg)
        t2 = self._transform_pt(+hu, +hv, h, cx, cy, z0, az_deg)
        t3 = self._transform_pt(-hu, +hv, h, cx, cy, z0, az_deg)
        self.add_quad(b0, b1, t1, t0, col)
        self.add_quad(b1, b2, t2, t1, col)
        self.add_quad(b2, b3, t3, t2, col)
        self.add_quad(b3, b0, t0, t3, col)
        self.add_quad(t0, t1, t2, t3, col)

    def add_cylinder(self, cx: float, cy: float, z0: float, z1: float,
                     r_bot: float, r_top: float, segs: int,
                     col: tuple[float, float, float, float], cap_top: bool = True):
        top_c = Vector((cx, cy, z1))
        for i in range(segs):
            a0 = (2.0 * math.pi * i) / segs
            a1 = (2.0 * math.pi * (i + 1)) / segs
            b0 = Vector((cx + r_bot * math.cos(a0), cy + r_bot * math.sin(a0), z0))
            b1 = Vector((cx + r_bot * math.cos(a1), cy + r_bot * math.sin(a1), z0))
            t0 = Vector((cx + r_top * math.cos(a0), cy + r_top * math.sin(a0), z1))
            t1 = Vector((cx + r_top * math.cos(a1), cy + r_top * math.sin(a1), z1))
            self.add_quad(b0, b1, t1, t0, col)
            if cap_top and r_top > 0.03:
                self.add_tri(top_c, t0, t1, col)

    def add_curved_paljak_roof(self, cx: float, cy: float, z_eave: float, roof_h: float,
                               eave_u: float, eave_v: float, az_deg: float,
                               roof_col: tuple[float, float, float, float],
                               golden_chimi: bool = False):
        """High-detail traditional Korean Paljak/Ujingak curved roof with eave lift (앙곡·안허리곡) & Chimi."""
        grid_u, grid_v = 10, 8
        hu, hv = 0.5 * eave_u, 0.5 * eave_v
        ridge_hu = max(0.20 * hu, hu - 0.56 * hv)

        def roof_pt(s: float, t: float) -> Vector:
            abs_t = abs(t)
            abs_s = abs(s)
            flare = 1.0 + 0.07 * (abs_s ** 2) * (abs_t ** 2)
            span_u = (1.0 - abs_t) * ridge_hu + abs_t * hu * flare
            u = s * span_u
            v = t * hv * (1.0 + 0.05 * (abs_s ** 2) * abs_t)
            slope = max(0.0, 1.0 - abs_t) ** 1.38
            corner_lift = 0.18 * roof_h * (abs_s ** 3) * (abs_t ** 2)
            z = roof_h * slope + corner_lift
            return self._transform_pt(u, v, z, cx, cy, z_eave, az_deg)

        for iu in range(grid_u):
            s0 = -1.0 + 2.0 * iu / grid_u
            s1 = -1.0 + 2.0 * (iu + 1) / grid_u
            for iv in range(grid_v):
                t0 = -1.0 + 2.0 * iv / grid_v
                t1 = -1.0 + 2.0 * (iv + 1) / grid_v
                self.add_quad(roof_pt(s0, t0), roof_pt(s1, t0), roof_pt(s1, t1), roof_pt(s0, t1), roof_col)

        # Main ridge beam (용마루) & diagonal hip ridges (내림마루/추녀마루)
        self.add_box(cx, cy, z_eave + roof_h - 0.12, 0.52, ridge_hu * 2.04, 0.70, az_deg, SILLA_ROOF_RIDGE)
        if golden_chimi:
            rad = math.radians(az_deg)
            for side in (-1, 1):
                uc = side * ridge_hu * 0.98
                chx = cx + uc * math.cos(rad)
                chy = cy - uc * math.sin(rad)
                self.add_box(chx, chy, z_eave + roof_h + 0.15, 1.75, 0.90, 1.25, az_deg, GOLD_SPIRE)

    def add_silla_hall(self, cx: float, cy: float, w_u: float, d_v: float,
                       wall_h: float, roof_h: float, az_deg: float,
                       pillar_col=ROYAL_VERMILION, roof_col=SILLA_ROOF_DARK,
                       two_story: bool = False, golden_chimi: bool = False, z_offset: float = 0.0):
        """Complete Silla palace/temple hall: 2-tier granite stylobate + stairs + red colonnade + bracket frieze + curved Paljak roof."""
        base_h = max(0.9, min(2.3, wall_h * 0.22))
        self.add_box(cx, cy, z_offset, base_h * 0.55, w_u * 1.15, d_v * 1.15, az_deg, GRANITE_MID)
        self.add_box(cx, cy, z_offset + base_h * 0.55, base_h * 0.45, w_u * 1.07, d_v * 1.07, az_deg, GRANITE_LIGHT)

        # Front & back stone steps (답도)
        rad = math.radians(az_deg)
        cos_a, sin_a = math.cos(rad), math.sin(rad)
        for s_dir in (-1, 1):
            sv = s_dir * 0.5 * d_v * 1.16
            sx = cx + sv * sin_a
            sy = cy + sv * cos_a
            self.add_box(sx, sy, z_offset, base_h * 0.78, min(6.5, w_u * 0.24), 2.5, az_deg, GRANITE_LIGHT)

        story1_h = wall_h * 0.56 if two_story else wall_h
        z1 = z_offset + base_h
        self.add_box(cx, cy, z1, story1_h, w_u * 0.88, d_v * 0.88, az_deg, PLASTER_WALL)
        # Bracket/entablature frieze (창방·평방·공포대)
        self.add_box(cx, cy, z1 + story1_h * 0.76, story1_h * 0.24, w_u * 1.01, d_v * 1.01, az_deg, pillar_col)

        # Perimeter colonnade (배흘림 기둥열)
        bays_u = max(3, min(9, int(round(w_u / 4.6))))
        bays_v = max(2, min(5, int(round(d_v / 4.6))))
        col_r = max(0.30, min(0.58, w_u * 0.015))
        for iu in range(bays_u + 1):
            for iv in range(bays_v + 1):
                if 0 < iu < bays_u and 0 < iv < bays_v:
                    continue
                u = (-0.5 + iu / bays_u) * w_u * 0.98
                v = (-0.5 + iv / bays_v) * d_v * 0.98
                px = cx + u * cos_a + v * sin_a
                py = cy - u * sin_a + v * cos_a
                self.add_cylinder(px, py, z1, z1 + story1_h, col_r, col_r * 0.86, 10, pillar_col, cap_top=False)

        if two_story:
            self.add_curved_paljak_roof(cx, cy, z1 + story1_h, roof_h * 0.52, w_u * 1.26, d_v * 1.26, az_deg, roof_col, False)
            z2 = z1 + story1_h + roof_h * 0.36
            story2_h = wall_h * 0.48
            self.add_box(cx, cy, z2, story2_h, w_u * 0.74, d_v * 0.74, az_deg, pillar_col)
            self.add_curved_paljak_roof(cx, cy, z2 + story2_h, roof_h * 0.85, w_u * 1.08, d_v * 1.08, az_deg, roof_col, golden_chimi)
        else:
            self.add_curved_paljak_roof(cx, cy, z1 + story1_h, roof_h, w_u * 1.28, d_v * 1.28, az_deg, roof_col, golden_chimi)

    def add_wall_segment(self, ax: float, ay: float, bx: float, by: float,
                         wall_h: float, thick: float, wall_col=ROYAL_VERMILION, roof_col=SILLA_ROOF_DARK):
        length = math.hypot(bx - ax, by - ay)
        if length < 2.0:
            return
        mx, my = 0.5 * (ax + bx), 0.5 * (ay + by)
        az_deg = math.degrees(math.atan2(bx - ax, by - ay))
        self.add_box(mx, my, 0.0, wall_h, thick, length, az_deg, wall_col)
        self.add_box(mx, my, wall_h, 1.15, thick * 1.40, length + 0.8, az_deg, roof_col)

    def add_royal_tomb_mound(self, cx: float, cy: float, radius_m: float, height_m: float,
                             has_curbstone: bool = True, has_table: bool = False):
        """Smooth Cosine-Paraboloid Silla tumulus dome with granite perimeter curbstone (호석) and optional spirit table (상석)."""
        rings = 12
        sectors = 32
        for ir in range(rings):
            u0 = ir / rings
            u1 = (ir + 1) / rings
            r0 = radius_m * u0
            r1 = radius_m * u1

            def h_at(u: float) -> float:
                if u >= 1.0:
                    return 0.0
                cos_d = 0.5 * (1.0 + math.cos(math.pi * u))
                para = 1.0 - u * u
                return height_m * (0.65 * cos_d + 0.35 * para)

            z0 = h_at(u0)
            z1 = h_at(u1)
            crest = 1.0 - 0.5 * (u0 + u1)
            col = GRANITE_MID if (has_curbstone and ir == rings - 1) else (
                MOUND_GRASS_LOW[0] + (MOUND_GRASS_TOP[0] - MOUND_GRASS_LOW[0]) * crest,
                MOUND_GRASS_LOW[1] + (MOUND_GRASS_TOP[1] - MOUND_GRASS_LOW[1]) * crest,
                MOUND_GRASS_LOW[2] + (MOUND_GRASS_TOP[2] - MOUND_GRASS_LOW[2]) * crest,
                1.0
            )
            for isec in range(sectors):
                a0 = (2.0 * math.pi * isec) / sectors
                a1 = (2.0 * math.pi * (isec + 1)) / sectors
                p00 = Vector((cx + r0 * math.cos(a0), cy + r0 * math.sin(a0), z0))
                p01 = Vector((cx + r0 * math.cos(a1), cy + r0 * math.sin(a1), z0))
                p11 = Vector((cx + r1 * math.cos(a1), cy + r1 * math.sin(a1), z1))
                p10 = Vector((cx + r1 * math.cos(a0), cy + r1 * math.sin(a0), z1))
                if ir == 0:
                    self.add_tri(Vector((cx, cy, z0)), p10, p11, col)
                else:
                    self.add_quad(p00, p10, p11, p01, col)

        if has_table:
            self.add_box(cx, cy - radius_m - 2.2, 0.0, 1.2, 3.2, 1.8, 0.0, GRANITE_LIGHT)

    def add_pond_polygon(self, pts_xy: list[tuple[float, float]], z_lift: float = 0.45,
                         water_col=POND_WATER, bank_col=GRANITE_MID):
        if len(pts_xy) < 3:
            return
        # Fan/ear-clip triangulation via Blender bmesh face + poke/triangulate
        verts = [self.bm.verts.new(Vector((x, y, z_lift))) for (x, y) in pts_xy]
        try:
            f = self.bm.faces.new(verts)
            for loop in f.loops:
                loop[self.color_layer] = water_col
        except ValueError:
            pass
        # Stone embankment rim around pond
        n = len(pts_xy)
        for i in range(n):
            ax, ay = pts_xy[i]
            bx, by = pts_xy[(i + 1) % n]
            length = math.hypot(bx - ax, by - ay)
            if length < 0.5:
                continue
            mx, my = 0.5 * (ax + bx), 0.5 * (ay + by)
            az_deg = math.degrees(math.atan2(bx - ax, by - ay))
            self.add_box(mx, my, 0.0, z_lift + 0.55, 2.4, length + 0.6, az_deg, bank_col)

    def add_sacred_tree(self, cx: float, cy: float, height_m: float = 11.0, crown_r: float = 4.5):
        self.add_cylinder(cx, cy, 0.0, height_m * 0.42, crown_r * 0.16, crown_r * 0.12, 6, TREE_TRUNK, cap_top=False)
        self.add_cylinder(cx, cy, height_m * 0.35, height_m * 0.78, crown_r, crown_r * 0.65, 10, FOREST_CANOPY)
        self.add_cylinder(cx, cy, height_m * 0.68, height_m, crown_r * 0.68, crown_r * 0.15, 10, FOREST_CANOPY)

    def finalize_and_extract(self) -> tuple[list[float], list[int]]:
        """Triangulates the BMesh in Blender, computes smooth normals, and extracts interleaved vertex/index buffers."""
        bmesh.ops.triangulate(self.bm, faces=self.bm.faces[:])
        self.bm.normal_update()

        vertices: list[float] = []
        indices: list[int] = []
        v_idx = 0
        for f in self.bm.faces:
            if len(f.verts) != 3:
                continue
            n = f.normal
            if n.length_squared < 1e-8:
                n = Vector((0.0, 0.0, 1.0))
            for loop in f.loops:
                co = loop.vert.co
                col = loop[self.color_layer]
                vertices.extend((
                    float(co.x), float(co.y), float(co.z),
                    float(n.x), float(n.y), float(n.z),
                    float(col[0]), float(col[1]), float(col[2]), float(col[3])
                ))
                indices.append(v_idx)
                v_idx += 1
        return vertices, indices

    def export_ply(self, filepath: str, vertices: list[float], indices: list[int]):
        """Writes standard ASCII PLY with vertex normals and RGBA colors for external inspection."""
        n_verts = len(vertices) // 10
        n_tris = len(indices) // 3
        with open(filepath, "w", encoding="utf-8") as f:
            f.write("ply\nformat ascii 1.0\n")
            f.write(f"comment Generated by Blender {bpy.app.version_string} for Strata 3D ({self.site_id})\n")
            f.write(f"element vertex {n_verts}\n")
            f.write("property float x\nproperty float y\nproperty float z\n")
            f.write("property float nx\nproperty float ny\nproperty float nz\n")
            f.write("property uchar red\nproperty uchar green\nproperty uchar blue\nproperty uchar alpha\n")
            f.write(f"element face {n_tris}\n")
            f.write("property list uchar int vertex_indices\nend_header\n")
            for i in range(n_verts):
                base = i * 10
                x, y, z, nx, ny, nz, r, g, b, a = vertices[base:base + 10]
                ri = max(0, min(255, int(round(r * 255))))
                gi = max(0, min(255, int(round(g * 255))))
                bi = max(0, min(255, int(round(b * 255))))
                ai = max(0, min(255, int(round(a * 255))))
                f.write(f"{x:.3f} {y:.3f} {z:.3f} {nx:.3f} {ny:.3f} {nz:.3f} {ri} {gi} {bi} {ai}\n")
            for i in range(n_tris):
                i0, i1, i2 = indices[i * 3], indices[i * 3 + 1], indices[i * 3 + 2]
                f.write(f"3 {i0} {i1} {i2}\n")

    def free(self):
        self.bm.free()


# ---------------------------------------------------------------------------
# Build All 20 Gyeongju Heritage Sites in Blender
# ---------------------------------------------------------------------------
def build_all_gyeongju_sites() -> list[BlenderSiteBuilder]:
    sites: list[BlenderSiteBuilder] = []

    # 1. 경주 황룡사지 대가람 (사적 제6호)
    s1 = BlenderSiteBuilder(
        "hwangnyongsa", "황룡사 9층목탑 · 대가람 (사적 제6호)", "대사찰",
        "높이 80m 9층목탑 · 중금당 · 동서금당 · 강당 · 종루 · 경루 · 남문 · 외곽 회랑",
        35.837180, 129.233130, 82.0, (205, 50, 35), 14.0, 32.0, 780.0
    )
    az = -1.5
    # 9-story wooden pagoda at (0, 0) = (35.837180, 129.233130)
    s1.add_box(0, 0, 0.0, 2.0, 36.0, 36.0, az, GRANITE_DARK)
    s1.add_box(0, 0, 2.0, 1.8, 30.5, 30.5, az, GRANITE_LIGHT)
    s1.add_box(0, 0, 0.0, 3.5, 7.5, 39.2, az, GRANITE_LIGHT)
    s1.add_box(0, 0, 0.0, 3.5, 39.2, 7.5, az, GRANITE_LIGHT)
    cur_z = 3.8
    for st in range(9):
        t = st / 8.0
        core_w = 22.4 * (1.0 - 0.52 * t)
        eave_w = core_w * 1.44
        wall_h = 4.3 - 1.45 * t
        roof_h = 2.5 - 0.75 * t
        s1.add_box(0, 0, cur_z, wall_h, core_w, core_w, az, ROYAL_VERMILION)
        cur_z += wall_h
        s1.add_curved_paljak_roof(0, 0, cur_z - 0.25, roof_h, eave_w, eave_w, az, SILLA_ROOF_DARK, False)
        cur_z += roof_h * 0.86
    s1.add_box(0, 0, cur_z, 2.2, 5.4, 5.4, az, GOLD_SPIRE)
    s1.add_cylinder(0, 0, cur_z + 2.2, cur_z + 4.6, 2.5, 1.6, 14, GOLD_SPIRE)
    for ring in range(9):
        rz = cur_z + 4.8 + ring * 1.15
        rr = 2.0 * (1.0 - 0.07 * ring)
        s1.add_cylinder(0, 0, rz, rz + 0.42, rr, rr, 14, GOLD_SPIRE)
    s1.add_cylinder(0, 0, cur_z + 2.2, 80.0, 0.55, 0.18, 12, GOLD_SPIRE)

    # Middle, West, East Golden Halls, Lecture Hall, Bell/Sutra Pavilions, Middle/South Gates, Cloister
    dx, dy = latlon_offset_to_meters(35.837550 - s1.lat, 129.233110 - s1.lon)
    s1.add_silla_hall(dx, dy, 51.0, 27.0, 12.2, 8.2, az, two_story=True, golden_chimi=True)
    dx, dy = latlon_offset_to_meters(35.837480 - s1.lat, 129.232580 - s1.lon)
    s1.add_silla_hall(dx, dy, 26.5, 19.5, 8.5, 5.8, az, two_story=False, golden_chimi=True)
    dx, dy = latlon_offset_to_meters(35.837500 - s1.lat, 129.233640 - s1.lon)
    s1.add_silla_hall(dx, dy, 26.5, 19.5, 8.5, 5.8, az, two_story=False, golden_chimi=True)
    dx, dy = latlon_offset_to_meters(35.837950 - s1.lat, 129.233090 - s1.lon)
    s1.add_silla_hall(dx, dy, 55.0, 21.0, 9.2, 6.2, az, two_story=False, golden_chimi=True)
    for plat, plon in ((35.837030, 129.232780), (35.837040, 129.233480)):
        dx, dy = latlon_offset_to_meters(plat - s1.lat, plon - s1.lon)
        s1.add_silla_hall(dx, dy, 13.5, 13.5, 7.6, 5.0, az, two_story=True, golden_chimi=False)
    dx, dy = latlon_offset_to_meters(35.836770 - s1.lat, 129.233150 - s1.lon)
    s1.add_silla_hall(dx, dy, 31.0, 14.5, 8.8, 5.8, az, two_story=True, golden_chimi=True)
    dx, dy = latlon_offset_to_meters(35.836520 - s1.lat, 129.233160 - s1.lon)
    s1.add_silla_hall(dx, dy, 26.0, 12.5, 7.2, 4.8, az, two_story=False, golden_chimi=True)
    corners = [
        latlon_offset_to_meters(35.836770 - s1.lat, 129.232120 - s1.lon),
        latlon_offset_to_meters(35.836770 - s1.lat, 129.234140 - s1.lon),
        latlon_offset_to_meters(35.837950 - s1.lat, 129.234100 - s1.lon),
        latlon_offset_to_meters(35.837950 - s1.lat, 129.232090 - s1.lon),
    ]
    for i in range(4):
        ax, ay = corners[i]
        bx, by = corners[(i + 1) % 4]
        s1.add_wall_segment(ax, ay, bx, by, 4.2, 4.6, ROYAL_VERMILION, SILLA_ROOF_DARK)
    sites.append(s1)

    # 2. 경주 분황사 모전석탑 & 보광전 (국보 제30호)
    s2 = BlenderSiteBuilder(
        "bunhwangsa", "분황사 모전석탑 · 보광전 (국보 제30호)", "국보 석탑",
        "신라 최고(634년) 안산암 벽돌 3층 석탑 · 네 모서리 돌사자 · 보광전 · 팔각석정",
        35.840566, 129.233618, 16.0, (110, 115, 125), 12.0, 30.0, 420.0
    )
    s2.add_box(0, 0, 0.0, 1.9, 13.8, 13.8, 0.0, GRANITE_LIGHT)
    for sx in (-5.6, 5.6):
        for sy in (-5.6, 5.6):
            s2.add_box(sx, sy, 1.9, 1.6, 1.6, 1.6, 0.0, GRANITE_DARK)
    cz = 1.9
    for st, (w, h) in enumerate(((9.8, 3.8), (7.0, 2.4), (4.8, 1.8))):
        s2.add_box(0, 0, cz, h, w, w, 0.0, ANDESITE_BRICK)
        if st == 0:
            s2.add_box(0, 0, cz + 0.3, 2.6, w * 1.04, 2.2, 0.0, GRANITE_LIGHT)
            s2.add_box(0, 0, cz + 0.3, 2.6, 2.2, w * 1.04, 0.0, GRANITE_LIGHT)
        cz += h
        for step in range(4):
            sw = w * (1.22 - 0.08 * step)
            s2.add_box(0, 0, cz + step * 0.36, 0.36, sw, sw, 0.0, SILLA_ROOF_DARK)
        cz += 1.44
    dx, dy = latlon_offset_to_meters(35.840890 - s2.lat, 129.233640 - s2.lon)
    s2.add_silla_hall(dx, dy, 19.0, 12.0, 6.8, 4.6, 0.0)
    dx, dy = latlon_offset_to_meters(35.840490 - s2.lat, 129.233830 - s2.lon)
    s2.add_cylinder(dx, dy, 0.0, 1.6, 1.8, 1.8, 8, GRANITE_LIGHT)
    sites.append(s2)

    # 3. 경주 구황동 원지 유적 (사적 제548호)
    s3 = BlenderSiteBuilder(
        "guhwangdong_wonji", "경주 구황동 원지 유적 (사적 제548호)", "왕경 정원",
        "신라 왕경 조경 연못 · 호안 석축 · 크기 다른 2개 인공섬 · 중심 정자",
        35.840550, 129.235180, 12.0, (25, 130, 175), 15.0, 34.0, 450.0
    )
    pond_pts = [
        latlon_offset_to_meters(lat - s3.lat, lon - s3.lon)
        for lat, lon in ((35.84032, 129.23492), (35.84030, 129.23542),
                         (35.84078, 129.23545), (35.84085, 129.23512), (35.84062, 129.23488))
    ]
    s3.add_pond_polygon(pond_pts, 0.5)
    s3.add_cylinder(-6.0, -2.0, 0.4, 2.6, 11.0, 8.0, 14, MOUND_GRASS_TOP)
    s3.add_cylinder(8.0, 6.0, 0.4, 2.2, 7.5, 5.2, 12, MOUND_GRASS_TOP)
    s3.add_silla_hall(0.0, -26.0, 14.0, 10.0, 5.8, 4.0, 0.0)
    sites.append(s3)

    # 4. 경주 동궁과 월지 / 안압지 (사적 제18호)
    s4 = BlenderSiteBuilder(
        "donggung_wolji", "동궁과 월지 · 안압지 (사적 제18호)", "태자궁·별궁",
        "굴곡진 호안 석축 월지 연못 · 삼신산 3개 섬(대도·중도·소도) · 서안 임해전 제1·2·3 누각",
        35.834788, 129.226537, 20.0, (25, 115, 210), 12.0, 32.0, 620.0
    )
    wolji_ll = [
        (35.83404, 129.22682), (35.83403, 129.22752), (35.83442, 129.22778),
        (35.83502, 129.22775), (35.83552, 129.22755), (35.83568, 129.22685),
        (35.83562, 129.22605), (35.83505, 129.22603), (35.83482, 129.22606),
        (35.83480, 129.22658), (35.83422, 129.22676)
    ]
    s4.add_pond_polygon([latlon_offset_to_meters(a - s4.lat, b - s4.lon) for a, b in wolji_ll], 0.55)
    for ilat, ilon, r_b, r_t, ih in (
        (35.834380, 129.227320, 21.0, 14.0, 4.0),
        (35.835060, 129.227280, 16.0, 10.5, 3.5),
        (35.835430, 129.226380, 11.0, 7.5, 2.8),
    ):
        ix, iy = latlon_offset_to_meters(ilat - s4.lat, ilon - s4.lon)
        s4.add_cylinder(ix, iy, 0.4, ih, r_b, r_t, 16, MOUND_GRASS_TOP)
        s4.add_sacred_tree(ix, iy, ih + 7.5, 4.2)
    # 3 lakeside pavilions + Donggung halls
    for plat, plon, wu, dv, wh, rh in (
        (35.834149, 129.226798, 16.5, 18.5, 7.8, 5.4),
        (35.834788, 129.226537, 23.5, 21.0, 9.2, 6.4),
        (35.835087, 129.225999, 13.0, 14.5, 7.0, 4.8),
        (35.834450, 129.226320, 36.0, 18.0, 8.8, 6.0),
        (35.834180, 129.226350, 26.0, 15.0, 7.5, 5.2),
    ):
        px, py = latlon_offset_to_meters(plat - s4.lat, plon - s4.lon)
        s4.add_silla_hall(px, py, wu, dv, wh, rh, -2.0, golden_chimi=True)
    sites.append(s4)

    # 5. 경주 월성 / 반월성 왕궁 (사적 제16호)
    s5 = BlenderSiteBuilder(
        "wolseong", "경주 월성 · 반월성 왕궁 (사적 제16호)", "신라 왕궁",
        "실제 초승달 모양 왕궁 토석 성곽 · 북편 계단식 해자 연못 · 왕궁 정전 및 내전군",
        35.831830, 129.222080, 24.0, (175, 45, 40), 12.0, 34.0, 920.0
    )
    crescent_ll = [
        (35.83149, 129.21842), (35.83086, 129.21847), (35.83009, 129.21806),
        (35.82959, 129.21840), (35.82930, 129.21905), (35.82907, 129.22049),
        (35.82972, 129.22127), (35.83099, 129.22188), (35.83157, 129.22320),
        (35.83178, 129.22480), (35.83042, 129.22666), (35.83071, 129.22776),
        (35.83158, 129.22862), (35.83190, 129.22808), (35.83255, 129.22601),
        (35.83317, 129.22500), (35.83300, 129.22429), (35.83281, 129.22308),
        (35.83246, 129.22206), (35.83193, 129.22093), (35.83153, 129.22061),
        (35.83131, 129.21914), (35.83149, 129.21842)
    ]
    w_pts = [latlon_offset_to_meters(a - s5.lat, b - s5.lon) for a, b in crescent_ll]
    for i in range(len(w_pts) - 1):
        ax, ay = w_pts[i]
        bx, by = w_pts[i + 1]
        s5.add_wall_segment(ax, ay, bx, by, 5.8, 9.5, EARTH_RAMPART, GRANITE_MID)
    for moat_poly in (
        [(35.83235, 129.22065), (35.83275, 129.22155), (35.83302, 129.22138), (35.83260, 129.22048)],
        [(35.83282, 129.22170), (35.83318, 129.22268), (35.83348, 129.22252), (35.83312, 129.22155)],
        [(35.83322, 129.22282), (35.83342, 129.22382), (35.83372, 129.22375), (35.83352, 129.22274)],
    ):
        s5.add_pond_polygon([latlon_offset_to_meters(a - s5.lat, b - s5.lon) for a, b in moat_poly], 0.4, MOAT_WATER)
    s5.add_silla_hall(0, 0, 48.0, 24.0, 11.5, 7.8, -14.0, two_story=True, golden_chimi=True)
    dx, dy = latlon_offset_to_meters(35.832450 - s5.lat, 129.224150 - s5.lon)
    s5.add_silla_hall(dx, dy, 38.0, 19.0, 9.2, 6.2, -10.0, two_story=False, golden_chimi=True)
    dx, dy = latlon_offset_to_meters(35.830500 - s5.lat, 129.219440 - s5.lon)
    s5.add_silla_hall(dx, dy, 32.0, 17.0, 8.5, 5.5, -12.0, two_story=False, golden_chimi=True)
    sites.append(s5)

    # 6. 경주 석빙고 (보물 제66호)
    s6 = BlenderSiteBuilder(
        "seokbinggo", "경주 석빙고 (보물 제66호)", "왕실 석조유산",
        "월성 북측 성루 위 화강암 홍예(아치) 얼음창고 · 환기구 3기 · 잔디 봉토",
        35.833140, 129.223950, 11.0, (120, 130, 140), 10.0, 30.0, 320.0
    )
    s6.add_box(0, 0, 0.0, 2.8, 9.6, 19.8, 5.0, GRANITE_MID)
    s6.add_box(0, 0, 2.8, 2.2, 7.8, 18.2, 5.0, MOUND_GRASS_TOP)
    for vy in (-5.0, 0.0, 5.0):
        s6.add_box(0, vy, 5.0, 0.9, 1.4, 1.4, 5.0, GRANITE_LIGHT)
    s6.add_box(0, -10.2, 0.0, 2.4, 2.8, 1.8, 5.0, DARK_WINDOW)
    sites.append(s6)

    # 7. 경주 첨성대 (국보 제31호)
    s7 = BlenderSiteBuilder(
        "cheomseongdae", "경주 첨성대 (국보 제31호)", "천문 관측대",
        "선덕여왕대 27단 하이퍼볼릭 곡선 원통형 석조 천문대 · 남측 창문 · 2단 정(井)자 정상석",
        35.834702, 129.218984, 16.0, (155, 110, 45), 14.0, 28.0, 260.0
    )
    sc = 1.45
    s7.add_box(0, 0, 0.0, 1.25 * sc, 5.4 * sc, 5.4 * sc, 0.0, GRANITE_MID)
    z_start = 1.25 * sc
    body_h = 6.85 * sc
    tiers = 16
    for i in range(tiers):
        u0, u1 = i / tiers, (i + 1) / tiers
        r0 = (1.45 + (2.58 - 1.45) * ((1.0 - u0) ** 1.65)) * sc
        r1 = (1.45 + (2.58 - 1.45) * ((1.0 - u1) ** 1.65)) * sc
        s7.add_cylinder(0, 0, z_start + u0 * body_h, z_start + u1 * body_h,
                        r0, r1, 24, GRANITE_LIGHT if i % 2 == 0 else GRANITE_MID, cap_top=(i == tiers - 1))
    s7.add_box(0, -1.95 * sc, z_start + body_h * 0.46, 1.15 * sc, 1.15 * sc, 0.8 * sc, 0.0, DARK_WINDOW)
    s7.add_box(0, -2.02 * sc, z_start + body_h * 0.43, 1.35 * sc, 1.45 * sc, 0.35 * sc, 0.0, GRANITE_LIGHT)
    z_top = z_start + body_h
    for side in (-1, 1):
        s7.add_box(side * 1.15 * sc, 0, z_top, 1.05 * sc, 0.55 * sc, 3.2 * sc, 0.0, GRANITE_LIGHT)
        s7.add_box(0, side * 1.15 * sc, z_top, 1.05 * sc, 3.2 * sc, 0.55 * sc, 0.0, GRANITE_LIGHT)
    sites.append(s7)

    # 8. 경주 월정교 (사적 제457호)
    s8 = BlenderSiteBuilder(
        "woljeonggyo", "경주 월정교 · 춘양교지 (사적 제457호)", "남천 누각교",
        "남천 위 주형(배 모양) 석조 교각 4기 · 단청 회랑 교량(66m) · 남·북 중층 문루 2동",
        35.829250, 129.218140, 20.0, (170, 65, 35), 18.0, 30.0, 420.0
    )
    for v_off in (-22.0, -7.5, 7.5, 22.0):
        s8.add_box(0.0, v_off, -1.0, 5.2, 11.8, 3.5, 1.0, GRANITE_LIGHT)
    s8.add_box(0.0, 0.0, 3.8, 1.4, 10.5, 62.0, 1.0, ROYAL_VERMILION)
    s8.add_silla_hall(0.0, 0.0, 9.2, 56.0, 5.2, 3.8, 1.0, z_offset=3.8)
    for v_gate in (-31.5, 31.5):
        s8.add_silla_hall(0.0, v_gate, 21.5, 13.5, 9.8, 6.4, 1.0, two_story=True, golden_chimi=True)
    sites.append(s8)

    # 9. 경주 계림 & 내물왕릉 (사적 제19·188호)
    s9 = BlenderSiteBuilder(
        "gyerim_naemul", "경주 계림 & 내물왕릉 (사적 제19·188호)", "신화숲·왕릉",
        "김알지 탄강 신화 계림 고목림 · 탄강 유허비각 · 내물마립간 왕릉 및 상석",
        35.832566, 129.217202, 14.0, (45, 145, 65), 14.0, 30.0, 440.0
    )
    s9.add_royal_tomb_mound(0.0, 0.0, 13.5, 6.8, has_curbstone=True, has_table=True)
    dx, dy = latlon_offset_to_meters(35.832350 - s9.lat, 129.217920 - s9.lon)
    s9.add_silla_hall(dx, dy, 8.5, 8.5, 5.2, 3.8, 15.0)
    for tx, ty, th, tr in ((35, -10, 12, 5), (52, 8, 13, 5.5), (68, -22, 11, 4.8),
                           (44, -35, 12.5, 5.2), (80, 5, 11.5, 4.6), (62, 24, 12, 5.0)):
        s9.add_sacred_tree(tx, ty, th, tr)
    sites.append(s9)

    # 10. 경주향교 & 교동 최부자댁 (보물 제1727·2148호)
    s10 = BlenderSiteBuilder(
        "gyeongju_hyanggyo", "경주향교 (대성전·명륜당) & 최부자댁", "국학·고택유산",
        "신라 국학(國學) 터에 세워진 경주향교 대성전·명륜당·동서무와 경주 교동 최부자댁 고택",
        35.831253, 129.216928, 14.0, (140, 85, 40), 16.0, 32.0, 420.0
    )
    s10.add_silla_hall(0.0, 0.0, 22.0, 14.0, 7.2, 5.0, 16.0)
    dx, dy = latlon_offset_to_meters(35.831579 - s10.lat, 129.216794 - s10.lon)
    s10.add_silla_hall(dx, dy, 23.0, 13.0, 6.8, 4.6, 16.0, pillar_col=HANOK_TIMBER)
    dx, dy = latlon_offset_to_meters(35.831090 - s10.lat, 129.216800 - s10.lon)
    s10.add_silla_hall(dx, dy, 9.0, 24.0, 5.2, 3.5, 16.0, pillar_col=HANOK_TIMBER)
    dx, dy = latlon_offset_to_meters(35.831210 - s10.lat, 129.217210 - s10.lon)
    s10.add_silla_hall(dx, dy, 9.0, 24.0, 5.2, 3.5, 16.0, pillar_col=HANOK_TIMBER)
    dx, dy = latlon_offset_to_meters(35.830880 - s10.lat, 129.216210 - s10.lon)
    s10.add_silla_hall(dx, dy, 24.0, 14.0, 5.5, 4.0, 15.0, pillar_col=HANOK_TIMBER)
    s10.add_silla_hall(dx - 16.0, dy + 12.0, 18.0, 11.0, 5.0, 3.6, 15.0, pillar_col=HANOK_TIMBER)
    sites.append(s10)

    # 11. 국립경주박물관 & 성덕대왕신종 (국보 제29호)
    s11 = BlenderSiteBuilder(
        "national_museum_emille", "성덕대왕신종 (국보 제29호) & 국립경주박물관", "국보 범종·박물관",
        "에밀레종 종각 및 청동 대종 · 신라역사관 · 신라미술관 · 고선사지 3층석탑(국보 제38호)",
        35.829891, 129.227277, 18.0, (165, 115, 30), 14.0, 32.0, 560.0
    )
    s11.add_silla_hall(0.0, 0.0, 15.5, 14.6, 8.2, 5.8, 0.0)
    s11.add_cylinder(0.0, 0.0, 1.8, 5.6, 1.6, 1.1, 14, BRONZE_BELL)
    dx, dy = latlon_offset_to_meters(35.829490 - s11.lat, 129.227820 - s11.lon)
    s11.add_silla_hall(dx, dy, 56.0, 42.0, 11.0, 6.5, 0.0, pillar_col=GRANITE_MID)
    dx, dy = latlon_offset_to_meters(35.828720 - s11.lat, 129.228560 - s11.lon)
    s11.add_silla_hall(dx, dy, 40.0, 52.0, 10.5, 6.2, 0.0, pillar_col=GRANITE_MID)
    dx, dy = latlon_offset_to_meters(35.828130 - s11.lat, 129.229060 - s11.lon)
    s11.add_box(dx, dy, 0.0, 2.4, 8.5, 8.5, 0.0, GRANITE_LIGHT)
    s11.add_box(dx, dy, 2.4, 6.8, 4.2, 4.2, 0.0, GRANITE_MID)
    sites.append(s11)

    # 12. 대릉원 황남대총 남·북 쌍분 (사적 제512호)
    s12 = BlenderSiteBuilder(
        "daereungwon_hwangnam", "대릉원 황남대총 (높이 23m 남북 쌍분 · 사적 제512호)", "최대 왕릉",
        "신라 최대 크기 표형분(쌍분, 남북 길이 120m · 높이 23m) 왕릉 · 금관 및 은관 출토",
        35.838967, 129.211929, 27.0, (45, 150, 60), 22.0, 28.0, 520.0
    )
    dx_n, dy_n = latlon_offset_to_meters(35.839240 - s12.lat, 129.211929 - s12.lon)
    dx_s, dy_s = latlon_offset_to_meters(35.838680 - s12.lat, 129.211929 - s12.lon)
    s12.add_royal_tomb_mound(dx_n, dy_n, 41.0, 23.0, has_curbstone=True)
    s12.add_royal_tomb_mound(dx_s, dy_s, 40.5, 22.2, has_curbstone=True)
    sites.append(s12)

    # 13. 천마총 · 미추왕릉 · 검총 (대릉원 왕릉군)
    s13 = BlenderSiteBuilder(
        "cheonmachong_michu", "천마총 · 미추왕릉 · 대릉원 고분군", "대릉원 왕릉",
        "천마도와 금관이 출토된 천마총(155호분) · 김씨 시조왕 미추왕릉 · 검총 · 90호 대분",
        35.838574, 129.210495, 18.0, (45, 150, 60), 18.0, 30.0, 680.0
    )
    s13.add_royal_tomb_mound(0.0, 0.0, 26.8, 12.7, has_curbstone=True)
    for tlat, tlon, r_m, h_m, tbl in (
        (35.837067, 129.213390, 28.0, 12.4, True),   # 미추왕릉
        (35.837456, 129.211932, 16.5, 8.2, False),   # 검총
        (35.839964, 129.213112, 28.0, 11.8, False),  # 90호
        (35.838237, 129.212099, 27.5, 11.5, False),  # 중앙 대분
        (35.837890, 129.211085, 24.8, 10.5, False),  # 서편 대분
        (35.836969, 129.212806, 24.9, 10.6, False),  # 남편 대분
    ):
        tx, ty = latlon_offset_to_meters(tlat - s13.lat, tlon - s13.lon)
        s13.add_royal_tomb_mound(tx, ty, r_m, h_m, has_curbstone=True, has_table=tbl)
    # Michuwangneung shrine gate (숭혜전/삼문)
    mx, my = latlon_offset_to_meters(35.835200 - s13.lat, 129.213760 - s13.lon)
    s13.add_silla_hall(mx, my, 18.0, 11.0, 6.2, 4.2, 0.0)
    sites.append(s13)

    # 14. 노동·노서동 고분군 (봉황대 · 금관총 · 서봉총 · 식리총)
    s14 = BlenderSiteBuilder(
        "bonghwangdae_geumgwanchong", "봉황대 (높이 22m) · 금관총 · 서봉총", "노동·노서 고분",
        "단일 최대 고분 봉황대(높이 22m) · 신라 금관 최초 출토지 금관총 · 서봉총 · 식리총",
        35.841289, 129.210629, 25.0, (45, 150, 60), 16.0, 30.0, 560.0
    )
    s14.add_royal_tomb_mound(0.0, 0.0, 39.2, 22.0, has_curbstone=True)
    for tlat, tlon, r_m, h_m in (
        (35.840772, 129.210419, 16.0, 7.8),   # 식리총·금령총
        (35.841140, 129.209825, 21.0, 9.6),   # 금관총
        (35.841454, 129.208288, 32.0, 13.5),  # 서봉총·호우총
        (35.840638, 129.208299, 27.6, 11.8),  # 노서동 138호
    ):
        tx, ty = latlon_offset_to_meters(tlat - s14.lat, tlon - s14.lon)
        s14.add_royal_tomb_mound(tx, ty, r_m, h_m, has_curbstone=True)
    # Geumgwanchong preservation exhibition hall
    gx, gy = latlon_offset_to_meters(35.841860 - s14.lat, 129.209640 - s14.lon)
    s14.add_silla_hall(gx, gy, 32.0, 24.0, 8.0, 4.5, 0.0, pillar_col=GRANITE_MID)
    sites.append(s14)

    # 15. 쪽샘지구 신라 고분군 (황오·인왕동 고분군)
    s15 = BlenderSiteBuilder(
        "jjoksaem", "쪽샘지구 신라 고분군 (44호·마갑총 · 발굴관)", "쪽샘 왕릉군",
        "비단벌레 장식과 말갑옷이 출토된 황오·인왕동 적석목곽분 밀집 지구 및 쪽샘유적발굴관",
        35.840346, 129.215712, 16.0, (55, 155, 65), 14.0, 32.0, 580.0
    )
    for tlat, tlon, r_m, h_m in (
        (35.840650, 129.214863, 25.0, 10.2),
        (35.839259, 129.214734, 16.5, 7.0),
        (35.840346, 129.215712, 15.0, 6.5),
        (35.840200, 129.214876, 14.0, 6.2),
        (35.839985, 129.214797, 12.5, 5.6),
        (35.832720, 129.214949, 34.0, 13.2),
    ):
        tx, ty = latlon_offset_to_meters(tlat - s15.lat, tlon - s15.lon)
        s15.add_royal_tomb_mound(tx, ty, r_m, h_m, has_curbstone=True)
    jx, jy = latlon_offset_to_meters(35.840110 - s15.lat, 129.214160 - s15.lon)
    s15.add_silla_hall(jx, jy, 38.0, 32.0, 8.5, 4.8, 0.0, pillar_col=GRANITE_MID)
    sites.append(s15)

    # 16. 경주 오릉 (五陵) & 숭덕전 · 알영정 (사적 제172호)
    s16 = BlenderSiteBuilder(
        "oreung", "경주 오릉 (五陵) & 숭덕전 · 알영정 (사적 제172호)", "시조 왕릉",
        "박혁거세 거서간·알영부인·남해·유리·파사이사금 5기 능과 시조 사당 숭덕전",
        35.822158, 129.208669, 16.0, (45, 150, 60), 15.0, 32.0, 620.0
    )
    for tlat, tlon, r_m, h_m in (
        (35.822450, 129.208250, 24.0, 10.8),
        (35.822100, 129.208650, 19.0, 8.5),
        (35.821820, 129.208180, 18.0, 8.0),
        (35.822650, 129.208780, 17.5, 7.8),
        (35.821600, 129.208720, 16.5, 7.2),
    ):
        tx, ty = latlon_offset_to_meters(tlat - s16.lat, tlon - s16.lon)
        s16.add_royal_tomb_mound(tx, ty, r_m, h_m, has_curbstone=True)
    sx, sy = latlon_offset_to_meters(35.822150 - s16.lat, 129.209570 - s16.lon)
    s16.add_silla_hall(sx, sy, 20.0, 12.5, 6.5, 4.5, 0.0)
    sites.append(s16)

    # 17. 경주 포석정지 & 나정 · 양산재 (사적 제1·245호)
    s17 = BlenderSiteBuilder(
        "poseokjeong_najeong", "경주 나정 · 양산재 & 포석정지 (사적 제1·245호)", "시조탄강·연회유적",
        "박혁거세 탄강지 나정 팔각건물지 · 육부촌장 사당 양산재 · 전복 모양 유상곡수 포석정",
        35.815730, 129.212720, 14.0, (150, 95, 45), 14.0, 32.0, 680.0
    )
    # Najeong octagonal shrine & monument pavilion
    s17.add_cylinder(0.0, 0.0, 0.0, 1.2, 9.5, 9.5, 8, GRANITE_LIGHT)
    s17.add_cylinder(0.0, 0.0, 1.2, 6.5, 7.2, 7.2, 8, ROYAL_VERMILION)
    s17.add_cylinder(0.0, 0.0, 6.5, 10.8, 9.8, 0.4, 8, SILLA_ROOF_DARK)
    # Yangsanjae shrine
    yx, yy = latlon_offset_to_meters(35.816770 - s17.lat, 129.214290 - s17.lon)
    s17.add_silla_hall(yx, yy, 22.0, 13.0, 6.8, 4.6, 0.0)
    # Poseokjeong abalone-shaped stone channel (35.80711, 129.21252)
    px, py = latlon_offset_to_meters(35.807110 - s17.lat, 129.212520 - s17.lon)
    abalone_pts = []
    for i in range(24):
        ang = (2.0 * math.pi * i) / 24
        r = 7.5 * (1.0 + 0.25 * math.sin(2 * ang) + 0.15 * math.cos(3 * ang))
        abalone_pts.append((px + r * math.cos(ang), py + r * math.sin(ang)))
    s17.add_pond_polygon(abalone_pts, 0.45, POND_WATER, GRANITE_LIGHT)
    sites.append(s17)

    # 18. 낭산 일원 · 황복사지 삼층석탑 & 선덕여왕릉 & 사천왕사지 (국보 제37호·사적 제138·8호)
    s18 = BlenderSiteBuilder(
        "hwangboksa_nangsan", "황복사지 3층석탑 · 선덕여왕릉 · 사천왕사지", "낭산 왕실사찰",
        "국보 제37호 황복사지 삼층석탑 · 도리천 선덕여왕릉 · 호국 사천왕사지 쌍탑·금당 가람",
        35.832190, 129.242390, 16.0, (130, 125, 115), 16.0, 32.0, 880.0
    )
    # Hwangboksa 3-story stone pagoda at (0, 0)
    s18.add_box(0, 0, 0.0, 1.4, 8.5, 8.5, 0.0, GRANITE_MID)
    s18.add_box(0, 0, 1.4, 1.6, 6.2, 6.2, 0.0, GRANITE_LIGHT)
    pz = 3.0
    for st in range(3):
        w = 3.8 * (1.0 - 0.22 * st)
        h = 2.2 - 0.35 * st
        s18.add_box(0, 0, pz, h, w, w, 0.0, GRANITE_LIGHT)
        pz += h
        s18.add_box(0, 0, pz, 0.85, w * 1.42, w * 1.42, 0.0, GRANITE_MID)
        pz += 0.85
    s18.add_cylinder(0, 0, pz, pz + 2.4, 0.45, 0.15, 8, GOLD_SPIRE)
    # Queen Seondeok's Royal Tomb (35.823480, 129.242390)
    sx, sy = latlon_offset_to_meters(35.823480 - s18.lat, 129.242390 - s18.lon)
    s18.add_royal_tomb_mound(sx, sy, 23.0, 10.2, has_curbstone=True, has_table=True)
    # Sacheonwangsa Temple site twin wooden pagodas & Golden Hall (35.819450, 129.242750)
    scx, scy = latlon_offset_to_meters(35.819450 - s18.lat, 129.242750 - s18.lon)
    s18.add_silla_hall(scx, scy + 18.0, 30.0, 18.0, 9.0, 6.0, 0.0, golden_chimi=True)
    for side in (-22.0, 22.0):
        s18.add_silla_hall(scx + side, scy - 10.0, 14.0, 14.0, 12.0, 7.5, 0.0, two_story=True)
    sites.append(s18)

    # 19. 태종무열왕릉 (국보 무열왕릉비) & 김유신장군묘 (사적 제20·21호)
    s19 = BlenderSiteBuilder(
        "muyeol_kimyushin", "태종무열왕릉 (국보 무열왕릉비) & 김유신장군묘", "삼국통일 왕릉",
        "사적 제20호 태종무열왕릉 · 국보 제25호 귀부·이수 비각 · 사적 제21호 김유신장군묘",
        35.825270, 129.187390, 18.0, (45, 150, 60), 18.0, 30.0, 820.0
    )
    s19.add_royal_tomb_mound(0.0, 0.0, 26.5, 12.2, has_curbstone=True, has_table=True)
    # National Treasure No. 25 Stele Pavilion (35.825130, 129.188550)
    bx, by = latlon_offset_to_meters(35.825130 - s19.lat, 129.188550 - s19.lon)
    s19.add_silla_hall(bx, by, 10.5, 9.5, 5.6, 4.0, 0.0)
    # Kim In-mun tomb & Seoak-dong tombs
    kx, ky = latlon_offset_to_meters(35.824630 - s19.lat, 129.189320 - s19.lon)
    s19.add_royal_tomb_mound(kx, ky, 18.0, 8.2, has_curbstone=True)
    # General Kim Yu-sin's Tomb (35.845140, 129.191470)
    yx, yy = latlon_offset_to_meters(35.845140 - s19.lat, 129.191470 - s19.lon)
    s19.add_royal_tomb_mound(yx, yy, 18.5, 8.5, has_curbstone=True, has_table=True)
    sites.append(s19)

    # 20. 경주읍성 (향일문·성곽) & 성동동 전랑지 · 남고루 · 재매정 (사적 제96·88·246호)
    s20 = BlenderSiteBuilder(
        "gyeongju_eupseong", "경주읍성 (향일문·성곽) & 성동동 전랑지 · 재매정", "읍성·관아유적",
        "사적 제96호 경주읍성 성벽·여장·중층 문루 향일문 · 사적 제88호 성동동 전랑지 관아 회랑",
        35.847195, 129.213564, 18.0, (135, 95, 55), 14.0, 32.0, 720.0
    )
    s20.add_box(0.0, 0.0, 0.0, 6.2, 14.0, 210.0, 8.0, GRANITE_MID)
    s20.add_box(0.0, 0.0, 6.2, 1.3, 15.2, 210.0, 8.0, GRANITE_LIGHT)
    s20.add_silla_hall(0.0, 0.0, 15.0, 24.0, 9.8, 6.4, 8.0, two_story=True, golden_chimi=True, z_offset=5.2)
    # Seongdong-dong Jeollangji (Royal Banquet/Palace Corridor Site: 35.844778, 129.224142)
    jx, jy = latlon_offset_to_meters(35.844778 - s20.lat, 129.224142 - s20.lon)
    s20.add_silla_hall(jx, jy, 42.0, 18.0, 8.5, 5.6, -1.5, golden_chimi=True)
    s20.add_wall_segment(jx - 45.0, jy - 30.0, jx + 45.0, jy - 30.0, 3.8, 4.2)
    s20.add_wall_segment(jx - 45.0, jy + 30.0, jx + 45.0, jy + 30.0, 3.8, 4.2)
    # Jaemaejeong Well Pavilion (35.828910, 129.212950)
    mx, my = latlon_offset_to_meters(35.828910 - s20.lat, 129.212950 - s20.lon)
    s20.add_silla_hall(mx, my, 9.5, 9.5, 5.2, 3.6, 0.0)
    sites.append(s20)

    # -----------------------------------------------------------------------
    # 21 ~ 315+. 국가유산청(KHS) 경주시 전체 지정·등록 문화유산 전수 3D 모델링
    # (국보·보물·사적·천연기념물·국가민속문화유산·시도유형문화유산·시도기념물·문화유산자료 등)
    # -----------------------------------------------------------------------
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    khs_json_path = os.path.join(repo_root, "data", "models", "gyeongju_khs_source.json")
    if os.path.exists(khs_json_path):
        with open(khs_json_path, "r", encoding="utf-8") as kf:
            khs_records = json.load(kf)

        # 이미 1~20번 대표 복합유적의 중심점으로 직접 생성된 핵심 항목 명칭은 중복 방지
        covered_exact_names = {
            "경주 황룡사지", "경주 분황사 모전석탑", "경주 분황사지",
            "경주 구황동 원지 유적 일원", "경주 동궁과 월지", "경주 월성",
            "경주 석빙고", "경주 첨성대", "경주 춘양교지와 월정교지",
            "경주 계림", "경주 내물왕릉", "경주향교 대성전",
            "성덕대왕신종", "경주 대릉원 일원", "경주 미추왕릉",
            "경주 오릉", "경주 나정", "경주 포석정지",
            "경주 황복사지 삼층석탑", "경주 무열왕릉", "경주 김유신묘",
            "경주읍성"
        }

        coord_counts = {}
        for rec in khs_records:
            rname = rec["name"].strip()
            if rname in covered_exact_names:
                continue
            rcat = rec["category"].strip()
            rlat = float(rec["lat"])
            rlon = float(rec["lon"])
            kid = rec["khs_id"]

            # 동일 좌표(예: 국립경주박물관 소장 국보·보물 또는 동일 경내 유산)는 원형/격자 오프셋 배치
            ckey = (round(rlat, 4), round(rlon, 4))
            cidx = coord_counts.get(ckey, 0)
            coord_counts[ckey] = cidx + 1
            if cidx > 0:
                ring = (cidx - 1) // 8 + 1
                slot = (cidx - 1) % 8
                ang = (2.0 * math.pi * slot) / 8.0 + ring * 0.25
                dist_m = 16.0 * ring
                rlat += (dist_m * math.sin(ang)) / 110950.0
                rlon += (dist_m * math.cos(ang)) / (111319.5 * math.cos(math.radians(35.835)))

            # 카테고리별 배지 색상
            if "국보" in rcat:
                badge = (205, 55, 40)
            elif "보물" in rcat:
                badge = (185, 115, 35)
            elif "사적" in rcat:
                badge = (45, 140, 85)
            elif "천연기념물" in rcat:
                badge = (40, 155, 70)
            elif "민속" in rcat:
                badge = (145, 95, 50)
            else:
                badge = (70, 125, 175)

            display_title = f"[{rcat}] {rname}"
            sb = classify_and_build_khs_site(kid, display_title, rcat, rname, rlat, rlon, badge)
            sites.append(sb)

    return sites


def classify_and_build_khs_site(site_id: str, display_title: str, rcat: str, rname: str,
                                lat: float, lon: float, badge: tuple[int, int, int]) -> BlenderSiteBuilder:
    """Build an authentic archetype-matched 3D monument in Blender BMesh for any Gyeongju KHS record."""
    # 1. 다보탑 / 석탑 / 모전석탑 / 사리탑 / 부도탑
    if any(k in rname for k in ("다보탑", "석탑", "사리탑", "부도탑", "탑신석")):
        stories = 3
        if "십삼층" in rname or "13층" in rname:
            stories = 13
        elif "오층" in rname or "5층" in rname:
            stories = 5
        elif "사리탑" in rname or "부도" in rname or "탑신석" in rname:
            stories = 1
        sub = f"경주 {rcat} · 신라 화강암 {stories}층 불탑 및 이중기단·상륜부 3D 복원"
        b = BlenderSiteBuilder(site_id, display_title, rcat, sub, lat, lon, 14.0, badge, 15.0, 28.0, 220.0)
        if "다보탑" in rname:
            # 국보 제20호 불국사 다보탑 (십자형 계단 기단 + 대나무형 기둥 + 팔각 난간·옥개 + 상륜)
            b.add_box(0, 0, 0.0, 1.8, 6.2, 6.2, 0.0, GRANITE_LIGHT)
            for ang_d in (0.0, 90.0):
                b.add_box(0, 0, 0.4, 1.4, 8.2, 2.2, ang_d, GRANITE_MID)
            for px in (-1.5, 1.5):
                for py in (-1.5, 1.5):
                    b.add_box(px, py, 1.8, 2.8, 0.55, 0.55, 0.0, GRANITE_LIGHT)
            b.add_box(0, 0, 4.6, 0.8, 4.6, 4.6, 0.0, GRANITE_MID)
            b.add_cylinder(0, 0, 5.4, 7.2, 2.1, 1.9, 8, GRANITE_LIGHT)
            b.add_cylinder(0, 0, 7.2, 8.4, 3.0, 0.6, 8, GRANITE_MID)
            b.add_cylinder(0, 0, 8.4, 10.6, 0.5, 0.15, 8, GOLD_SPIRE)
        elif stories == 1:
            b.add_cylinder(0, 0, 0.0, 1.2, 2.6, 2.2, 8, GRANITE_MID)
            b.add_cylinder(0, 0, 1.2, 3.2, 1.4, 1.4, 8, GRANITE_LIGHT)
            b.add_cylinder(0, 0, 3.2, 4.2, 2.2, 0.4, 8, GRANITE_DARK)
            b.add_cylinder(0, 0, 4.2, 5.4, 0.35, 0.1, 8, GOLD_SPIRE)
        else:
            is_twin = ("동·서" in rname or "동ㆍ서" in rname)
            offsets = (-10.0, 10.0) if is_twin else (0.0,)
            for ox in offsets:
                base_w = 7.2 if stories >= 5 else 5.8
                b.add_box(ox, 0, 0.0, 1.1, base_w, base_w, 0.0, GRANITE_MID)
                b.add_box(ox, 0, 1.1, 1.3, base_w * 0.74, base_w * 0.74, 0.0, GRANITE_LIGHT)
                pz = 2.4
                for st in range(stories):
                    ratio = max(0.32, 1.0 - (0.58 * st) / max(1, stories))
                    w = base_w * 0.48 * ratio
                    h = (1.65 if stories <= 5 else 0.92) * ratio
                    b.add_box(ox, 0, pz, h, w, w, 0.0, GRANITE_LIGHT)
                    pz += h
                    b.add_box(ox, 0, pz, 0.55 if stories <= 5 else 0.38, w * 1.38, w * 1.38, 0.0, GRANITE_DARK)
                    pz += (0.55 if stories <= 5 else 0.38)
                b.add_cylinder(ox, 0, pz, pz + 2.2, 0.38, 0.12, 8, GOLD_SPIRE)
        return b

    # 2. 불국사 청운교·백운교 / 연화교·칠보교 / 가구식 석축
    if any(k in rname for k in ("청운교", "연화교", "석축", "효불효교")):
        sub = f"경주 {rcat} · 신라 화강암 홍예 석교 및 석축 계단 3D 복원"
        b = BlenderSiteBuilder(site_id, display_title, rcat, sub, lat, lon, 13.0, badge, 12.0, 28.0, 240.0)
        b.add_box(0, 0, 0.0, 4.8, 28.0, 8.0, 0.0, GRANITE_MID)
        for st in range(6):
            b.add_box(0, -4.0 - st * 1.2, st * 0.75, 0.8, 5.2, 1.4, 0.0, GRANITE_LIGHT)
        b.add_silla_hall(0, 2.0, 14.0, 8.0, 5.2, 3.8, 0.0, z_offset=4.8)
        return b

    # 3. 석굴암 석굴
    if "석굴암 석굴" in rname:
        sub = "국보 제24호 · 토함산 화강암 원형 돔 주실·본존불 및 목조 전실 3D 복원"
        b = BlenderSiteBuilder(site_id, display_title, rcat, sub, lat, lon, 15.0, badge, 20.0, 28.0, 260.0)
        b.add_cylinder(0, 4.0, 0.0, 6.8, 7.5, 5.2, 16, MOUND_GRASS_TOP)
        b.add_cylinder(0, 4.0, 0.0, 4.5, 4.8, 4.2, 12, GRANITE_LIGHT)
        b.add_silla_hall(0, -3.5, 13.5, 8.5, 5.4, 3.8, 0.0)
        return b

    # 4. 문무대왕릉 (수중릉 대왕암)
    if "문무대왕릉" in rname:
        sub = "사적 제158호 · 동해 대왕암 바위섬 및 십자형 수로·수중 석실 3D 복원"
        b = BlenderSiteBuilder(site_id, display_title, rcat, sub, lat, lon, 14.0, badge, 25.0, 32.0, 360.0)
        for ang_i in range(8):
            ang = (2.0 * math.pi * ang_i) / 8.0
            rx, ry = 14.0 * math.cos(ang), 14.0 * math.sin(ang)
            b.add_cylinder(rx, ry, 0.0, 4.2, 8.5, 5.2, 7, GRANITE_MID)
        b.add_cylinder(0, 0, 0.0, 1.2, 9.0, 9.0, 12, POND_WATER)
        b.add_box(0, 0, 0.8, 1.4, 3.8, 2.6, 0.0, GRANITE_LIGHT)
        return b

    # 5. 왕릉 · 고분군 · 방형분 · 묘
    if any(k in rname for k in ("왕릉", "여왕릉", "고분", "방형분", "삼릉", "삼안총", "간묘", "유신묘", "인문묘", "김양묘", "설총묘", "부조묘")):
        sub = f"경주 {rcat} · 신라 봉토분·호석(둘레돌)·상석 및 능역 3D 복원"
        b = BlenderSiteBuilder(site_id, display_title, rcat, sub, lat, lon, 16.0, badge, 18.0, 30.0, 340.0)
        if "방형분" in rname:
            b.add_box(0, 0, 0.0, 1.8, 13.0, 13.0, 0.0, GRANITE_MID)
            b.add_box(0, 0, 1.8, 4.5, 11.0, 11.0, 0.0, MOUND_GRASS_TOP)
        elif "삼릉" in rname:
            for ox in (-24.0, 0.0, 24.0):
                b.add_royal_tomb_mound(ox, 0.0, 11.5, 5.8, has_curbstone=True, has_table=(ox == 0.0))
        elif "고분군" in rname:
            for ox, oy, rm, hm in ((-22.0, -12.0, 15.0, 7.2), (0.0, 0.0, 18.5, 8.8),
                                   (24.0, 10.0, 14.0, 6.5), (-10.0, 22.0, 12.5, 5.8)):
                b.add_royal_tomb_mound(ox, oy, rm, hm, has_curbstone=True)
        else:
            b.add_royal_tomb_mound(0.0, 0.0, 17.5, 8.4, has_curbstone=True, has_table=True)
            # 문인석·무인석·석주 (원성왕릉·성덕왕릉·흥덕왕릉 등)
            for side in (-5.5, 5.5):
                b.add_box(side, -22.0, 0.0, 2.6, 0.9, 0.9, 0.0, GRANITE_LIGHT)
                b.add_box(side, -26.0, 0.0, 2.5, 0.9, 0.9, 0.0, GRANITE_MID)
        return b

    # 6. 당간지주
    if "당간지주" in rname:
        sub = f"경주 {rcat} · 신라 사찰 화강암 당간지주 쌍기둥 및 간대석 3D 복원"
        b = BlenderSiteBuilder(site_id, display_title, rcat, sub, lat, lon, 12.0, badge, 15.0, 26.0, 180.0)
        b.add_box(0, 0, 0.0, 0.6, 3.2, 2.2, 0.0, GRANITE_MID)
        b.add_box(-0.75, 0, 0.6, 4.5, 0.65, 1.05, 0.0, GRANITE_LIGHT)
        b.add_box(0.75, 0, 0.6, 4.5, 0.65, 1.05, 0.0, GRANITE_LIGHT)
        b.add_cylinder(0, 0, 0.6, 6.2, 0.22, 0.14, 8, BRONZE_BELL)
        return b

    # 7. 마애불 · 석조여래 · 보살 · 사면불 · 석불 · 불상
    if any(k in rname for k in ("마애", "불상", "여래", "보살", "석불", "입불", "사면불", "육존불", "칠불암")):
        sub = f"경주 {rcat} · 신라 화강암 연화대좌·광배·불상 3D 복원"
        b = BlenderSiteBuilder(site_id, display_title, rcat, sub, lat, lon, 12.0, badge, 15.0, 26.0, 190.0)
        if "사면불" in rname or "마애" in rname or "칠불암" in rname or "탑곡" in rname:
            b.add_box(0, 1.2, 0.0, 5.2, 5.6, 3.8, 0.0, GRANITE_MID)
        is_triad = ("삼존" in rname or "칠불" in rname)
        offsets = (-2.2, 0.0, 2.2) if is_triad else (0.0,)
        for ox in offsets:
            sc = 1.0 if ox == 0.0 else 0.75
            b.add_cylinder(ox, 0, 0.0, 1.1 * sc, 1.6 * sc, 1.3 * sc, 8, GRANITE_DARK)
            b.add_cylinder(ox, 0, 1.1 * sc, 3.4 * sc, 0.95 * sc, 0.72 * sc, 10, GRANITE_LIGHT)
            b.add_cylinder(ox, 0, 3.4 * sc, 4.3 * sc, 0.52 * sc, 0.42 * sc, 10, GRANITE_LIGHT)
            b.add_box(ox, 0.35 * sc, 1.2 * sc, 3.4 * sc, 2.1 * sc, 0.28 * sc, 0.0, GRANITE_MID)
        return b

    # 8. 산성 · 성곽 · 남고루 · 관문성 · 명활성 · 부산성
    if any(k in rname for k in ("명활성", "남산신성", "부산성", "관문성", "남고루")):
        sub = f"경주 {rcat} · 신라 석축 산성 성벽·여장 및 망루 3D 복원"
        b = BlenderSiteBuilder(site_id, display_title, rcat, sub, lat, lon, 16.0, badge, 20.0, 30.0, 420.0)
        b.add_wall_segment(-45.0, 0.0, 45.0, 0.0, 5.5, 6.2, GRANITE_MID, GRANITE_LIGHT)
        b.add_silla_hall(0.0, 0.0, 14.0, 10.0, 6.5, 4.5, 0.0, two_story=True, z_offset=5.2)
        return b

    # 9. 사지(절터) · 사찰 · 대웅전 · 대적광전 · 약사전 · 응진전 · 진남루
    if any(k in rname for k in ("사지", "불국사", "기림사", "백률사", "석굴암", "사적지대")):
        sub = f"경주 {rcat} · 신라 사찰 금당 기단·목조 불전 및 가람 3D 복원"
        b = BlenderSiteBuilder(site_id, display_title, rcat, sub, lat, lon, 15.0, badge, 16.0, 30.0, 320.0)
        if "사지" in rname:
            b.add_box(0, 8.0, 0.0, 1.4, 24.0, 15.0, 0.0, GRANITE_LIGHT)
            b.add_silla_hall(0, 8.0, 20.0, 12.0, 7.2, 4.8, 0.0)
            b.add_box(0, -8.0, 0.0, 3.6, 2.2, 2.2, 0.0, GRANITE_MID)
        else:
            b.add_silla_hall(0, 0, 22.0, 14.0, 7.8, 5.2, 0.0, golden_chimi=True)
        return b

    # 10. 비석 · 귀부 · 석조 · 석정 · 서출지 · 요지 · 암각화 · 천연기념물 노거수
    if any(k in rname for k in ("비", "귀부", "석조", "석정", "서출지", "요지", "암각화", "표암")) or "천연기념물" in rcat:
        sub = f"경주 {rcat} · 실측 위치 3D 입체 복원"
        b = BlenderSiteBuilder(site_id, display_title, rcat, sub, lat, lon, 11.0, badge, 15.0, 28.0, 200.0)
        if "천연기념물" in rcat or "나무" in rname:
            if "주상절리" in rname:
                for i in range(12):
                    ang = (2.0 * math.pi * i) / 12.0
                    b.add_cylinder(6.0 * math.cos(ang), 6.0 * math.sin(ang), 0.0, 4.5, 1.6, 1.4, 6, ANDESITE_BRICK)
            else:
                b.add_sacred_tree(0, 0, 14.0, 6.8)
        elif "서출지" in rname:
            b.add_pond_polygon([(-14, -10), (14, -10), (16, 10), (-14, 12)], 0.5, POND_WATER, GRANITE_MID)
            b.add_silla_hall(-12.0, 6.0, 9.5, 7.0, 5.0, 3.6, 0.0, pillar_col=HANOK_TIMBER)
        elif "석조" in rname or "석정" in rname:
            b.add_box(0, 0, 0.0, 1.6, 4.2, 2.8, 0.0, GRANITE_LIGHT)
            b.add_box(0, 0, 1.2, 0.5, 3.2, 1.9, 0.0, POND_WATER)
        elif "요지" in rname:
            for st in range(5):
                b.add_cylinder(0, st * 3.2 - 6.0, st * 0.6, st * 0.6 + 1.8, 2.4, 1.8, 8, EARTH_RAMPART)
        elif "암각화" in rname or "표암" in rname:
            b.add_box(0, 0, 0.0, 3.8, 7.5, 4.2, 12.0, GRANITE_DARK)
        else:
            # 귀부·이수·비신 및 비각
            b.add_box(0, 0, 0.0, 1.1, 2.8, 3.6, 0.0, GRANITE_MID)
            b.add_box(0, 0, 1.1, 3.2, 1.5, 0.45, 0.0, GRANITE_DARK)
            b.add_box(0, 0, 4.3, 0.9, 1.8, 0.75, 0.0, GRANITE_LIGHT)
        return b

    # 11. 박물관 소장 국보·보물 유물 (금관·관모·허리띠·뿔잔·환두대도·성덕대왕신종 권역 등)
    if any(k in rname for k in ("금관", "관모", "관식", "허리띠", "귀걸이", "목걸이", "유리잔", "환두대도",
                                "자루솥", "고배", "팔뚝", "드리개", "유리구슬", "보검", "명기", "뿔잔",
                                "발걸이", "청동기", "서기석", "사리장엄구", "묘법연화경", "삼국사기",
                                "수고본", "유묵", "전적", "책판", "논상록권", "분재기", "영정", "불회도", "소장유물")):
        sub = f"경주 {rcat} · 신라 왕실 출토 유물 및 석조 전시대 3D 복원"
        b = BlenderSiteBuilder(site_id, display_title, rcat, sub, lat, lon, 9.0, badge, 15.0, 26.0, 150.0)
        b.add_box(0, 0, 0.0, 1.6, 3.2, 3.2, 0.0, GRANITE_LIGHT)
        b.add_cylinder(0, 0, 1.6, 3.6, 1.1, 1.25, 10, GOLD_SPIRE)
        for sp in range(5):
            ang = (2.0 * math.pi * sp) / 5.0
            b.add_box(0.95 * math.cos(ang), 0.95 * math.sin(ang), 3.4, 1.8, 0.28, 0.28, 0.0, GOLD_SPIRE)
        return b

    # 12. 기본: 양동마을 고택 · 종택 · 서원 · 서당 · 정자 · 향교 · 전(숭덕전·숭신전·숭혜전) 등 전통 목조 한옥
    sub = f"경주 {rcat} · 전통 목조 기와 건축 및 담장·정문 3D 복원"
    b = BlenderSiteBuilder(site_id, display_title, rcat, sub, lat, lon, 13.0, badge, 16.0, 30.0, 260.0)
    b.add_silla_hall(0.0, 2.0, 16.5, 10.5, 5.8, 4.0, 0.0, pillar_col=HANOK_TIMBER)
    b.add_silla_hall(0.0, -8.5, 12.0, 6.0, 4.2, 2.8, 0.0, pillar_col=HANOK_TIMBER)
    return b


def main():
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    out_dir = os.path.join(repo_root, "data", "models")
    ply_dir = os.path.join(out_dir, "gyeongju")
    os.makedirs(ply_dir, exist_ok=True)

    print(f"[Blender {bpy.app.version_string}] Generating 20 Gyeongju 3D Heritage Models...")
    sites = build_all_gyeongju_sites()

    bin_path = os.path.join(out_dir, "gyeongju_blender_heritage.s3dmesh")
    manifest_path = os.path.join(out_dir, "gyeongju_heritage_manifest.json")

    manifest = {
        "generator": f"Blender {bpy.app.version_string} (bpy + bmesh)",
        "format": "S3DMESH_V1",
        "siteCount": len(sites),
        "sites": []
    }

    total_verts = 0
    total_tris = 0

    with open(bin_path, "wb") as bf:
        # Magic header: "S3DMESH1" (8 bytes) + uint32 site_count
        bf.write(b"S3DMESH1")
        bf.write(struct.pack("<I", len(sites)))

        for s in sites:
            verts, idxs = s.finalize_and_extract()
            n_verts = len(verts) // 10
            n_idxs = len(idxs)
            total_verts += n_verts
            total_tris += n_idxs // 3

            # Export standalone PLY file for flagship complex sites
            if not s.site_id.startswith("khs_"):
                ply_path = os.path.join(ply_dir, f"{s.site_id}.ply")
                s.export_ply(ply_path, verts, idxs)

            # Write binary site record
            def write_str(text: str):
                b = text.encode("utf-8")
                bf.write(struct.pack("<I", len(b)))
                bf.write(b)

            write_str(s.site_id)
            write_str(s.name)
            write_str(s.category)
            write_str(s.subtitle)
            bf.write(struct.pack(
                "<ddffffBBBBII",
                s.lat, s.lon,
                s.label_h, s.cam_yaw, s.cam_pitch, s.cam_dist,
                s.badge_rgb[0], s.badge_rgb[1], s.badge_rgb[2], 255,
                len(verts), len(idxs)
            ))
            bf.write(struct.pack(f"<{len(verts)}f", *verts))
            bf.write(struct.pack(f"<{len(idxs)}I", *idxs))

            manifest["sites"].append({
                "id": s.site_id,
                "name": s.name,
                "category": s.category,
                "subtitle": s.subtitle,
                "lat": s.lat,
                "lon": s.lon,
                "labelHeightM": s.label_h,
                "badgeColor": list(s.badge_rgb),
                "camera": {"yaw": s.cam_yaw, "pitch": s.cam_pitch, "distM": s.cam_dist},
                "vertices": n_verts,
                "triangles": n_idxs // 3,
                "plyFile": f"data/models/gyeongju/{s.site_id}.ply"
            })
            print(f"  [OK] {s.site_id:28s} | {s.name} | {n_verts:5d} verts | {n_idxs // 3:4d} tris")
            s.free()

    manifest["totalVertices"] = total_verts
    manifest["totalTriangles"] = total_tris
    with open(manifest_path, "w", encoding="utf-8") as mf:
        json.dump(manifest, mf, ensure_ascii=False, indent=2)

    print(f"[DONE] Exported {len(sites)} sites ({total_verts} vertices, {total_tris} triangles) to {bin_path}")


if __name__ == "__main__":
    main()
