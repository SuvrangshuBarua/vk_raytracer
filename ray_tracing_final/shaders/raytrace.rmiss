#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "raycommon.glsl"
#include "wavefront.glsl"

layout(location = 0) rayPayloadInEXT hitPayload prd;

layout(push_constant) uniform _PushConstantRay
{
  PushConstantRay pcRay;
};

void main()
{
   //prd.hitValue = pcRay.clearColor.xyz * 0.8;
   //prd.hitValue = clearColor.xyz * 0.8;
  if(prd.depth == 0)
    prd.hitValue = pcRay.clearColor.xyz * 0.8;
  else
    prd.hitValue = vec3(0.2, 0.2, 0.3);  // No contribution from environment
  prd.depth = 100;              // Ending trace
}