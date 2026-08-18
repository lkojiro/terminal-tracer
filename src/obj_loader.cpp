#include "obj_loader.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {

// Resolves a raw (possibly negative/relative) OBJ index to a 0-based
// vector index. Positive indices are 1-based per the spec; negative
// indices count back from the vertex most recently parsed at this point
// in the file.
int resolveIndex(int rawIndex, size_t currentVertexCount) {
    if (rawIndex > 0) return rawIndex - 1;
    return static_cast<int>(currentVertexCount) + rawIndex;
}

// A face vertex reference is "v", "v/vt", "v/vt/vn", or "v//vn" -- only
// the leading v matters here.
std::optional<int> parseFaceVertexIndex(const std::string& token) {
    std::string vertexPart = token.substr(0, token.find('/'));
    if (vertexPart.empty()) return std::nullopt;

    try {
        size_t consumed = 0;
        int value = std::stoi(vertexPart, &consumed);
        if (consumed != vertexPart.size()) return std::nullopt; // trailing garbage
        return value;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

} // namespace

std::optional<Mesh> parseObj(std::istream& in) {
    Mesh mesh;
    std::string line;
    int lineNo = 0;

    while (std::getline(in, line)) {
        lineNo++;
        std::istringstream ls(line);
        std::string tag;
        ls >> tag;

        if (tag == "v") {
            float x, y, z;
            if (!(ls >> x >> y >> z)) {
                std::cerr << "parseObj: line " << lineNo << ": malformed vertex ('" << line << "')\n";
                return std::nullopt;
            }
            mesh.vertices.push_back(Vec3(x, y, z));

        } else if (tag == "f") {
            std::vector<int> faceIndices;
            std::string token;
            while (ls >> token) {
                std::optional<int> raw = parseFaceVertexIndex(token);
                if (!raw) {
                    std::cerr << "parseObj: line " << lineNo << ": malformed face vertex '" << token << "'\n";
                    return std::nullopt;
                }
                int idx = resolveIndex(*raw, mesh.vertices.size());
                if (idx < 0 || idx >= static_cast<int>(mesh.vertices.size())) {
                    std::cerr << "parseObj: line " << lineNo << ": face vertex index " << *raw << " out of range\n";
                    return std::nullopt;
                }
                faceIndices.push_back(idx);
            }
            if (faceIndices.size() < 3) {
                std::cerr << "parseObj: line " << lineNo << ": face has fewer than 3 vertices\n";
                return std::nullopt;
            }
            // Fan triangulation from the first vertex -- preserves the
            // face's winding order, but assumes it's convex and roughly
            // planar (see parseObj's doc comment).
            for (size_t i = 1; i + 1 < faceIndices.size(); i++) {
                mesh.triangles.push_back({faceIndices[0], faceIndices[i], faceIndices[i + 1]});
            }
        }
        // Everything else -- comments, vt/vn/vp, g/o/s, mtllib/usemtl,
        // blank lines -- isn't needed for basic geometry, so it's ignored.
    }

    return mesh;
}

std::optional<Mesh> loadObj(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        std::cerr << "loadObj: couldn't open '" << path << "'\n";
        return std::nullopt;
    }
    return parseObj(file);
}

void fitToSize(Mesh& mesh, float targetSize) {
    if (mesh.vertices.empty()) return;

    Vec3 lo = mesh.vertices[0], hi = mesh.vertices[0];
    for (const Vec3& v : mesh.vertices) {
        lo = Vec3(std::min(lo.x, v.x), std::min(lo.y, v.y), std::min(lo.z, v.z));
        hi = Vec3(std::max(hi.x, v.x), std::max(hi.y, v.y), std::max(hi.z, v.z));
    }

    Vec3 center = (lo + hi) * 0.5f;
    Vec3 extent = hi - lo;
    float largestAxis = std::max({extent.x, extent.y, extent.z});
    if (largestAxis < 1e-8f) return; // degenerate (single-point) mesh

    float scale = targetSize / largestAxis;
    for (Vec3& v : mesh.vertices) {
        v = (v - center) * scale;
    }
}
