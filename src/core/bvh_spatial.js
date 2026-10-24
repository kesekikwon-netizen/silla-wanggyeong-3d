/**
 * @file bvh_spatial.js
 * @description Blender 4.2.3 REA-derived Spatial Acceleration Engine:
 * - PBVHTree: Port of `bf_blenlib.lib` (`BLI_kdopbvh.c`) & `bf_blenkernel.lib` (`pbvh.cc`)
 *   for O(log N) terrain/building raycasting, viewshed occlusion, and frustum culling.
 * - KDTree3D: Port of `bf_blenlib.lib` (`kdtree_3d.c` / `PyInit_mathutils_kdtree`)
 *   for instantaneous 500m / 1,000m archaeological site proximity search.
 * - Round Buffer Generator (`JoinStyle::Round` equivalent for 500m/1,000m heritage buffers).
 */

const EPSILON = 1e-7;

export class PBVHTree {
  /**
   * @param {Array<{
   *   v0: [number, number, number],
   *   v1: [number, number, number],
   *   v2: [number, number, number],
   *   id?: number|string,
   *   chunkId?: string
   * }>} triangles
   * @param {number} [leafMaxSize=12]
   */
  constructor(triangles = [], leafMaxSize = 12) {
    this.leafMaxSize = leafMaxSize;
    this.triangles = triangles;
    this.nodes = [];
    if (triangles.length > 0) {
      this.build(triangles);
    }
  }

  build(triangles) {
    this.triangles = triangles;
    this.nodes = [];
    const items = triangles.map((tri, index) => {
      const minX = Math.min(tri.v0[0], tri.v1[0], tri.v2[0]);
      const minY = Math.min(tri.v0[1], tri.v1[1], tri.v2[1]);
      const minZ = Math.min(tri.v0[2], tri.v1[2], tri.v2[2]);
      const maxX = Math.max(tri.v0[0], tri.v1[0], tri.v2[0]);
      const maxY = Math.max(tri.v0[1], tri.v1[1], tri.v2[1]);
      const maxZ = Math.max(tri.v0[2], tri.v1[2], tri.v2[2]);
      return {
        index,
        min: [minX, minY, minZ],
        max: [maxX, maxY, maxZ],
        centroid: [
          (tri.v0[0] + tri.v1[0] + tri.v2[0]) / 3.0,
          (tri.v0[1] + tri.v1[1] + tri.v2[1]) / 3.0,
          (tri.v0[2] + tri.v1[2] + tri.v2[2]) / 3.0,
        ],
      };
    });

    if (items.length > 0) {
      this._buildRecursive(items, 0, items.length);
    }
    return this;
  }

  _buildRecursive(items, start, end) {
    const nodeIndex = this.nodes.length;
    let minX = Infinity,
      minY = Infinity,
      minZ = Infinity;
    let maxX = -Infinity,
      maxY = -Infinity,
      maxZ = -Infinity;
    let cMinX = Infinity,
      cMinY = Infinity,
      cMinZ = Infinity;
    let cMaxX = -Infinity,
      cMaxY = -Infinity,
      cMaxZ = -Infinity;

    for (let i = start; i < end; i++) {
      const it = items[i];
      if (it.min[0] < minX) minX = it.min[0];
      if (it.min[1] < minY) minY = it.min[1];
      if (it.min[2] < minZ) minZ = it.min[2];
      if (it.max[0] > maxX) maxX = it.max[0];
      if (it.max[1] > maxY) maxY = it.max[1];
      if (it.max[2] > maxZ) maxZ = it.max[2];

      if (it.centroid[0] < cMinX) cMinX = it.centroid[0];
      if (it.centroid[1] < cMinY) cMinY = it.centroid[1];
      if (it.centroid[2] < cMinZ) cMinZ = it.centroid[2];
      if (it.centroid[0] > cMaxX) cMaxX = it.centroid[0];
      if (it.centroid[1] > cMaxY) cMaxY = it.centroid[1];
      if (it.centroid[2] > cMaxZ) cMaxZ = it.centroid[2];
    }

    const count = end - start;
    const node = {
      min: [minX, minY, minZ],
      max: [maxX, maxY, maxZ],
      left: -1,
      right: -1,
      triIndices: null,
    };
    this.nodes.push(node);

    if (count <= this.leafMaxSize) {
      node.triIndices = [];
      for (let i = start; i < end; i++) {
        node.triIndices.push(items[i].index);
      }
      return nodeIndex;
    }

    // Split along longest centroid extent axis (XZ horizontal priority for terrain)
    const extX = cMaxX - cMinX;
    const extY = cMaxY - cMinY;
    const extZ = cMaxZ - cMinZ;
    let axis = 0;
    if (extZ >= extX && extZ >= extY) axis = 2;
    else if (extY > extX && extY > extZ) axis = 1;

    const slice = items.slice(start, end).sort((a, b) => a.centroid[axis] - b.centroid[axis]);
    for (let i = 0; i < slice.length; i++) {
      items[start + i] = slice[i];
    }

    const mid = start + (count >> 1);
    node.left = this._buildRecursive(items, start, mid);
    node.right = this._buildRecursive(items, mid, end);
    return nodeIndex;
  }

