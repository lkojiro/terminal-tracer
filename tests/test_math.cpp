#include "test_framework.hpp"
#include "vec3.hpp"
#include "mat4.hpp"

// --- Vec3 tests -----------------------------------------------------

TEST(vec3_add) {
    Vec3 a(1, 2, 3), b(4, 5, 6);
    Vec3 c = a + b;
    CHECK_NEAR(c.x, 5.0f, 1e-6f);
    CHECK_NEAR(c.y, 7.0f, 1e-6f);
    CHECK_NEAR(c.z, 9.0f, 1e-6f);
}

TEST(vec3_dot_orthogonal_is_zero) {
    Vec3 a(1, 0, 0), b(0, 1, 0);
    CHECK_NEAR(a.dot(b), 0.0f, 1e-6f);
}

TEST(vec3_cross_x_cross_y_is_z) {
    Vec3 x(1, 0, 0), y(0, 1, 0);
    Vec3 z = x.cross(y);
    CHECK_NEAR(z.x, 0.0f, 1e-6f);
    CHECK_NEAR(z.y, 0.0f, 1e-6f);
    CHECK_NEAR(z.z, 1.0f, 1e-6f);
}

TEST(vec3_normalized_has_unit_length) {
    Vec3 v(3, 4, 0); // 3-4-5 triangle
    Vec3 n = v.normalized();
    CHECK_NEAR(n.length(), 1.0f, 1e-6f);
    CHECK_NEAR(n.x, 0.6f, 1e-6f);
    CHECK_NEAR(n.y, 0.8f, 1e-6f);
}

// --- Mat4 tests -------------------------------------------------------

TEST(mat4_identity_leaves_vector_unchanged) {
    Mat4 id = Mat4::identity();
    Vec4 v(1, 2, 3, 1);
    Vec4 r = id * v;
    CHECK_NEAR(r.x, 1.0f, 1e-6f);
    CHECK_NEAR(r.y, 2.0f, 1e-6f);
    CHECK_NEAR(r.z, 3.0f, 1e-6f);
    CHECK_NEAR(r.w, 1.0f, 1e-6f);
}

TEST(mat4_translate_moves_point) {
    Mat4 t = Mat4::translate(5, 0, 0);
    Vec4 v(1, 2, 3, 1);
    Vec4 r = t * v;
    CHECK_NEAR(r.x, 6.0f, 1e-6f);
    CHECK_NEAR(r.y, 2.0f, 1e-6f);
    CHECK_NEAR(r.z, 3.0f, 1e-6f);
}

TEST(mat4_scale_scales_point) {
    Mat4 s = Mat4::scale(2, 3, 4);
    Vec4 v(1, 1, 1, 1);
    Vec4 r = s * v;
    CHECK_NEAR(r.x, 2.0f, 1e-6f);
    CHECK_NEAR(r.y, 3.0f, 1e-6f);
    CHECK_NEAR(r.z, 4.0f, 1e-6f);
}

TEST(mat4_multiply_composes_transforms) {
    // Translating then scaling should NOT be the same as scaling then
    // translating -- a good test of both your understanding of
    // matrix composition order and the multiply implementation.
    Mat4 t = Mat4::translate(1, 0, 0);
    Mat4 s = Mat4::scale(2, 2, 2);
    Vec4 v(1, 0, 0, 1);

    Vec4 scaleThenTranslate = (t * s) * v; // scale first, then translate
    CHECK_NEAR(scaleThenTranslate.x, 3.0f, 1e-6f); // (1*2) + 1 = 3

    Vec4 translateThenScale = (s * t) * v; // translate first, then scale
    CHECK_NEAR(translateThenScale.x, 4.0f, 1e-6f); // (1+1) * 2 = 4
}

// --- TODO(you): add tests here as you implement rotateY / perspective /
// lookAt. A few good properties to test once they exist:
//
//   - rotateY(0) should be the identity (a vector should pass through
//     unchanged)
TEST(mat4_rotate_y_identity) {
    Mat4 yi = Mat4::rotateY(0);
    Vec4 u(1.0f, 1.0f, 1.0f, 1.0f);
    
    Vec4 u_prime = yi * u;
    CHECK_NEAR(u_prime.x, u.x, 1e-6f);
    CHECK_NEAR(u_prime.y, u.y, 1e-6f);
    CHECK_NEAR(u_prime.z, u.z, 1e-6f);
    CHECK_NEAR(u_prime.w, u.w, 1e-6f);
}

//   - rotateY(pi/2) applied to the X axis should land on (roughly)
//     the -Z or +Z axis depending on your rotation convention --
//     good way to catch a sign error early
TEST(mat4_rotate_y_deg_90) {
    Mat4 deg_90_rot = Mat4::rotateY(std::numbers::pi/2);
    Vec4 u(1.0f, 0.0f, 0.0f, 1.0f);
    Vec4 v(0.0f, 0.0f, -1.0f, 1.0f);
    
    Vec4 u_prime = deg_90_rot * u;
    CHECK_NEAR(u_prime.x, v.x, 1e-6f);
    CHECK_NEAR(u_prime.y, v.y, 1e-6f);
    CHECK_NEAR(u_prime.z, v.z, 1e-6f);
    CHECK_NEAR(u_prime.w, v.w, 1e-6f);
}

