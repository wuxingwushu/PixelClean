#define MomentInertiaSamplingSize 5 // 转动惯量采样数量

#define PixelBlockEdgeSize 16     // 像素块边长， （必须是 2 的幂，方便计算）
#define PixelBlockPowerMaxNum 4   // 16 是 2 的 4次幂
#define PixelBlockPowerMinNum 0xF // 4 位bit

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
