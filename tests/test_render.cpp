#include "test_framework.hpp"
#include "render.hpp"
#include "vec3.hpp"
#include <numbers>

// --- faceNormal tests ---------------------------------------------------

TEST(faceNormal_right_hand_rule) {
    // a->b along +x, a->c along +y: cross(ab, ac) should point along +z,
    // the textbook right-hand-rule case.
    Vec3 a(0, 0, 0), b(1, 0, 0), c(0, 1, 0);
    Vec3 n = faceNormal(a, b, c);
    CHECK_NEAR(n.x, 0.0f, 1e-6f);
    CHECK_NEAR(n.y, 0.0f, 1e-6f);
    CHECK_NEAR(n.z, 1.0f, 1e-6f);
}

TEST(faceNormal_winding_order_flips_sign) {
    // Swapping b and c reverses the winding order, which should flip
    // the normal to point the opposite way -- this is exactly the
    // property back-face culling depends on.
    Vec3 a(0, 0, 0), b(1, 0, 0), c(0, 1, 0);
    Vec3 n = faceNormal(a, b, c);
    Vec3 nFlipped = faceNormal(a, c, b);
    CHECK_NEAR(nFlipped.x, -n.x, 1e-6f);
    CHECK_NEAR(nFlipped.y, -n.y, 1e-6f);
    CHECK_NEAR(nFlipped.z, -n.z, 1e-6f);
}

TEST(faceNormal_perpendicular_to_both_edges) {
    // For a non-axis-aligned triangle, the normal should be orthogonal
    // to both edge vectors -- a more general correctness check than the
    // axis-aligned case above, which a swapped-component bug could
    // still pass by accident.
    Vec3 a(1, 2, 3), b(4, 1, -2), c(0, 5, 1);
    Vec3 n = faceNormal(a, b, c);
    Vec3 ab = b - a, ac = c - a;
    CHECK_NEAR(n.dot(ab), 0.0f, 1e-4f);
    CHECK_NEAR(n.dot(ac), 0.0f, 1e-4f);
}

TEST(faceNormal_degenerate_triangle_is_zero) {
    // Three collinear points enclose zero area -- cross product of
    // parallel edges should come out (0,0,0).
    Vec3 a(0, 0, 0), b(1, 1, 1), c(2, 2, 2);
    Vec3 n = faceNormal(a, b, c);
    CHECK_NEAR(n.x, 0.0f, 1e-6f);
    CHECK_NEAR(n.y, 0.0f, 1e-6f);
    CHECK_NEAR(n.z, 0.0f, 1e-6f);
}

TEST(faceNormal_cube_back_face_points_outward) {
    // Grounded in the actual cube data from main.cpp: the back face
    // (z = -1) is wound {0,2,1} so its normal should point outward,
    // i.e. roughly along -z -- per the comment on cubeTriangles.
    Vec3 v0(-1, -1, -1), v1(1, -1, -1), v2(1, 1, -1);
    Vec3 n = faceNormal(v0, v2, v1);
    CHECK(n.z < 0.0f);
    CHECK_NEAR(n.x, 0.0f, 1e-6f);
    CHECK_NEAR(n.y, 0.0f, 1e-6f);
}

TEST(faceNormal_cube_right_face_points_outward) {
    // Same idea for the right face (x = +1), wound {1,2,6} -- normal
    // should point outward along +x. Covers a different axis than the
    // back-face test above.
    Vec3 v1(1, -1, -1), v2(1, 1, -1), v6(1, 1, 1);
    Vec3 n = faceNormal(v1, v2, v6);
    CHECK(n.x > 0.0f);
    CHECK_NEAR(n.y, 0.0f, 1e-6f);
    CHECK_NEAR(n.z, 0.0f, 1e-6f);
}

// --- lambertIntensity tests ----------------------------------------------

TEST(lambertIntensity_full_when_facing_light) {
    // Normal pointing directly at the light -> maximum brightness.
    Vec3 normal(0, 0, 1), lightDir(0, 0, 1);
    CHECK_NEAR(lambertIntensity(normal, lightDir), 1.0f, 1e-6f);
}

