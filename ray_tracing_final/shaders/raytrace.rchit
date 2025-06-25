#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "raycommon.glsl"
#include "wavefront.glsl"
#include "random.glsl"

hitAttributeEXT vec3 attribs;

layout(location = 0) rayPayloadInEXT hitPayload prd;
layout(location = 1) rayPayloadEXT shadowPayload prdShadow;

layout(buffer_reference, scalar) buffer Vertices {Vertex v[]; };
layout(buffer_reference, scalar) buffer Indices {ivec3 i[]; };
layout(buffer_reference, scalar) buffer Materials {WaveFrontMaterial m[]; };
layout(buffer_reference, scalar) buffer MatIndices {int i[]; };
layout(set = 0, binding = eTlas) uniform accelerationStructureEXT topLevelAS;
layout(set = 1, binding = eObjDescs, scalar) buffer ObjDesc_ { ObjDesc i[]; } objDesc;
layout(set = 1, binding = eTextures) uniform sampler2D textureSamplers[];

layout(push_constant) uniform _PushConstantRay { PushConstantRay pcRay; };

void main()
{
    // Object data
    ObjDesc    objResource = objDesc.i[gl_InstanceCustomIndexEXT];
    MatIndices matIndices  = MatIndices(objResource.materialIndexAddress);
    Materials  materials   = Materials(objResource.materialAddress);
    Indices    indices     = Indices(objResource.indexAddress);
    Vertices   vertices    = Vertices(objResource.vertexAddress);
  
    // Triangle indices
    ivec3 ind = indices.i[gl_PrimitiveID];
  
    // Triangle vertices
    Vertex v0 = vertices.v[ind.x];
    Vertex v1 = vertices.v[ind.y];
    Vertex v2 = vertices.v[ind.z];

    // Barycentric coordinates
    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    // Hit position
    const vec3 pos      = v0.pos * barycentrics.x + v1.pos * barycentrics.y + v2.pos * barycentrics.z;
    const vec3 worldPos = vec3(gl_ObjectToWorldEXT * vec4(pos, 1.0));

    // Normal at hit position
    const vec3 nrm      = v0.nrm * barycentrics.x + v1.nrm * barycentrics.y + v2.nrm * barycentrics.z;
    const vec3 worldNrm = normalize(vec3(nrm * gl_WorldToObjectEXT));

    // Material of the object
    int               matIdx = matIndices.i[gl_PrimitiveID];
    WaveFrontMaterial mat    = materials.m[matIdx];

    // Emissive component
    vec3 emittance = mat.emission;

    // Diffuse albedo
    vec3 albedo = mat.diffuse;
    if(mat.textureId >= 0)
    {
        uint txtId    = mat.textureId + objDesc.i[gl_InstanceCustomIndexEXT].txtOffset;
        vec2 texCoord = v0.texCoord * barycentrics.x + v1.texCoord * barycentrics.y + v2.texCoord * barycentrics.z;
        albedo *= texture(textureSamplers[nonuniformEXT(txtId)], texCoord).xyz;
    }

    // Vector toward the light
    vec3  L;
    float lightIntensity = pcRay.lightIntensity;
    float lightDistance  = 100000.0;
    if(pcRay.lightType == 0) // Point light
    {
        vec3 lDir      = pcRay.lightPosition - worldPos;
        lightDistance  = length(lDir);
        lightIntensity = pcRay.lightIntensity / (lightDistance * lightDistance);
        L              = normalize(lDir);
    }
    else // Directional light
    {
        L = normalize(pcRay.lightPosition);
    }

    // Shadow ray tracing
    float attenuation = 1.0;
    if(dot(worldNrm, L) > 0)
    {
        float tMin   = 0.001;
        float tMax   = lightDistance;
        vec3  origin = worldPos;
        vec3  rayDir = L;
        uint  flags  = gl_RayFlagsSkipClosestHitShaderEXT;
        prdShadow.isHit = true;
        prdShadow.seed  = prd.seed;
        traceRayEXT(topLevelAS, flags, 0xFF, 1, 0, 1, origin, tMin, rayDir, tMax, 1);
        prd.seed = prdShadow.seed;
        if(prdShadow.isHit)
        {
            attenuation = 0.3;
        }
    }

    // Direct lighting (diffuse and specular)
    vec3 diffuse  = computeDiffuse(mat, L, worldNrm) * attenuation * lightIntensity;
    vec3 specular = vec3(0);
    if(!prdShadow.isHit)
    {
        specular = computeSpecular(mat, gl_WorldRayDirectionEXT, L, worldNrm) * attenuation * lightIntensity;
    }

    // Path tracing for indirect lighting
    if(prd.depth < 10)
    {
        prd.depth++;
        vec3 tangent, bitangent;
        createCoordinateSystem(worldNrm, tangent, bitangent);
        vec3 rayOrigin    = worldPos;
        vec3 rayDirection;

        if(mat.illum == 3) // Reflective material
        {
            rayDirection = reflect(gl_WorldRayDirectionEXT, worldNrm);
            prd.attenuation *= mat.specular;
        }
        else // Diffuse material
        {
            rayDirection = samplingHemisphere(prd.seed, tangent, bitangent, worldNrm);
            float cos_theta = dot(rayDirection, worldNrm);
            float p = cos_theta / M_PI; // PDF for hemispherical sampling
            vec3 BRDF = albedo / M_PI;
            prd.attenuation *= albedo;
            prd.weight = BRDF * cos_theta / p;
        }

        prd.rayOrigin = rayOrigin;
        prd.rayDir    = rayDirection;
        prd.done      = 0;

        // Trace indirect ray
        float tMin = 0.001;
        float tMax = 100000000.0;
        uint flags = gl_RayFlagsOpaqueEXT;
        traceRayEXT(topLevelAS, flags, 0xFF, 1, 0, 1, rayOrigin, tMin, rayDirection, tMax, 0);

        // Combine direct and indirect lighting
        if(mat.illum != 3)
        {
            prd.hitValue = emittance + (diffuse + specular) + (prd.weight * prd.hitValue);
            //prd.hitValue = albedo;
        }
        else
        {
            prd.hitValue = emittance + (diffuse + specular) + (mat.specular * prd.hitValue);
            //prd.hitValue = albedo;
        }
    }
    else
    {
        //prd.hitValue = albedo;
        prd.hitValue = emittance + (diffuse + specular);
        prd.done = 1;
    }
}