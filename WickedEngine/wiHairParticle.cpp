#include "wiHairParticle.h"
#include "wiRenderer.h"
#include "wiResourceManager.h"
#include "wiMath.h"
#include "wiFrustum.h"
#include "wiRandom.h"
#include "ResourceMapping.h"
#include "wiArchive.h"
#include "ShaderInterop.h"
#include "wiTextureHelper.h"
#include "wiSceneSystem.h"
#include "ShaderInterop_HairParticle.h"

using namespace std;
using namespace wiGraphicsTypes;

namespace wiSceneSystem
{

VertexShader *wiHairParticle::vs = nullptr;
PixelShader *wiHairParticle::ps[];
PixelShader *wiHairParticle::ps_simplest = nullptr;
ComputeShader *wiHairParticle::cs_simulate = nullptr;
DepthStencilState wiHairParticle::dss_default, wiHairParticle::dss_equal, wiHairParticle::dss_rejectopaque_keeptransparent;
RasterizerState wiHairParticle::rs, wiHairParticle::ncrs, wiHairParticle::wirers;
BlendState wiHairParticle::bs[2]; 
GraphicsPSO wiHairParticle::PSO[SHADERTYPE_COUNT][2];
GraphicsPSO wiHairParticle::PSO_wire;
ComputePSO wiHairParticle::CPSO_simulate;

void wiHairParticle::CleanUpStatic()
{
	SAFE_DELETE(vs);
	for (int i = 0; i < SHADERTYPE_COUNT; ++i)
	{
		SAFE_DELETE(ps[i]);
	}

	SAFE_DELETE(ps_simplest);
	SAFE_DELETE(cs_simulate);
}
void wiHairParticle::LoadShaders()
{
	vs = static_cast<VertexShader*>(wiResourceManager::GetShaderManager()->add(wiRenderer::SHADERPATH + "hairparticleVS.cso", wiResourceManager::VERTEXSHADER));
	

	for (int i = 0; i < SHADERTYPE_COUNT; ++i)
	{
		SAFE_INIT(ps[i]);
	}
	
	ps[SHADERTYPE_DEPTHONLY] = static_cast<PixelShader*>(wiResourceManager::GetShaderManager()->add(wiRenderer::SHADERPATH + "hairparticlePS_alphatestonly.cso", wiResourceManager::PIXELSHADER));
	ps[SHADERTYPE_DEFERRED] = static_cast<PixelShader*>(wiResourceManager::GetShaderManager()->add(wiRenderer::SHADERPATH + "hairparticlePS_deferred.cso", wiResourceManager::PIXELSHADER));
	ps[SHADERTYPE_FORWARD] = static_cast<PixelShader*>(wiResourceManager::GetShaderManager()->add(wiRenderer::SHADERPATH + "hairparticlePS_forward.cso", wiResourceManager::PIXELSHADER));
	ps[SHADERTYPE_TILEDFORWARD] = static_cast<PixelShader*>(wiResourceManager::GetShaderManager()->add(wiRenderer::SHADERPATH + "hairparticlePS_tiledforward.cso", wiResourceManager::PIXELSHADER));


	GraphicsDevice* device = wiRenderer::GetDevice();

	for (int i = 0; i < SHADERTYPE_COUNT; ++i)
	{
		if (ps[i] == nullptr)
		{
			continue;
		}

		for (int j = 0; j < 2; ++j)
		{
			if ((i == SHADERTYPE_DEPTHONLY || i == SHADERTYPE_DEFERRED) && j == 1)
			{
				continue;
			}

			GraphicsPSODesc desc;
			desc.vs = vs;
			desc.ps = ps[i];
			desc.bs = &bs[j];
			desc.rs = &ncrs;
			desc.dss = &dss_default;

			desc.DSFormat = wiRenderer::DSFormat_full;

			switch (i)
			{
			case SHADERTYPE_TEXTURE:
				desc.numRTs = 1;
				desc.RTFormats[0] = wiRenderer::RTFormat_hdr;
				break;
			case SHADERTYPE_FORWARD:
			case SHADERTYPE_TILEDFORWARD:
				desc.numRTs = 2;
				desc.RTFormats[0] = wiRenderer::RTFormat_hdr;
				desc.RTFormats[1] = wiRenderer::RTFormat_gbuffer_1;
				break;
			case SHADERTYPE_DEFERRED:
				desc.numRTs = 4;
				desc.RTFormats[0] = wiRenderer::RTFormat_gbuffer_0;
				desc.RTFormats[1] = wiRenderer::RTFormat_gbuffer_1;
				desc.RTFormats[2] = wiRenderer::RTFormat_gbuffer_2;
				desc.RTFormats[3] = wiRenderer::RTFormat_gbuffer_3;
			default:
				break;
			}

			if (i == SHADERTYPE_TILEDFORWARD)
			{
				desc.dss = &dss_equal; // opaque
			}

			if(j == 1)
			{
				desc.dss = &dss_rejectopaque_keeptransparent; // transparent
				desc.numRTs = 1;
			}

			device->CreateGraphicsPSO(&desc, &PSO[i][j]);
		}
	}

	SAFE_INIT(ps_simplest);
	ps_simplest = static_cast<PixelShader*>(wiResourceManager::GetShaderManager()->add(wiRenderer::SHADERPATH + "hairparticlePS_simplest.cso", wiResourceManager::PIXELSHADER));

	{
		GraphicsPSODesc desc;
		desc.vs = vs;
		desc.ps = ps_simplest;
		desc.bs = &bs[0];
		desc.rs = &wirers;
		desc.dss = &dss_default;
		desc.numRTs = 1;
		desc.RTFormats[0] = wiRenderer::RTFormat_hdr;
		desc.DSFormat = wiRenderer::DSFormat_full;
		device->CreateGraphicsPSO(&desc, &PSO_wire);
	}

	SAFE_INIT(cs_simulate);
	cs_simulate = static_cast<ComputeShader*>(wiResourceManager::GetShaderManager()->add(wiRenderer::SHADERPATH + "hairparticle_simulateCS.cso", wiResourceManager::COMPUTESHADER));

	{
		ComputePSODesc desc;
		desc.cs = cs_simulate;
		device->CreateComputePSO(&desc, &CPSO_simulate);
	}
}
void wiHairParticle::SetUpStatic()
{
	RasterizerStateDesc rsd;
	rsd.FillMode=FILL_SOLID;
	rsd.CullMode=CULL_BACK;
	rsd.FrontCounterClockwise=true;
	rsd.DepthBias=0;
	rsd.DepthBiasClamp=0;
	rsd.SlopeScaledDepthBias=0;
	rsd.DepthClipEnable=true;
	rsd.MultisampleEnable=false;
	rsd.AntialiasedLineEnable=false;
	wiRenderer::GetDevice()->CreateRasterizerState(&rsd, &rs);

	rsd.FillMode=FILL_SOLID;
	rsd.CullMode=CULL_NONE;
	rsd.FrontCounterClockwise=true;
	rsd.DepthBias=0;
	rsd.DepthBiasClamp=0;
	rsd.SlopeScaledDepthBias=0;
	rsd.DepthClipEnable=true;
	rsd.MultisampleEnable=false;
	rsd.AntialiasedLineEnable=false;
	wiRenderer::GetDevice()->CreateRasterizerState(&rsd, &ncrs);

	rsd.FillMode = FILL_WIREFRAME;
	rsd.CullMode = CULL_NONE;
	rsd.FrontCounterClockwise = true;
	rsd.DepthBias = 0;
	rsd.DepthBiasClamp = 0;
	rsd.SlopeScaledDepthBias = 0;
	rsd.DepthClipEnable = true;
	rsd.MultisampleEnable = false;
	rsd.AntialiasedLineEnable = false;
	wiRenderer::GetDevice()->CreateRasterizerState(&rsd, &wirers);

	
	DepthStencilStateDesc dsd;
	dsd.DepthEnable = true;
	dsd.DepthWriteMask = DEPTH_WRITE_MASK_ALL;
	dsd.DepthFunc = COMPARISON_GREATER;

	dsd.StencilEnable = true;
	dsd.StencilReadMask = 0xFF;
	dsd.StencilWriteMask = 0xFF;
	dsd.FrontFace.StencilFunc = COMPARISON_ALWAYS;
	dsd.FrontFace.StencilPassOp = STENCIL_OP_REPLACE;
	dsd.FrontFace.StencilFailOp = STENCIL_OP_KEEP;
	dsd.FrontFace.StencilDepthFailOp = STENCIL_OP_KEEP;
	dsd.BackFace.StencilFunc = COMPARISON_ALWAYS;
	dsd.BackFace.StencilPassOp = STENCIL_OP_REPLACE;
	dsd.BackFace.StencilFailOp = STENCIL_OP_KEEP;
	dsd.BackFace.StencilDepthFailOp = STENCIL_OP_KEEP;
	wiRenderer::GetDevice()->CreateDepthStencilState(&dsd, &dss_default);

	dsd.DepthWriteMask = DEPTH_WRITE_MASK_ZERO;
	dsd.DepthFunc = COMPARISON_EQUAL;
	wiRenderer::GetDevice()->CreateDepthStencilState(&dsd, &dss_equal);
	dsd.DepthFunc = COMPARISON_GREATER;
	wiRenderer::GetDevice()->CreateDepthStencilState(&dsd, &dss_rejectopaque_keeptransparent);

	
	BlendStateDesc bld;
	bld.RenderTarget[0].BlendEnable=false;
	bld.AlphaToCoverageEnable=false; // maybe for msaa
	wiRenderer::GetDevice()->CreateBlendState(&bld, &bs[0]);

	bld.RenderTarget[0].SrcBlend = BLEND_SRC_ALPHA;
	bld.RenderTarget[0].DestBlend = BLEND_INV_SRC_ALPHA;
	bld.RenderTarget[0].BlendOp = BLEND_OP_ADD;
	bld.RenderTarget[0].SrcBlendAlpha = BLEND_ONE;
	bld.RenderTarget[0].DestBlendAlpha = BLEND_ONE;
	bld.RenderTarget[0].BlendOpAlpha = BLEND_OP_ADD;
	bld.RenderTarget[0].BlendEnable = true;
	bld.RenderTarget[0].RenderTargetWriteMask = COLOR_WRITE_ENABLE_ALL;
	bld.AlphaToCoverageEnable = false;
	bld.IndependentBlendEnable = false;
	wiRenderer::GetDevice()->CreateBlendState(&bld, &bs[1]);





	LoadShaders();
}

void wiHairParticle::UpdateRenderData(const MeshComponent& mesh, const MaterialComponent& material, GRAPHICSTHREAD threadID)
{
	if (strandCount == 0)
	{
		return;
	}

	bool regenerate = false;

	if(cb == nullptr || (strandCount * segmentCount) != particleBuffer->GetDesc().ByteWidth / sizeof(Patch))
	{
		regenerate = true;

		cb.reset(new GPUBuffer);
		particleBuffer.reset(new GPUBuffer);
		simulationBuffer.reset(new GPUBuffer);

		GPUBufferDesc bd;
		bd.Usage = USAGE_DEFAULT;
		bd.BindFlags = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
		bd.CPUAccessFlags = 0;
		bd.MiscFlags = RESOURCE_MISC_BUFFER_STRUCTURED;

		bd.StructureByteStride = sizeof(Patch);
		bd.ByteWidth = bd.StructureByteStride * strandCount * segmentCount;
		wiRenderer::GetDevice()->CreateBuffer(&bd, nullptr, particleBuffer.get());

		bd.StructureByteStride = sizeof(PatchSimulationData);
		bd.ByteWidth = bd.StructureByteStride * strandCount * segmentCount;
		wiRenderer::GetDevice()->CreateBuffer(&bd, nullptr, simulationBuffer.get());

		bd.Usage = USAGE_DYNAMIC;
		bd.ByteWidth = sizeof(HairParticleCB);
		bd.BindFlags = BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = CPU_ACCESS_WRITE;
		bd.MiscFlags = 0;
		wiRenderer::GetDevice()->CreateBuffer(&bd, nullptr, cb.get());
	}

	GraphicsDevice* device = wiRenderer::GetDevice();
	device->EventBegin("HairParticle - UpdateRenderData", threadID);

	device->BindComputePSO(&CPSO_simulate, threadID);

	HairParticleCB hcb;
	hcb.xWorld = world;
	hcb.xColor = material.baseColor;
	hcb.xHairRegenerate = regenerate ? 1 : 0;
	hcb.xLength = length;
	hcb.xStiffness = stiffness;
	hcb.xHairRandomness = randomness;
	hcb.xHairStrandCount = strandCount;
	hcb.xHairSegmentCount = max(segmentCount, 1);
	hcb.xHairParticleCount = hcb.xHairStrandCount * hcb.xHairSegmentCount;
	hcb.xHairRandomSeed = randomSeed;
	hcb.xHairViewDistance = viewDistance;
	hcb.xHairBaseMeshIndexCount = (uint)mesh.indices.size();
	hcb.xHairBaseMeshVertexPositionStride = sizeof(MeshComponent::Vertex_POS);
	// segmentCount will be loop in the shader, not a threadgroup so we don't need it here:
	hcb.xHairNumDispatchGroups = (uint)ceilf((float)strandCount / (float)THREADCOUNT_SIMULATEHAIR);
	device->UpdateBuffer(cb.get(), &hcb, threadID);

	device->BindConstantBuffer(CS, cb.get(), CB_GETBINDSLOT(HairParticleCB), threadID);

	GPUResource* uavs[] = {
		particleBuffer.get(),
		simulationBuffer.get()
	};
	device->BindUAVs(CS, uavs, 0, ARRAYSIZE(uavs), threadID);

	GPUResource* res[] = {
		mesh.indexBuffer.get(),
		mesh.streamoutBuffer_POS != nullptr ? mesh.streamoutBuffer_POS.get() : mesh.vertexBuffer_POS.get()
	};
	device->BindResources(CS, res, TEXSLOT_ONDEMAND0, ARRAYSIZE(res), threadID);

	device->Dispatch(hcb.xHairNumDispatchGroups, 1, 1, threadID);

	device->UnbindUAVs(0, ARRAYSIZE(uavs), threadID);
	device->UnbindResources(TEXSLOT_ONDEMAND0, ARRAYSIZE(res), threadID);

	device->EventEnd(threadID);
}

void wiHairParticle::Draw(const CameraComponent& camera, const MaterialComponent& material, SHADERTYPE shaderType, bool transparent, GRAPHICSTHREAD threadID) const
{
	if (strandCount == 0 || cb == nullptr)
	{
		return;
	}

	GraphicsDevice* device = wiRenderer::GetDevice();
	device->EventBegin("HairParticle - Draw", threadID);

	if (wiRenderer::IsWireRender())
	{
		if (transparent || shaderType == SHADERTYPE_DEPTHONLY)
		{
			return;
		}
		device->BindGraphicsPSO(&PSO_wire, threadID);
		device->BindResource(VS, wiTextureHelper::getInstance()->getWhite(), TEXSLOT_ONDEMAND0, threadID);
	}
	else
	{
		device->BindGraphicsPSO(&PSO[shaderType][transparent], threadID);

		GPUResource* res[] = {
			material.GetBaseColorMap()
		};
		device->BindResources(PS, res, TEXSLOT_ONDEMAND0, ARRAYSIZE(res), threadID);
		device->BindResources(VS, res, TEXSLOT_ONDEMAND0, ARRAYSIZE(res), threadID);
	}

	device->BindConstantBuffer(VS, cb.get(), CB_GETBINDSLOT(HairParticleCB), threadID);

	device->BindResource(VS, particleBuffer.get(), 0, threadID);

	device->Draw((int)strandCount * 12 * max(segmentCount, 1), 0, threadID);

	device->EventEnd(threadID);
}


void wiHairParticle::Serialize(wiArchive& archive, uint32_t seed)
{
	if (archive.IsReadMode())
	{
		archive >> _flags;
		wiECS::SerializeEntity(archive, meshID, seed);
		archive >> strandCount;
		archive >> segmentCount;
		archive >> randomSeed;
		archive >> length;
		archive >> stiffness;
		archive >> randomness;
		archive >> viewDistance;
	}
	else
	{
		archive << _flags;
		wiECS::SerializeEntity(archive, meshID, seed);
		archive << strandCount;
		archive << segmentCount;
		archive << randomSeed;
		archive << length;
		archive << stiffness;
		archive << randomness;
		archive << viewDistance;
	}
}

}