TEST(lambertIntensity_zero_when_perpendicular_to_light) {
    // Light grazing the surface edge-on contributes no illumination.
    Vec3 normal(0, 0, 1), lightDir(1, 0, 0);
    CHECK_NEAR(lambertIntensity(normal, lightDir), 0.0f, 1e-6f);
}

TEST(lambertIntensity_clamped_when_facing_away) {
    // Surface facing directly away from the light: raw dot product is
    // -1, but intensity should clamp to 0, not go negative.
    Vec3 normal(0, 0, 1), lightDir(0, 0, -1);
    CHECK_NEAR(lambertIntensity(normal, lightDir), 0.0f, 1e-6f);
}

TEST(lambertIntensity_matches_cosine_of_angle) {
    // At a 60-degree angle between normal and light, intensity should
    // equal cos(60deg) = 0.5 -- checks the general (non-edge-case) path,
    // not just the 0/90/180-degree special cases above.
    Vec3 normal(0, 0, 1);
    float angle = std::numbers::pi / 3.0f; // 60 degrees, in the XZ plane
    Vec3 lightDir(std::sin(angle), 0, std::cos(angle));
    CHECK_NEAR(lambertIntensity(normal, lightDir), 0.5f, 1e-5f);
}

TEST(lambertIntensity_clamped_upper_bound) {
    // A non-unit-length normal aligned with the light gives a raw dot
    // product > 1 -- intensity should still clamp to 1.0, not blow past
    // it (would otherwise wash out shadeChar()'s density ramp).
    Vec3 normal(0, 0, 3), lightDir(0, 0, 1);
    CHECK_NEAR(lambertIntensity(normal, lightDir), 1.0f, 1e-6f);
}

// --- edgeFunction / isInside tests ---------------------------------------
//
// Shared fixture triangle, wound the way isInside expects ("ccw", per its
// comment -- which in screen space (x right, y down) means visually
// counter-clockwise, the opposite of math-CCW in a y-up frame):
//   a = (0,0) -- top-left
//   b = (0,4) -- bottom-left
//   c = (4,0) -- top-right

TEST(edgeFunction_zero_for_point_on_the_line) {
    // A point sitting exactly on segment a-b should read as zero, same
    // as the "on the line" case documented on edgeFunction.
    ScreenVertex a{0, 0, 0.0f}, b{0, 4, 0.0f}, p{0, 2, 0.0f};
    CHECK_NEAR(edgeFunction(a, b, p), 0.0f, 1e-6f);
}

TEST(edgeFunction_sign_flips_across_the_line) {
    // Edge a-b lies along x=0. Mirroring a test point across that line
    // should flip the sign but not the magnitude -- a direct check that
    // the formula is actually measuring signed distance/side, not just
    // returning some incidental positive number.
    ScreenVertex a{0, 0, 0.0f}, b{0, 4, 0.0f};
    ScreenVertex left{-1, 1, 0.0f}, right{1, 1, 0.0f};
    float eLeft = edgeFunction(a, b, left);
    float eRight = edgeFunction(a, b, right);
    CHECK(eLeft < 0.0f);
    CHECK(eRight > 0.0f);
    CHECK_NEAR(eLeft, -eRight, 1e-6f);
}

TEST(edgeFunction_of_third_vertex_is_twice_triangle_area) {
    // Classic identity: edgeFunction(a, b, c) equals twice the signed
    // area of triangle abc. Legs of 4 and 4 -> area 8 -> 16.
    ScreenVertex a{0, 0, 0.0f}, b{0, 4, 0.0f}, c{4, 0, 0.0f};
    CHECK_NEAR(edgeFunction(a, b, c), 16.0f, 1e-6f);
}

TEST(isInside_centroid_is_inside) {
    ScreenVertex a{0, 0, 0.0f}, b{0, 4, 0.0f}, c{4, 0, 0.0f};
    ScreenVertex centroid{1, 1, 0.0f}; // true centroid is (1.33, 1.33); (1,1) is safely interior
    CHECK(isInside(centroid, a, b, c));
}

TEST(isInside_point_outside_is_false) {
    ScreenVertex a{0, 0, 0.0f}, b{0, 4, 0.0f}, c{4, 0, 0.0f};
    ScreenVertex outside{10, 10, 0.0f};
    CHECK(!isInside(outside, a, b, c));
}

