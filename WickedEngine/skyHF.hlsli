#ifndef _SKY_HF_
#define _SKY_HF_
#include "globals.hlsli"
#include "lightingHF.hlsli"


float3 GetStaticSkyColor(in float3 normal)
{
	return texture_env_global.SampleLevel(sampler_linear_clamp, normal, 0).rgb;
}

float3 GetDynamicSkyColor(in float3 normal)
{
	float aboveHorizon = saturate(pow(saturate(normal.y), 1.0f / 4.0f + g_xWorld_Fog.z) / (g_xWorld_Fog.z + 1));
	float3 sky = lerp(g_xWorld_Horizon, g_xWorld_Zenith, aboveHorizon);
	float3 sun = normal.y > 0 ? max(saturate(dot(GetSunDirection(), normal) > 0.9998 ? 1 : 0)*GetSunColor() * 10000, 0) : 0;
	return sky + sun;
}

void AddCloudLayer(inout float4 color, in float3 normal, bool dark)
{
	float3 o = g_xCamera_CamPos;
	float3 d = normal;
	float3 planeOrigin = float3(0, 1000, 0);
	float3 planeNormal = float3(0, -1, 0);
	float t = Trace_plane(o, d, planeOrigin, planeNormal);

	if (t < 0)
	{
		return;
	}

	float3 cloudPos = o + d * t;
	float2 cloudUV = planeOrigin.xz - cloudPos.xz;
	cloudUV *= g_xWorld_CloudScale;

	float clouds = texture_0.SampleLevel(sampler_linear_mirror, cloudUV, 0).r;
	float cloudOpacity = saturate(clouds - (1 - g_xWorld_Cloudiness)) * pow(saturate(normal.y), 0.5);

	if (dark)
	{
		color.rgb *= pow(saturate(1 - cloudOpacity), 16.0f);
	}
	else
	{
		color.rgb = lerp(color.rgb, clouds.xxx, cloudOpacity);
	}
}


#endif // _SKY_HF_