TEST(mat4_rotate_axis_identity_at_zero_radians) {
    // Any axis, zero angle -- should leave the vector unchanged, same
    // property as rotateY(0).
    Mat4 r = Mat4::rotateAxis(Vec3(1, 1, 1), 0.0f);
    Vec4 u(1.0f, 2.0f, 3.0f, 1.0f);

    Vec4 u_prime = r * u;
    CHECK_NEAR(u_prime.x, u.x, 1e-6f);
    CHECK_NEAR(u_prime.y, u.y, 1e-6f);
    CHECK_NEAR(u_prime.z, u.z, 1e-6f);
    CHECK_NEAR(u_prime.w, u.w, 1e-6f);
}

TEST(mat4_rotate_axis_y_matches_rotate_y) {
    // rotateAxis around (0,1,0) should reproduce rotateY exactly -- same
    // sign convention, same result.
    float angle = std::numbers::pi / 3.0f;
    Mat4 viaAxis = Mat4::rotateAxis(Vec3(0, 1, 0), angle);
    Mat4 viaY = Mat4::rotateY(angle);
    Vec4 u(1.0f, 2.0f, 3.0f, 1.0f);

    Vec4 a = viaAxis * u;
    Vec4 b = viaY * u;
    CHECK_NEAR(a.x, b.x, 1e-6f);
    CHECK_NEAR(a.y, b.y, 1e-6f);
    CHECK_NEAR(a.z, b.z, 1e-6f);
}

TEST(mat4_rotate_axis_z_deg_90) {
    // Rotating 90deg around +Z should take the X axis to the Y axis
    // (right-handed rule) -- pins down the sign convention on an axis
    // rotateY() doesn't cover.
    Mat4 r = Mat4::rotateAxis(Vec3(0, 0, 1), std::numbers::pi / 2.0f);
    Vec4 u(1.0f, 0.0f, 0.0f, 1.0f);

    Vec4 u_prime = r * u;
    CHECK_NEAR(u_prime.x, 0.0f, 1e-6f);
    CHECK_NEAR(u_prime.y, 1.0f, 1e-6f);
    CHECK_NEAR(u_prime.z, 0.0f, 1e-6f);
}

TEST(mat4_rotate_axis_normalizes_input) {
    // A non-unit-length axis should give the same rotation as its
    // normalized form -- catches a missing/forgotten normalize() call.
    float angle = std::numbers::pi / 4.0f;
    Mat4 fromUnit = Mat4::rotateAxis(Vec3(0, 1, 0), angle);
    Mat4 fromScaled = Mat4::rotateAxis(Vec3(0, 5, 0), angle);
    Vec4 u(1.0f, 2.0f, 3.0f, 1.0f);

    Vec4 a = fromUnit * u;
    Vec4 b = fromScaled * u;
    CHECK_NEAR(a.x, b.x, 1e-5f);
    CHECK_NEAR(a.y, b.y, 1e-5f);
    CHECK_NEAR(a.z, b.z, 1e-5f);
}

//   - perspective(): a point on the near plane's center should map
//     to roughly z = -1 (or 0, depending on your convention) in NDC
TEST(mat4_perspective_near_plane_maps_to_ndc_z_neg1) {
    // On-axis point sitting exactly on the near plane. w after the
    // matrix multiply should equal nearZ (that's what w=-z means),
    // and dividing z_clip by that w should land exactly on NDC z=-1.
    Mat4 p = Mat4::perspective(std::numbers::pi / 2.0f, 1.0f, 1.0f, 10.0f);
    Vec4 v(0.0f, 0.0f, -1.0f, 1.0f); // z = -nearZ

    Vec4 clip = p * v;
    CHECK_NEAR(clip.w, 1.0f, 1e-6f); // w == nearZ
    CHECK_NEAR(clip.z / clip.w, -1.0f, 1e-5f);
}

TEST(mat4_perspective_far_plane_maps_to_ndc_z_pos1) {
    // Same idea at the far plane: w should equal farZ, and z_clip/w
    // should land on NDC z=+1. Catches a sign or algebra error in the
    // A/B coefficients that the near-plane test alone wouldn't catch.
    Mat4 p = Mat4::perspective(std::numbers::pi / 2.0f, 1.0f, 1.0f, 10.0f);
    Vec4 v(0.0f, 0.0f, -10.0f, 1.0f); // z = -farZ

    Vec4 clip = p * v;
    CHECK_NEAR(clip.w, 10.0f, 1e-5f); // w == farZ
    CHECK_NEAR(clip.z / clip.w, 1.0f, 1e-5f);
}

