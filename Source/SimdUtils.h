#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>

#if defined(__AVX__) || defined(__AVX2__)
#include <immintrin.h>
#endif

namespace dsp {
namespace simd {

#if defined(__AVX__) || defined(__AVX2__)

using Vec4 = __m128;
using Vec8 = __m256;

inline Vec4 load4(const float* ptr) { return _mm_loadu_ps(ptr); }
inline void store4(float* ptr, Vec4 v) { _mm_storeu_ps(ptr, v); }

inline Vec8 load8(const float* ptr) { return _mm256_loadu_ps(ptr); }
inline void store8(float* ptr, Vec8 v) { _mm256_storeu_ps(ptr, v); }

inline Vec4 add4(Vec4 a, Vec4 b) { return _mm_add_ps(a, b); }
inline Vec4 mul4(Vec4 a, Vec4 b) { return _mm_mul_ps(a, b); }
inline Vec4 sub4(Vec4 a, Vec4 b) { return _mm_sub_ps(a, b); }

inline Vec8 add8(Vec8 a, Vec8 b) { return _mm256_add_ps(a, b); }
inline Vec8 mul8(Vec8 a, Vec8 b) { return _mm256_mul_ps(a, b); }

inline Vec4 set1(float x) { return _mm_set1_ps(x); }
inline Vec8 set1_8(float x) { return _mm256_set1_ps(x); }

inline float horizontalSum(Vec4 v)
{
    v = _mm_add_ps(v, _mm_movehl_ps(v, v));
    v = _mm_add_ss(v, _mm_movehdup_ps(v));
    return _mm_cvtss_f32(v);
}

#elif defined(__SSE__)

using Vec4 = __m128;

inline Vec4 load4(const float* ptr) { return _mm_loadu_ps(ptr); }
inline void store4(float* ptr, Vec4 v) { _mm_storeu_ps(ptr, v); }

inline Vec4 add4(Vec4 a, Vec4 b) { return _mm_add_ps(a, b); }
inline Vec4 mul4(Vec4 a, Vec4 b) { return _mm_mul_ps(a, b); }
inline Vec4 sub4(Vec4 a, Vec4 b) { return _mm_sub_ps(a, b); }

inline Vec4 set1(float x) { return _mm_set1_ps(x); }

inline float horizontalSum(Vec4 v)
{
    v = _mm_add_ps(v, _mm_movehl_ps(v, v));
    v = _mm_add_ss(v, _mm_movehdup_ps(v));
    return _mm_cvtss_f32(v);
}

#else

// Scalar fallback
struct Vec4 { float v[4]; };

inline void store4(float* ptr, float a, float b, float c, float d)
{
    ptr[0] = a; ptr[1] = b; ptr[2] = c; ptr[3] = d;
}

inline Vec4 load4(const float* ptr)
{
    return Vec4{{ptr[0], ptr[1], ptr[2], ptr[3]}};
}

inline Vec4 add4(float a0, float a1, float a2, float a3,
                float b0, float b1, float b2, float b3)
{
    return Vec4{{a0+b0, a1+b1, a2+b2, a3+b3}};
}

inline Vec4 mul4(float a0, float a1, float a2, float a3,
                 float b0, float b1, float b2, float b3)
{
    return Vec4{{a0*b0, a1*b1, a2*b2, a3*b3}};
}

inline float horizontalSum(const Vec4& v)
{
    return v.v[0] + v.v[1] + v.v[2] + v.v[3];
}

#endif

// Process array with potential SIMD acceleration
inline void applyGain(float* buffer, int count, float gain)
{
    #if defined(__AVX__) || defined(__AVX2__)
    if (count >= 8)
    {
        Vec8 gainVec = set1_8(gain);
        int i = 0;
        for (; i + 8 <= count; i += 8)
        {
            Vec8 v = load8(buffer + i);
            v = mul8(v, gainVec);
            store8(buffer + i, v);
        }
        // Handle remainder
        for (; i < count; ++i)
            buffer[i] *= gain;
    }
    #elif defined(__SSE__)
    if (count >= 4)
    {
        Vec4 gainVec = set1(gain);
        int i = 0;
        for (; i + 4 <= count; i += 4)
        {
            Vec4 v = load4(buffer + i);
            v = mul4(v, gainVec);
            store4(buffer + i, v);
        }
        for (; i < count; ++i)
            buffer[i] *= gain;
    }
    #else
    for (int i = 0; i < count; ++i)
        buffer[i] *= gain;
    #endif
}

// Mix two buffers: out = a + b * gain
inline void mixWithGain(float* out, const float* a, const float* b, int count, float gain)
{
    #if defined(__AVX__) || defined(__AVX2__)
    if (count >= 8)
    {
        Vec8 gainVec = set1_8(gain);
        int i = 0;
        for (; i + 8 <= count; i += 8)
        {
            Vec8 va = load8(a + i);
            Vec8 vb = load8(b + i);
            Vec8 vr = add8(va, mul8(vb, gainVec));
            store8(out + i, vr);
        }
        for (; i < count; ++i)
            out[i] = a[i] + b[i] * gain;
    }
    #elif defined(__SSE__)
    if (count >= 4)
    {
        Vec4 gainVec = set1(gain);
        int i = 0;
        for (; i + 4 <= count; i += 4)
        {
            Vec4 va = load4(a + i);
            Vec4 vb = load4(b + i);
            // Need manual mul and add for SSE
            for (int j = 0; j < 4; ++j)
                out[i + j] = a[i + j] + b[i + j] * gain;
        }
        for (; i < count; ++i)
            out[i] = a[i] + b[i] * gain;
    }
    #else
    for (int i = 0; i < count; ++i)
        out[i] = a[i] + b[i] * gain;
    #endif
}

// Check for NaN or Inf
inline bool hasNanOrInf(const float* buffer, int count)
{
    for (int i = 0; i < count; ++i)
    {
        float v = buffer[i];
        if (std::isnan(v) || std::isinf(v))
            return true;
    }
    return false;
}

// Clamp values to range
inline void clamp(float* buffer, int count, float minVal, float maxVal)
{
    for (int i = 0; i < count; ++i)
        buffer[i] = std::max(minVal, std::min(maxVal, buffer[i]));
}

}} // namespace dsp::simd