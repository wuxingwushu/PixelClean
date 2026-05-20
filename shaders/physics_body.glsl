// physics_body.glsl
// 
// This file documents the shared data structures used by all physics compute shaders.
// NOTE: GLSL #include is not natively supported by glslangValidator without
// extensions, so each .comp file has this content inlined.
// This file is kept for reference and documentation.
//
// BODY_STRIDE = 8 uints per body
// Body layout (binding=0, all kernels share):
//   offset 0: speedX          (float, read-write, atomic)
//   offset 1: speedY          (float, read-write, atomic)
//   offset 2: angleSpeed      (float, read-write, atomic)
//   offset 3: invMass         (float, read-only)
//   offset 4: invMomentInertia(float, read-only)
//   offset 5: mass            (float, read-only, for FLOAT_MAX check)
//   offset 6: friction        (float, read-only)
//   offset 7: padding         (float, unused)
//
// Arbiter stride = 8 + 20 * 12 = 248 uints (binding=1, arbiter kernel only)
// Joint stride = 20 floats (binding=1, joint kernel only)
// Junction stride = 20 floats (binding=1, junction kernel only)
// Count buffer = 1 uint (binding=2, elementCount for each kernel)

#version 450

#define ARBITER_TYPE_AA 0
#define ARBITER_TYPE_AD 1
#define ARBITER_TYPE_A  2
#define ARBITER_TYPE_D  3

#define JUNCTION_TYPE_AA 0
#define JUNCTION_TYPE_A  1
#define JUNCTION_TYPE_P  2
#define JUNCTION_TYPE_PP 3

#define MAX_CONTACTS 20
#define BODY_STRIDE 8
#define ARBITER_META_U32 8
#define CONTACT_U32 12
#define INVALID_INDEX 0xFFFFFFFFu