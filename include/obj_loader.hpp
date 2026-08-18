#pragma once
#include <array>
#include <istream>
#include <optional>
#include <string>
#include <vector>

#include "vec3.hpp"

// A loaded triangle mesh: vertex positions plus the triangle index list
// render() expects (see render.hpp's `triangles` param). No normals,
// UVs, or materials -- just enough geometry to get something other than
// the hardcoded cube on screen.
struct Mesh {
    std::vector<Vec3> vertices;
    std::vector<std::array<int, 3>> triangles;
};

// Parses a basic Wavefront .obj mesh from an already-open stream:
//   v x y z      -- a vertex position (a trailing w, if present, is
//                    ignored -- homogeneous vertex weights aren't
//                    something this renderer uses).
//   f i j k ...  -- a face, as whitespace-separated vertex references.
//                    Each reference may be a bare index ("5") or carry
//                    texture/normal indices ("5/2", "5/2/1", "5//1") --
//                    only the vertex index is used. Faces with more than
//                    3 vertices are fan-triangulated from their first
//                    vertex, which preserves the file's winding order but
//                    only produces correct results for convex,
//                    (approximately) planar polygons -- true for most
//                    exported quads/n-gons, but not guaranteed in
//                    general. Negative indices (relative to the current
//                    end of the vertex list, per the OBJ spec) are
//                    supported.
// Everything else -- comments, vt/vn/vp, g/o/s, mtllib/usemtl, blank
// lines -- is silently ignored.
//
// Returns std::nullopt (after writing why to stderr, with a line number)
// on any malformed vertex/face line or out-of-range index.
std::optional<Mesh> parseObj(std::istream& in);

// Opens `path` and parses it with parseObj(). Returns std::nullopt (after
// writing why to stderr) if the file can't be opened.
std::optional<Mesh> loadObj(const std::string& path);

// Recenters `mesh` on its bounding-box center and uniformly rescales it
// so its longest axis spans `targetSize`. OBJ files come in whatever
// scale/position their source scene used, which usually isn't anywhere
// near this renderer's camera/near/far setup -- call this after loading
// if you want a model framed similarly to the built-in cube (whose own
// extents are exactly the targetSize=2.0f default) rather than tuning
// the camera per file. A no-op on an empty or single-point mesh.
void fitToSize(Mesh& mesh, float targetSize = 2.0f);