TEST(isInside_vertices_follow_top_left_rule) {
    // Boundary case, under the top-left fill rule: a corner counts as
    // inside only if *both* edges meeting there are top/left edges. For
    // this triangle, a->b (dy=4>0, "left") and c->a (dy=0, dx=-4<0, "top")
    // are both top-left, so vertex a -- sitting on both -- is inside.
    // b and c each touch b->c (dy=-4, neither top nor left), so they're
    // excluded, the same as any other point on that edge.
    ScreenVertex a{0, 0, 0.0f}, b{0, 4, 0.0f}, c{4, 0, 0.0f};
    CHECK(isInside(a, a, b, c));
    CHECK(!isInside(b, a, b, c));
    CHECK(!isInside(c, a, b, c));
}

TEST(isInside_edge_midpoint_follows_top_left_rule) {
    // Same top-left boundary behavior, but on edge midpoints rather than
    // vertices. b-c isn't a top/left edge, so its midpoint reads as
    // outside -- this (plus the matching exclusion on whichever triangle
    // is on the far side) is what keeps a shared edge between two
    // adjacent triangles from being double-painted. a-b *is* a left edge,
    // so its midpoint still reads as inside.
    ScreenVertex a{0, 0, 0.0f}, b{0, 4, 0.0f}, c{4, 0, 0.0f};
    ScreenVertex midBC{2, 2, 0.0f}; // midpoint of edge b-c (not top-left)
    ScreenVertex midAB{0, 2, 0.0f}; // midpoint of edge a-b (left edge)
    CHECK(!isInside(midBC, a, b, c));
    CHECK(isInside(midAB, a, b, c));
}

TEST(isInside_requires_matching_winding_order) {
    // isInside is documented as expecting ccw-wound (a, b, c). Passing
    // the same triangle with b and c swapped (cw winding) flips every
    // edgeFunction's sign, so a genuinely interior point should now
    // read as outside. Worth pinning down since fillTriangle will only
    // work if callers respect this contract.
    ScreenVertex a{0, 0, 0.0f}, b{0, 4, 0.0f}, c{4, 0, 0.0f};
    ScreenVertex interior{1, 1, 0.0f};
    CHECK(isInside(interior, a, b, c));
    CHECK(!isInside(interior, a, c, b));
}

TEST(isInside_degenerate_triangle_accepts_nothing) {
    // Three collinear points enclose zero area. a->b and b->c point the
    // same way along the line (dx=1, dy=1: a "left" edge, top-left), but
    // the closing edge c->a points back over both of them (dx=-2, dy=-2:
    // neither top nor left), so it's held to the strict >0 test. Every
    // point on the line -- on or off the a..c span -- makes all three
    // edge functions exactly zero, and that closing edge's zero fails
    // strict >0. So unlike the plain >=0 rule (where the whole line read
    // as a zero-area "inside"), the top-left rule makes a degenerate
    // triangle accept no points at all -- consistent with fillTriangle
    // skipping zero-area triangles outright instead of dividing by their
    // area.
    ScreenVertex a{0, 0, 0.0f}, b{1, 1, 0.0f}, c{2, 2, 0.0f};
    ScreenVertex onLine{5, 5, 0.0f};
    ScreenVertex offLine{5, 4, 0.0f};
    CHECK(!isInside(onLine, a, b, c));
    CHECK(!isInside(offLine, a, b, c));
}

// --- fillTriangle tests ---------------------------------------------------

TEST(fillTriangle_paints_interior_leaves_outside_untouched) {
    Framebuffer fb(10, 10);
    ScreenVertex a{0, 0, 1.0f}, b{0, 8, 1.0f}, c{8, 0, 1.0f};
    fillTriangle(fb, a, b, c, 1.0f);
    fb.resolveMSAA();

    CHECK(fb.chars[2 * fb.width + 2] != ' '); // (2,2) is interior
    CHECK(fb.chars[9 * fb.width + 9] == ' '); // (9,9) is outside the triangle
}