  /**
   * Slab-based Ray-AABB intersection test (`BLI_bvhtree_ray_cast` inner kernel).
   */
  _intersectAABB(origin, invDir, min, max, maxT) {
    let tmin = -Infinity;
    let tmax = maxT;

    for (let a = 0; a < 3; a++) {
      const t1 = (min[a] - origin[a]) * invDir[a];
      const t2 = (max[a] - origin[a]) * invDir[a];
      const tNear = Math.min(t1, t2);
      const tFar = Math.max(t1, t2);
      if (tNear > tmin) tmin = tNear;
      if (tFar < tmax) tmax = tFar;
      if (tmax < tmin || tmax < 0) return Infinity;
    }
    return tmin >= 0 ? tmin : 0;
  }

  /**
   * Möller–Trumbore ray-triangle intersection.
   */
  _intersectTriangle(origin, dir, tri, maxT) {
    const [ax, ay, az] = tri.v0;
    const [bx, by, bz] = tri.v1;
    const [cx, cy, cz] = tri.v2;

    const e1x = bx - ax,
      e1y = by - ay,
      e1z = bz - az;
    const e2x = cx - ax,
      e2y = cy - ay,
      e2z = cz - az;

    const px = dir[1] * e2z - dir[2] * e2y;
    const py = dir[2] * e2x - dir[0] * e2z;
    const pz = dir[0] * e2y - dir[1] * e2x;

    const det = e1x * px + e1y * py + e1z * pz;
    if (Math.abs(det) < EPSILON) return null;

    const invDet = 1.0 / det;
    const tx = origin[0] - ax;
    const ty = origin[1] - ay;
    const tz = origin[2] - az;

    const u = (tx * px + ty * py + tz * pz) * invDet;
    if (u < 0.0 || u > 1.0) return null;

    const qx = ty * e1z - tz * e1y;
    const qy = tz * e1x - tx * e1z;
    const qz = tx * e1y - ty * e1x;

    const v = (dir[0] * qx + dir[1] * qy + dir[2] * qz) * invDet;
    if (v < 0.0 || u + v > 1.0) return null;

    const t = (e2x * qx + e2y * qy + e2z * qz) * invDet;
    if (t <= EPSILON || t >= maxT) return null;

    // Face normal
    let nx = e1y * e2z - e1z * e2y;
    let ny = e1z * e2x - e1x * e2z;
    let nz = e1x * e2y - e1y * e2x;
    const nLen = Math.hypot(nx, ny, nz) || 1.0;
    nx /= nLen;
    ny /= nLen;
    nz /= nLen;
    if (ny < 0) {
      nx = -nx;
      ny = -ny;
      nz = -nz;
    }

    return {
      t,
      point: [origin[0] + dir[0] * t, origin[1] + dir[1] * t, origin[2] + dir[2] * t],
      normal: [nx, ny, nz],
    };
  }

