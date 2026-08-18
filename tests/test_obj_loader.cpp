#include "test_framework.hpp"
#include "obj_loader.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>

// --- parseObj: basic geometry -------------------------------------------

TEST(parseObj_parses_a_single_triangle) {
    std::istringstream in(
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "f 1 2 3\n"
    );

    auto mesh = parseObj(in);
    CHECK(mesh.has_value());
    CHECK(mesh->vertices.size() == 3);
    CHECK(mesh->triangles.size() == 1);
    CHECK(mesh->triangles[0][0] == 0);
    CHECK(mesh->triangles[0][1] == 1);
    CHECK(mesh->triangles[0][2] == 2);
    CHECK_NEAR(mesh->vertices[1].x, 1.0f, 1e-6f);
    CHECK_NEAR(mesh->vertices[2].y, 1.0f, 1e-6f);
}

TEST(parseObj_fan_triangulates_ngon_faces_preserving_winding) {
    // A quad: fan triangulation from vertex 0 should produce (0,1,2) and
    // (0,2,3), in that order, not just "2 triangles somehow".
    std::istringstream in(
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 1 1 0\n"
        "v 0 1 0\n"
        "f 1 2 3 4\n"
    );

    auto mesh = parseObj(in);
    CHECK(mesh.has_value());
    CHECK(mesh->triangles.size() == 2);
    CHECK(mesh->triangles[0][0] == 0 && mesh->triangles[0][1] == 1 && mesh->triangles[0][2] == 2);
    CHECK(mesh->triangles[1][0] == 0 && mesh->triangles[1][1] == 2 && mesh->triangles[1][2] == 3);
}

TEST(parseObj_ignores_texture_and_normal_indices) {
    // v/vt/vn and v//vn forms should still resolve to the right vertex.
    std::istringstream in(
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "f 1/1/1 2/2/2 3//3\n"
    );

    auto mesh = parseObj(in);
    CHECK(mesh.has_value());
    CHECK(mesh->triangles.size() == 1);
    CHECK(mesh->triangles[0][0] == 0);
    CHECK(mesh->triangles[0][1] == 1);
    CHECK(mesh->triangles[0][2] == 2);
}

TEST(parseObj_resolves_negative_relative_indices) {
    // -1/-2/-3 should refer back to the 3 vertices just defined, same as
    // "1 2 3" would here.
    std::istringstream in(
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "f -3 -2 -1\n"
    );

    auto mesh = parseObj(in);
    CHECK(mesh.has_value());
    CHECK(mesh->triangles.size() == 1);
    CHECK(mesh->triangles[0][0] == 0);
    CHECK(mesh->triangles[0][1] == 1);
    CHECK(mesh->triangles[0][2] == 2);
}

TEST(parseObj_ignores_comments_and_unhandled_tags) {
    std::istringstream in(
        "# a comment\n"
        "\n"
        "vt 0 0\n"
        "vn 0 0 1\n"
        "o SomeObject\n"
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "g group1\n"
        "usemtl material1\n"
        "f 1 2 3\n"
    );

    auto mesh = parseObj(in);
    CHECK(mesh.has_value());
    CHECK(mesh->vertices.size() == 3);
    CHECK(mesh->triangles.size() == 1);
}

// --- parseObj: error handling --------------------------------------------

TEST(parseObj_rejects_malformed_vertex_line) {
    std::istringstream in("v 1 2\n"); // missing z
    CHECK(!parseObj(in).has_value());
}

TEST(parseObj_rejects_malformed_face_token) {
    std::istringstream in(
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "f 1 2 abc\n"
    );
    CHECK(!parseObj(in).has_value());
}

TEST(parseObj_rejects_out_of_range_face_index) {
    std::istringstream in(
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "f 1 2 4\n" // only 3 vertices defined
    );
    CHECK(!parseObj(in).has_value());
}

TEST(parseObj_rejects_face_with_fewer_than_3_vertices) {
    std::istringstream in(
        "v 0 0 0\nv 1 0 0\n"
        "f 1 2\n"
    );
    CHECK(!parseObj(in).has_value());
}

// --- loadObj: file I/O -----------------------------------------------------

TEST(loadObj_returns_nullopt_for_missing_file) {
    CHECK(!loadObj("/nonexistent/path/does_not_exist.obj").has_value());
}

TEST(loadObj_reads_and_parses_a_real_file) {
    const char* path = "test_obj_loader_tmp.obj";
    {
        std::ofstream out(path);
        out << "v 0 0 0\nv 2 0 0\nv 0 2 0\nf 1 2 3\n";
    }

    auto mesh = loadObj(path);
    std::remove(path);

    CHECK(mesh.has_value());
    CHECK(mesh->vertices.size() == 3);
    CHECK(mesh->triangles.size() == 1);
}

// --- fitToSize -------------------------------------------------------------

TEST(fitToSize_centers_and_rescales_to_target_extent) {
    // Bounding box [0,4] x [0,2] x [0,2] -> center (2,1,1), longest axis 4.
    Mesh mesh;
    mesh.vertices = {
        Vec3(0, 0, 0), Vec3(4, 0, 0), Vec3(0, 2, 0), Vec3(0, 0, 2),
    };

    fitToSize(mesh, 2.0f); // targetSize 2 -> scale = 2/4 = 0.5

    // (0,0,0) is the bbox's min corner, farthest from center (2,1,1) along
    // the axis that got scaled to fill exactly targetSize/2 in each
    // direction: (0-2)*0.5 = -1, etc.
    CHECK_NEAR(mesh.vertices[0].x, -1.0f, 1e-5f);
    CHECK_NEAR(mesh.vertices[0].y, -0.5f, 1e-5f);
    CHECK_NEAR(mesh.vertices[0].z, -0.5f, 1e-5f);

    CHECK_NEAR(mesh.vertices[1].x, 1.0f, 1e-5f); // (4,0,0) -> opposite corner along x
}

TEST(fitToSize_is_a_noop_on_empty_mesh) {
    Mesh mesh;
    fitToSize(mesh); // must not crash on an empty vertex list
    CHECK(mesh.vertices.empty());
}