TEST(fillTriangle_nearer_triangle_wins_regardless_of_draw_order) {
    ScreenVertex far0{0, 0, 5.0f}, far1{0, 8, 5.0f}, far2{8, 0, 5.0f};
    ScreenVertex near0{0, 0, 1.0f}, near1{0, 8, 1.0f}, near2{8, 0, 1.0f};
    float farIntensity = 0.2f, nearIntensity = 0.9f;

    // Reference: what the near triangle alone renders as, deep in its
    // interior (fully covered, so no MSAA blending to complicate the
    // comparison) -- what a correct depth test should match regardless
    // of draw order, without this test needing to know shadeChar's
    // internals to predict the exact glyph.
    Framebuffer reference(10, 10);
    fillTriangle(reference, near0, near1, near2, nearIntensity);
    reference.resolveMSAA();
    char nearAlone = reference.chars[2 * reference.width + 2];
    CHECK(nearAlone != ' ');

    // Nearer (smaller depth) drawn second still wins over farther.
    Framebuffer fb1(10, 10);
    fillTriangle(fb1, far0, far1, far2, farIntensity);
    fillTriangle(fb1, near0, near1, near2, nearIntensity);
    fb1.resolveMSAA();
    CHECK(fb1.chars[2 * fb1.width + 2] == nearAlone);

    // Farther drawn second must not overwrite the nearer pixel already there.
    Framebuffer fb2(10, 10);
    fillTriangle(fb2, near0, near1, near2, nearIntensity);
    fillTriangle(fb2, far0, far1, far2, farIntensity);
    fb2.resolveMSAA();
    CHECK(fb2.chars[2 * fb2.width + 2] == nearAlone);
}

TEST(fillTriangle_interpolates_depth_via_inverse_z_per_sample) {
    // a=(0,0,z=1), b=(0,8,z=2), c=(8,0,z=4); area = 64 (see
    // edgeFunction_of_third_vertex_is_twice_triangle_area-style math).
    // fillTriangle's MSAA offsets place sample 0 of pixel (2,2) at
    // (1.75, 1.75) (its {-0.25,-0.25} entry). Barycentric weights there
    // work out to (9/16, 7/32, 7/32) for (a, b, c) -- checked by hand via
    // edgeFunction on each opposite edge. Depth is interpolated in 1/z,
    // then flipped back:
    //   invZ = 9/16*(1/1) + 7/32*(1/2) + 7/32*(1/4) = 93/128
    //   z    = 128/93
    Framebuffer fb(10, 10);
    ScreenVertex a{0, 0, 1.0f}, b{0, 8, 2.0f}, c{8, 0, 4.0f};
    fillTriangle(fb, a, b, c, 1.0f);

    int sampleIdx = (2 * fb.width + 2) * Framebuffer::kMSAASamples + 0;
    CHECK_NEAR(fb.sampleDepth[sampleIdx], 128.0f / 93.0f, 1e-4f);
}

TEST(fillTriangle_antialiases_partial_edge_coverage) {
    // Triangle with a vertical edge exactly at x=5: a->b runs straight
    // down that line, so at pixel (5, 10), the left-hand samples (x=4.75)
    // fall just outside it and the right-hand samples (x=5.25) fall just
    // inside -- exactly 2 of 4 samples covered, checked by hand against
    // fillTriangle's {-0.25, +0.25} sample offsets. (b-c and c-a are both
    // comfortably far from this pixel, so they don't affect its coverage.)
    Framebuffer fb(25, 25);
    ScreenVertex a{5, 0, 1.0f}, b{5, 20, 1.0f}, c{20, 0, 1.0f};
    fillTriangle(fb, a, b, c, 1.0f);
    fb.resolveMSAA();

    // Reference: a pixel deep in the interior, fully covered by all 4
    // samples -- the "no antialiasing needed" glyph.
    char fullyCovered = fb.chars[10 * fb.width + 10];
    CHECK(fullyCovered != ' ');

    char edgeGlyph = fb.chars[10 * fb.width + 5];
    CHECK(edgeGlyph != ' ');           // partially covered, not background
    CHECK(edgeGlyph != fullyCovered);  // partial coverage -> a different (lighter) glyph
}