  /**
   * Perform O(log N) raycast against the BVH tree.
   * @param {[number,number,number]} origin
   * @param {[number,number,number]} direction - Unit direction vector
   * @param {number} [maxDistance=Infinity]
   */
  raycast(origin, direction, maxDistance = Infinity) {
    if (this.nodes.length === 0) {
      return { hit: false, distance: Infinity, point: null, normal: null, triIndex: -1, nodeVisits: 0 };
    }

    const len = Math.hypot(direction[0], direction[1], direction[2]) || 1.0;
    const dir = [direction[0] / len, direction[1] / len, direction[2] / len];
    const invDir = [
      Math.abs(dir[0]) > 1e-12 ? 1.0 / dir[0] : 1e12,
      Math.abs(dir[1]) > 1e-12 ? 1.0 / dir[1] : 1e12,
      Math.abs(dir[2]) > 1e-12 ? 1.0 / dir[2] : 1e12,
    ];

    let closestT = maxDistance;
    let bestPoint = null;
    let bestNormal = null;
    let bestTriIndex = -1;
    let nodeVisits = 0;

    const stack = [0];
    while (stack.length > 0) {
      const nodeIdx = stack.pop();
      const node = this.nodes[nodeIdx];
      nodeVisits++;

      const boxDist = this._intersectAABB(origin, invDir, node.min, node.max, closestT);
      if (boxDist === Infinity) continue;

      if (node.triIndices !== null) {
        for (let i = 0; i < node.triIndices.length; i++) {
          const triIdx = node.triIndices[i];
          const res = this._intersectTriangle(origin, dir, this.triangles[triIdx], closestT);
          if (res && res.t < closestT) {
            closestT = res.t;
            bestPoint = res.point;
            bestNormal = res.normal;
            bestTriIndex = triIdx;
          }
        }
      } else {
        const leftNode = this.nodes[node.left];
        const rightNode = this.nodes[node.right];
        const dLeft = this._intersectAABB(origin, invDir, leftNode.min, leftNode.max, closestT);
        const dRight = this._intersectAABB(origin, invDir, rightNode.min, rightNode.max, closestT);

        // Push farther child first so nearer child is popped and tested first
        if (dLeft < dRight) {
          if (dRight < closestT) stack.push(node.right);
          if (dLeft < closestT) stack.push(node.left);
        } else {
          if (dLeft < closestT) stack.push(node.left);
          if (dRight < closestT) stack.push(node.right);
        }
      }
    }

    return {
      hit: bestTriIndex !== -1,
      distance: closestT,
      point: bestPoint,
      normal: bestNormal,
      triIndex: bestTriIndex,
      nodeVisits,
    };
  }

  /**
   * Test 3D line-of-sight visibility between observer (p1) and target landmark (p2).
   * Used for Gyeongju Royal Capital Viewshed Evaluation (e.g., Wolseong <-> Hwangnyongsa <-> Namsan).
   * @param {[number,number,number]} p1 - [worldX, worldY, worldZ] observer eye point
   * @param {[number,number,number]} p2 - [worldX, worldY, worldZ] target landmark point
   */
  hasLineOfSight(p1, p2) {
    const dx = p2[0] - p1[0];
    const dy = p2[1] - p1[1];
    const dz = p2[2] - p1[2];
    const dist = Math.hypot(dx, dy, dz);
    if (dist < 1.0) return { visible: true, distanceM: dist, obstructionPoint: null };

    const dir = [dx / dist, dy / dist, dz / dist];
    // Start 2m along ray and stop 2m before target to avoid self-intersection with ground at endpoints
    const startOrigin = [p1[0] + dir[0] * 2.0, p1[1] + dir[1] * 2.0, p1[2] + dir[2] * 2.0];
    const res = this.raycast(startOrigin, dir, Math.max(0.1, dist - 4.0));
    return {
      visible: !res.hit,
      distanceM: dist,
      obstructionPoint: res.hit ? res.point : null,
      obstructionDistanceM: res.hit ? res.distance + 2.0 : null,
      nodeVisits: res.nodeVisits,
    };
  }

