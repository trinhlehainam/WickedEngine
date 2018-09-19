#ifndef _OCEAN_SIMULATOR_H
#define _OCEAN_SIMULATOR_H

#include "CommonInclude.h"
#include "wiGraphicsAPI.h"
#include "wiFFTGenerator.h"
#include "wiSceneSystem_Decl.h"

#include <vector>

struct wiOceanParameter
{
	// Must be power of 2.
	int dmap_dim;
	// Typical value is 1000 ~ 2000
	float patch_length;

	// Adjust the time interval for simulation.
	float time_scale;
	// Amplitude for transverse wave. Around 1.0
	float wave_amplitude;
	// Wind direction. Normalization not required.
	XMFLOAT2 wind_dir;
	// Around 100 ~ 1000
	float wind_speed;
	// This value damps out the waves against the wind direction.
	// Smaller value means higher wind dependency.
	float wind_dependency;
	// The amplitude for longitudinal wave. Must be positive.
	float choppy_scale;

	wiOceanParameter()
	{
		// Original version:
		//dmap_dim = 512;
		//patch_length = 2000.0f;
		//time_scale = 0.8f;
		//wave_amplitude = 0.35f;
		//wind_dir = XMFLOAT2(0.8f, 0.6f);
		//wind_speed = 600.0f;
		//wind_dependency = 0.07f;
		//choppy_scale = 1.3f;

		// Scaled version:
		dmap_dim = 512;
		patch_length = 50.0f;
		time_scale = 0.3f;
		wave_amplitude = 1000.0f;
		wind_dir = XMFLOAT2(0.8f, 0.6f);
		wind_speed = 600.0f;
		wind_dependency = 0.07f;
		choppy_scale = 1.3f;
	}
};


class wiOcean
{
public:
	wiOcean(const wiOceanParameter& params);
	~wiOcean();

	void UpdateDisplacementMap(float time, GRAPHICSTHREAD threadID);
	void Render(const wiSceneSystem::CameraComponent& camera, float time, GRAPHICSTHREAD threadID);

	wiGraphicsTypes::Texture2D* getDisplacementMap();
	wiGraphicsTypes::Texture2D* getGradientMap();

	const wiOceanParameter& getParameters();

	static void LoadShaders();
	static void SetUpStatic();
	static void CleanUpStatic();

	XMFLOAT3 waterColor = XMFLOAT3(powf(0.07f, 1.0f / 2.2f), powf(0.15f, 1.0f / 2.2f), powf(0.2f, 1.0f / 2.2f));
	float waterHeight = 0.0f;
	uint32_t surfaceDetail = 4;
	float surfaceDisplacementTolerance = 2;

protected:
	wiOceanParameter m_param;

	wiGraphicsTypes::Texture2D* m_pDisplacementMap;		// (RGBA32F)
	wiGraphicsTypes::Texture2D* m_pGradientMap;			// (RGBA16F)


	void initHeightMap(XMFLOAT2* out_h0, float* out_omega);


	// Initial height field H(0) generated by Phillips spectrum & Gauss distribution.
	wiGraphicsTypes::GPUBuffer* m_pBuffer_Float2_H0;

	// Angular frequency
	wiGraphicsTypes::GPUBuffer* m_pBuffer_Float_Omega;

	// Height field H(t), choppy field Dx(t) and Dy(t) in frequency domain, updated each frame.
	wiGraphicsTypes::GPUBuffer* m_pBuffer_Float2_Ht;

	// Height & choppy buffer in the space domain, corresponding to H(t), Dx(t) and Dy(t)
	wiGraphicsTypes::GPUBuffer* m_pBuffer_Float_Dxyz;


	wiGraphicsTypes::GPUBuffer* m_pImmutableCB;
	wiGraphicsTypes::GPUBuffer* m_pPerFrameCB;


	static CSFFT512x512_Plan m_fft_plan;

	static wiGraphicsTypes::ComputeShader* m_pUpdateSpectrumCS;
	static wiGraphicsTypes::ComputeShader* m_pUpdateDisplacementMapCS;
	static wiGraphicsTypes::ComputeShader* m_pUpdateGradientFoldingCS;
	static wiGraphicsTypes::VertexShader* g_pOceanSurfVS;
	static wiGraphicsTypes::PixelShader* g_pOceanSurfPS;
	static wiGraphicsTypes::PixelShader* g_pWireframePS;

	static wiGraphicsTypes::GPUBuffer* g_pShadingCB;

	static wiGraphicsTypes::RasterizerState rasterizerState;
	static wiGraphicsTypes::RasterizerState wireRS;
	static wiGraphicsTypes::DepthStencilState depthStencilState;
	static wiGraphicsTypes::BlendState blendState;

	static wiGraphicsTypes::GraphicsPSO PSO, PSO_wire;
	static wiGraphicsTypes::ComputePSO CPSO_updateSpectrum, CPSO_updateDisplacementMap, CPSO_updateGradientFolding;
};

#endif	// _OCEAN_SIMULATOR_H
