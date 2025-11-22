//
//  RMDLMainRenderer_shared.h
//  Loupy
//
//  Created by Rémy on 22/11/2025.
//

#ifndef RMDLMainRenderer_shared_h
#define RMDLMainRenderer_shared_h

#include <simd/simd.h>

struct RMDLCameraUniforms
{
    simd::float4x4      viewMatrix;
    simd::float4x4      projectionMatrix;
    simd::float4x4      viewProjectionMatrix;
    simd::float4x4      invOrientationProjectionMatrix;
    simd::float4x4      invViewProjectionMatrix;
    simd::float4x4      invProjectionMatrix;
    simd::float4x4      invViewMatrix;
    simd::float4        frustumPlanes[6];
};

struct RMDLUniforms
{
    RMDLCameraUniforms  cameraUniforms;
    RMDLCameraUniforms  shadowCameraUniforms[3];

    // Mouse state: x, y = position in pixels; z = buttons
    simd::float3        mouseState;
    simd::float2        invScreenSize;
    float               projectionYScale;
    float               brushSize;

    float               ambientOcclusionContrast;
    float               ambientOcclusionScale;
    float               ambientLightScale;
#if !USE_CONST_GAME_TIME
    float               gameTime;
#endif
    float               frameTime;  // TODO. this doesn't appear to be initialized until UpdateCpuUniforms. OK?
};

struct Plane
{
    simd::float3        normal = { 0.f, 1.f, 0.f };
    float               distance = 0.f;
};

struct Frustum
{
    Plane               topFace;
    Plane               bottomFace;
    Plane               rightFace;
    Plane               leftFace;
    Plane               farFace;
    Plane               nearFace;
};

struct RMDLObjVertex
{
    simd::float3    position;
    simd::float3    normal;
    simd::float3    color;
};

#endif /* RMDLMainRenderer_shared_h */