  /**
   * 6-Plane Camera Frustum Culling (`draw_pbvh.cc` equivalent).
   * Each plane is `{ normal: [nx, ny, nz], d: number }` where `nx*x + ny*y + nz*z + d >= 0` is inside.
   */
  cullFrustum(planes) {
    if (this.nodes.length === 0) return { visibleLeafCount: 0, totalLeafCount: 0, visibleTriIndices: [] };

    const visibleTriIndices = [];
    let visibleLeafCount = 0;
    const stack = [0];

    while (stack.length > 0) {
      const idx = stack.pop();
      const node = this.nodes[idx];

      let outside = false;
      for (let p = 0; p < planes.length; p++) {
        const pl = planes[p];
        const px = pl.normal[0] >= 0 ? node.max[0] : node.min[0];
        const py = pl.normal[1] >= 0 ? node.max[1] : node.min[1];
        const pz = pl.normal[2] >= 0 ? node.max[2] : node.min[2];
        if (pl.normal[0] * px + pl.normal[1] * py + pl.normal[2] * pz + pl.d < 0) {
          outside = true;
          break;
        }
      }
      if (outside) continue;

      if (node.triIndices !== null) {
        visibleLeafCount++;
        for (let i = 0; i < node.triIndices.length; i++) {
          visibleTriIndices.push(node.triIndices[i]);
        }
      } else {
        stack.push(node.left, node.right);
      }
    }

    return {
      visibleLeafCount,
      visibleTriIndices,
    };
  }
}

/**
 * 3D KD-Tree port of Blender `bf_blenlib.lib: kdtree_3d.c` (`PyInit_mathutils_kdtree`).
 * Provides O(log N) `findNearest` and `findRange` for archaeological site proximity queries.
 */
export class KDTree3D {
  /**
   * @param {Array<{x: number, y: number, z?: number, data: any}>} points
   */
  constructor(points = []) {
    this.root = null;
    this.size = points.length;
    if (points.length > 0) {
      this.balance(points);
    }
  }

  balance(points) {
    const copy = points.map((p, idx) => ({
      coords: [p.x, p.y, p.z ?? 0.0],
      data: p.data ?? p,
      index: idx,
    }));
    this.size = copy.length;
    this.root = this._buildNode(copy, 0);
    return this;
  }

  _buildNode(items, depth) {
    if (items.length === 0) return null;
    const axis = depth % 3;
    items.sort((a, b) => a.coords[axis] - b.coords[axis]);
    const mid = items.length >> 1;
    return {
      point: items[mid],
      axis,
      left: this._buildNode(items.slice(0, mid), depth + 1),
      right: this._buildNode(items.slice(mid + 1), depth + 1),
    };
  }

  /**
   * Find all points within `radius` (in 2D XY or 3D XYZ) sorted by ascending distance.
   * Mirrors `BLI_kdtree_3d_range_search`.
   */
  findRange(x, y, z = 0.0, radius, use2D = true) {
    const query = [x, y, use2D ? 0.0 : z];
    const r2 = radius * radius;
    const results = [];

    const search = (node) => {
      if (!node) return;
      const pt = node.point.coords;
      const dx = pt[0] - query[0];
      const dy = pt[1] - query[1];
      const dz = use2D ? 0.0 : pt[2] - query[2];
      const distSq = dx * dx + dy * dy + dz * dz;

      if (distSq <= r2) {
        results.push({
          distance: Math.sqrt(distSq),
          index: node.point.index,
          coords: pt,
          data: node.point.data,
        });
      }

      const axis = node.axis;
      const diff = (axis === 2 && use2D) ? 0.0 : query[axis] - pt[axis];
      const first = diff <= 0 ? node.left : node.right;
      const second = diff <= 0 ? node.right : node.left;

      search(first);
      if (diff * diff <= r2) {
        search(second);
      }
    };

    search(this.root);
    results.sort((a, b) => a.distance - b.distance);
    return results;
  }

  /**
   * Find single nearest neighbor to `(x, y, z)`.
   */
  findNearest(x, y, z = 0.0, use2D = true) {
    const query = [x, y, use2D ? 0.0 : z];
    let best = null;
    let bestDistSq = Infinity;

    const search = (node) => {
      if (!node) return;
      const pt = node.point.coords;
      const dx = pt[0] - query[0];
      const dy = pt[1] - query[1];
      const dz = use2D ? 0.0 : pt[2] - query[2];
      const d2 = dx * dx + dy * dy + dz * dz;
      if (d2 < bestDistSq) {
        bestDistSq = d2;
        best = {
          distance: Math.sqrt(d2),
          index: node.point.index,
          coords: pt,
          data: node.point.data,
        };
      }

      const axis = node.axis;
      const diff = (axis === 2 && use2D) ? 0.0 : query[axis] - pt[axis];
      const first = diff <= 0 ? node.left : node.right;
      const second = diff <= 0 ? node.right : node.left;

      search(first);
      if (diff * diff < bestDistSq) {
        search(second);
      }
    };

    search(this.root);
    return best;
  }
}

