//
//  File.metal
//  Loupy
//
//  Created by Rémy on 26/11/2025.
//

#include <metal_stdlib>
using namespace metal;

#include "RMDLMainRenderer_shared.h"

struct GBufferFragOut
{
    float4 gBuffer0 [[color(0)]];
    float4 gBuffer1 [[color(1)]];
};

struct LightingPsOut
{
    float4 backbuffer [[color(0)]];
};

struct LightingVtxOut
{
    float4 position [[position]];
};

vertex LightingVtxOut LightingVs(uint vid [[vertex_id]])
{
    const float2 vertices[] =
    {
        float2(-1, -1),
        float2(-1,  3),
        float2( 3, -1)
    };

    LightingVtxOut out;
    out.position = float4(vertices[vid], 1.0, 1.0);
    return out;
}

// The bi-directional reflectance distribution properties
//  - Defines how light is reflected on an opaque surface
struct BrdfProperties
{
    float3 albedo;
    float3 normal;
    float specIntensity;
    float specPower;
    float ao;
    float shadow;
};

inline float2 OctahedronWrap(float2 n)
{
    float2 signMask = {n.x >= 0? 1.f : -1.f, n.y >= 0? 1.f : -1.f};
    return (1.f - abs(n.yx)) * signMask;
}

inline float2 EncodeNormal(float3 n)
{
    float3 ret = n / (abs(n.x) + abs(n.y) + abs(n.z));
    ret.xy = ret.z >= 0? ret.xy : OctahedronWrap(ret.xy);
    ret.xy = ret.xy * 0.5 + 0.5;
    return ret.xy;
}

inline float3 DecodeNormal(float2 enc)
{
    enc = enc * 2 - 1;

    float3 ret;
    ret.z = 1.f - abs(enc.x) - abs(enc.y);
    ret.xy = ret.z >= 0? enc.xy : OctahedronWrap(enc.xy);
    return normalize(ret);
}

inline float PackFloat2(float x, float y)
{
    uint val = uint(ceil(saturate(x) * 15.f)) | (uint(ceil(saturate(y) * 15.f)) << 4);
    return val / 255.f;
}

inline float2 UnpackFloat2(float val)
{
    uint uval = val * 255;
    return float2((uval & 15) / 15.f, ((uval >> 4) & 15) / 15.f);
}

// GBuffer0: r g b specPower/specIntensity
// GBuffer1: nx ny shadow ao
inline GBufferFragOut PackBrdfProperties(thread BrdfProperties &brdfProperties)
{
    GBufferFragOut frag_out;
    frag_out.gBuffer0 = float4(sqrt(brdfProperties.albedo), PackFloat2(brdfProperties.specPower / 32, brdfProperties.specIntensity));
    frag_out.gBuffer1 = float4(EncodeNormal(brdfProperties.normal), brdfProperties.shadow, brdfProperties.ao);
    return frag_out;
}

inline BrdfProperties UnpackBrdfProperties(float4 gBuffer0, float4 gBuffer1)
{
    BrdfProperties ret;
    ret.albedo = gBuffer0.xyz * gBuffer0.xyz;

    float2 powerIntensity = UnpackFloat2(gBuffer0.w);
    ret.specPower = powerIntensity.x * 32;
    ret.specIntensity = powerIntensity.y;

    ret.normal = DecodeNormal(gBuffer1.xy);
    ret.shadow = gBuffer1.z;
    ret.ao = gBuffer1.w;

    return ret;
}

inline float3 UnpackNormal(float2 gBuffer1)
{
    return DecodeNormal(gBuffer1.xy);
}

inline float evaluateModificationBrush(simd::float2 worldPosition,
                                       simd::float4 mousePosition,
                                       float        size)
{
    float dist = simd::length(worldPosition - mousePosition.xz);
    dist /= size;
    return saturate(min(2-dist, 1.0 / (1.0 + pow(dist*2.0, 4))));
}

inline uint wang_hash(uint seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

static float3 GetWorldPositionAndViewDirFromDepth(uint2 tid, float depth, constant RMDLUniforms& uniforms, thread float3& outViewDirection)
{
    float4 ndc;
    ndc.xy = (float2(tid) + 0.5) * uniforms.invScreenSize;
    ndc.xy = ndc.xy * 2 - 1;
    ndc.y *= -1;

    ndc.z = depth;
    ndc.w = 1;

    float4 worldPosition = uniforms.cameraUniforms.invViewProjectionMatrix * ndc;
    worldPosition.xyz /= worldPosition.w;

    ndc.z = 1.f;
    float4 viewDir = uniforms.cameraUniforms.invOrientationProjectionMatrix * ndc;
    viewDir /= viewDir.w;
    outViewDirection = viewDir.xyz;

    return worldPosition.xyz;
}