TEST(mat4_perspective_frustum_edge_maps_to_ndc_y_pos1) {
    // With fovY = 90deg, tan(fovY/2) == 1, so at depth z=-5 the top of
    // the frustum sits at y = 5. That point should land exactly on
    // NDC y=+1 after the divide -- checks the d = 1/tan(fovY/2) term.
    Mat4 p = Mat4::perspective(std::numbers::pi / 2.0f, 1.0f, 1.0f, 10.0f);
    Vec4 v(0.0f, 5.0f, -5.0f, 1.0f);

    Vec4 clip = p * v;
    CHECK_NEAR(clip.y / clip.w, 1.0f, 1e-5f);
}

TEST(mat4_perspective_aspect_scales_x_not_y) {
    // aspect = 2 with fovY = 90deg means tan(fovX/2) = aspect * tan(fovY/2) = 2,
    // so at depth z=-5 the frustum's right edge sits at x=10. That
    // point should land on NDC x=+1. If x were scaled by aspect
    // instead of divided by it, this would come out to x=4 instead --
    // catches exactly that mistake.
    Mat4 p = Mat4::perspective(std::numbers::pi / 2.0f, 2.0f, 1.0f, 10.0f);
    Vec4 v(10.0f, 0.0f, -5.0f, 1.0f);

    Vec4 clip = p * v;
    CHECK_NEAR(clip.x / clip.w, 1.0f, 1e-5f);
}

//   - lookAt(): looking from (0,0,5) at the origin, a point at the
//     origin should transform to have z roughly equal to the
//     eye-to-target distance (negated, depending on convention)
TEST(lookAt_eye_maps_to_origin) {
    // Whatever the camera is looking at, the eye position itself must
    // always land exactly at (0,0,0) in view space -- the camera is
    // always at the center of its own view.
    Vec3 eye(0, 0, 5), target(0, 0, 0), up(0, 1, 0);
    Mat4 view = Mat4::lookAt(eye, target, up);

    Vec4 eyeInViewSpace = view * Vec4(eye, 1.0f);
    CHECK_NEAR(eyeInViewSpace.x, 0.0f, 1e-5f);
    CHECK_NEAR(eyeInViewSpace.y, 0.0f, 1e-5f);
    CHECK_NEAR(eyeInViewSpace.z, 0.0f, 1e-5f);
}

TEST(lookAt_target_lands_on_negative_z) {
    // With eye at (0,0,5) looking at the origin, the target should end
    // up straight ahead of the camera -- x and y unchanged (0), and z
    // equal to *negative* the eye-to-target distance. Negative because
    // things in front of the camera sit at negative view-space z with
    // this convention (zaxis pointing back toward the eye).
    Vec3 eye(0, 0, 5), target(0, 0, 0), up(0, 1, 0);
    Mat4 view = Mat4::lookAt(eye, target, up);

    Vec4 targetInViewSpace = view * Vec4(target, 1.0f);
    CHECK_NEAR(targetInViewSpace.x, 0.0f, 1e-5f);
    CHECK_NEAR(targetInViewSpace.y, 0.0f, 1e-5f);
    CHECK_NEAR(targetInViewSpace.z, -5.0f, 1e-5f);
}

TEST(lookAt_up_direction_stays_positive_y) {
    // A point directly "above" the eye (offset along world up) should
    // land with a positive y in view space and x/z unchanged -- this
    // catches a flipped or mismatched up/right basis vector, which
    // wouldn't necessarily show up in the previous two tests.
    Vec3 eye(0, 0, 5), target(0, 0, 0), up(0, 1, 0);
    Mat4 view = Mat4::lookAt(eye, target, up);

    Vec3 pointAboveEye = eye + up;
    Vec4 result = view * Vec4(pointAboveEye, 1.0f);
    CHECK_NEAR(result.x, 0.0f, 1e-5f);
    CHECK_NEAR(result.y, 1.0f, 1e-5f);
    CHECK_NEAR(result.z, 0.0f, 1e-5f);
}

TEST(lookAt_right_direction_stays_positive_x) {
    // A point directly to the "right" of the eye (offset along world
    // +x) should land with a positive x in view space and y/z
    // unchanged -- this pins down the handedness/sign of the right
    // basis vector, which the up-direction test above doesn't cover.
    Vec3 eye(0, 0, 5), target(0, 0, 0), up(0, 1, 0);
    Mat4 view = Mat4::lookAt(eye, target, up);

    Vec3 pointRightOfEye = eye + Vec3(1, 0, 0);
    Vec4 result = view * Vec4(pointRightOfEye, 1.0f);
    CHECK_NEAR(result.x, 1.0f, 1e-5f);
    CHECK_NEAR(result.y, 0.0f, 1e-5f);
    CHECK_NEAR(result.z, 0.0f, 1e-5f);
}