/**
 * Generate a `JoinStyle::Round` buffer polygon around a point or convex/concave survey polygon.
 * Used for the 500m and 1,000m archaeological impact buffer rings (`문화유산 영향검토 반경 500m / 1,000m`).
 * @param {Array<[number, number]> | [number, number]} geometry - Point `[x, y]` or polygon ring `[[x, y], ...]`
 * @param {number} radiusM - Buffer distance in meters (e.g. 500 or 1000)
 * @param {number} [segments=64] - Circular smoothness (Round JoinStyle)
 * @returns {Array<[number, number]>} Closed ring of `[x, y]` vertices
 */
export function generateRoundBufferPolygon(geometry, radiusM, segments = 64) {
  if (!Array.isArray(geometry) || geometry.length === 0) return [];

  // Point buffer: exact circle with `segments` vertices
  if (typeof geometry[0] === 'number') {
    const [cx, cy] = geometry;
    const ring = [];
    for (let i = 0; i <= segments; i++) {
      const theta = (2.0 * Math.PI * i) / segments;
      ring.push([cx + Math.cos(theta) * radiusM, cy + Math.sin(theta) * radiusM]);
    }
    return ring;
  }

  // Polygon buffer: compute Minkowski sum of convex hull + circle of radius `radiusM` (JoinStyle::Round)
  const hull = computeConvexHull2D(geometry);
  if (hull.length === 1) {
    return generateRoundBufferPolygon(hull[0], radiusM, segments);
  }

  const ring = [];
  const n = hull.length;
  const arcSteps = Math.max(8, Math.ceil(segments / n));

  for (let i = 0; i < n; i++) {
    const prev = hull[(i - 1 + n) % n];
    const curr = hull[i];
    const next = hull[(i + 1) % n];

    // Outward normals for CCW hull edges
    const e1x = curr[0] - prev[0],
      e1y = curr[1] - prev[1];
    const len1 = Math.hypot(e1x, e1y) || 1.0;
    const n1x = e1y / len1,
      n1y = -e1x / len1;

    const e2x = next[0] - curr[0],
      e2y = next[1] - curr[1];
    const len2 = Math.hypot(e2x, e2y) || 1.0;
    const n2x = e2y / len2,
      n2y = -e2x / len2;

    let a1 = Math.atan2(n1y, n1x);
    let a2 = Math.atan2(n2y, n2x);
    while (a2 < a1) a2 += 2.0 * Math.PI;

    for (let s = 0; s <= arcSteps; s++) {
      const a = a1 + ((a2 - a1) * s) / arcSteps;
      ring.push([curr[0] + Math.cos(a) * radiusM, curr[1] + Math.sin(a) * radiusM]);
    }
  }

  if (ring.length > 0) {
    ring.push([ring[0][0], ring[0][1]]);
  }
  return ring;
}

/**
 * Monotone chain 2D convex hull in CCW order.
 */
export function computeConvexHull2D(points) {
  const pts = points.slice().sort((a, b) => (a[0] === b[0] ? a[1] - b[1] : a[0] - b[0]));
  if (pts.length <= 2) return pts;

  const cross = (o, a, b) => (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0]);

  const lower = [];
  for (const p of pts) {
    while (lower.length >= 2 && cross(lower[lower.length - 2], lower[lower.length - 1], p) <= 0) {
      lower.pop();
    }
    lower.push(p);
  }

  const upper = [];
  for (let i = pts.length - 1; i >= 0; i--) {
    const p = pts[i];
    while (upper.length >= 2 && cross(upper[upper.length - 2], upper[upper.length - 1], p) <= 0) {
      upper.pop();
    }
    upper.push(p);
  }

  lower.pop();
  upper.pop();
  return lower.concat(upper);
}
