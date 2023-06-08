#define PI 3.14159265359f
#define SH_COSINE_LOBE_C0 0.886226925f // sqrt(pi)/2
#define SH_COSINE_LOBE_C1 1.02332671f // sqrt(pi/3)
#define SH_C0 0.282094792f // 1 / 2sqrt(pi)
#define SH_C1 0.488602512f // sqrt(3/pi) / 2

#define LPV_DIM 32
#define LPV_DIM_HALF 16
#define LPV_DIM_INVERSE 0.03125f
#define LPV_SCALE 0.25f

static const float FLT_MAX = asfloat(0x7F7FFFFF);

float LinearizeDepth(float nonLinearDepth, float4x4 invProj)
{   
    float4 ndcCoords = float4(0, 0, nonLinearDepth, 1.0f);
    
    // Unproject the vector into (homogenous) view-space vector
    float4 viewCoords = mul(invProj, ndcCoords);
    
    // Divide by w, which results in actual view-space z value
    float linearDepth = -viewCoords.z / viewCoords.w;
    return linearDepth;
}
float3 ReconstructWorldPosFromDepth(float2 uv, float depth, float4x4 invProj, float4x4 invView)
{   
    float ndcX = uv.x * 2 - 1;
    float ndcY = 1 - uv.y * 2; // Remember to flip y!!!
    float4 viewPos = mul(invProj, float4(ndcX, ndcY, depth, 1.0f));
    viewPos = viewPos / viewPos.w;
    return mul(invView, viewPos).xyz;
}
float3 ReconstructViewPosFromDepth(float2 uv, float depth, float4x4 invProj)
{
    float ndcX = uv.x * 2 - 1;
    float ndcY = 1 - uv.y * 2; // Remember to flip y!!!
    float4 viewPos = mul(invProj, float4(ndcX, ndcY, depth, 1.0f));
    viewPos = viewPos / viewPos.w;
    return viewPos.xyz;
}

// Helpers from: https://github.com/NVIDIAGameWorks/GettingStartedWithRTXRayTracing
// Generates a seed for a random number generator from 2 inputs plus a backoff
uint initRand(uint val0, uint val1, uint backoff = 16)
{
    uint v0 = val0, v1 = val1, s0 = 0;

	[unroll]
    for (uint n = 0; n < backoff; n++)
    {
        s0 += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
    }
    return v0;
}

// Takes our seed, updates it, and returns a pseudorandom float in [0..1]
float nextRand(inout uint s)
{
    s = (1664525u * s + 1013904223u);
    return float(s & 0x00FFFFFF) / float(0x01000000);
}

// Utility function to get a vector perpendicular to an input vector 
//    (from "Efficient Construction of Perpendicular Vectors Without Branching")
float3 getPerpendicularVector(float3 u)
{
    float3 a = abs(u);
    uint xm = ((a.x - a.y) < 0 && (a.x - a.z) < 0) ? 1 : 0;
    uint ym = (a.y - a.z) < 0 ? (1 ^ xm) : 0;
    uint zm = 1 ^ (xm | ym);
    return cross(u, float3(xm, ym, zm));
}

// A Fast and Robust Method for Avoiding Self-Intersection by Carsten Wächter and Nikolaus Binder, Chapter 6. Ray Tracing Gems NVIDIA
float3 OffsetRay(const float3 p, const float3 n)
{
    static const float origin = 1.0f / 32.0f;
    static const float floatScale = 1.0f / 65536.0f;
    static const float intScale = 256.0f;

    int3 of_i = int3(intScale * n.x, intScale * n.y, intScale * n.z);
    float3 p_i = float3(
		asfloat(asint(p.x) + ((p.x < 0) ? -of_i.x : of_i.x)),
		asfloat(asint(p.y) + ((p.y < 0) ? -of_i.y : of_i.y)),
		asfloat(asint(p.z) + ((p.z < 0) ? -of_i.z : of_i.z)));

    return float3(abs(p.x) < origin ? p.x + floatScale * n.x : p_i.x,
		abs(p.y) < origin ? p.y + floatScale * n.y : p_i.y,
		abs(p.z) < origin ? p.z + floatScale * n.z : p_i.z);
}

// Get a cosine-weighted random vector centered around a specified normal direction.
float3 GetCosHemisphereSample(inout uint randSeed, float3 hitNorm)  
{
	// Get 2 random numbers to select our sample with
    float2 randVal = float2(nextRand(randSeed), nextRand(randSeed));

	// Cosine weighted hemisphere sample from RNG
    float3 bitangent = getPerpendicularVector(hitNorm);
    float3 tangent = cross(bitangent, hitNorm);
    float r = sqrt(randVal.x);
    float phi = 2.0f * 3.14159265f * randVal.y;

	// Get our cosine-weighted hemisphere lobe sample direction
    return tangent * (r * cos(phi).x) + bitangent * (r * sin(phi)) + hitNorm.xyz * sqrt(1 - randVal.x);
}

struct Payload
{
    bool skipShading;
    float rayHitT;
};

struct ShadowPayload
{
    bool isHit;
};

float4 DirToSH(float3 direction)
{
    return float4(SH_C0, -SH_C1 * direction.y, SH_C1 * direction.z, -SH_C1 * direction.x);
}

float4 DirCosLobeToSH(float3 direction)
{
    return float4(SH_COSINE_LOBE_C0, -SH_COSINE_LOBE_C1 * direction.y, SH_COSINE_LOBE_C1 * direction.z, -SH_COSINE_LOBE_C1 * direction.x);
}