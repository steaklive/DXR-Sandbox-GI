#pragma once

#include "DXRSGraphics.h"
#include "DXRSTimer.h"
#include "DXRSModel.h"
#include "DXRSRenderTarget.h"
#include "DXRSDepthBuffer.h"
#include "DXRSBuffer.h"

#include "RootSignature.h"
#include "PipelineStateObject.h"

#define SHADOWMAP_SIZE 2048
#define RSM_SIZE 2048
#define RSM_SAMPLES_COUNT 512
#define LPV_DIM 32
#define VCT_SCENE_VOLUME_SIZE 256

class DXRSExampleGIScene
{
public:
	DXRSExampleGIScene();
	~DXRSExampleGIScene();

	void Init(HWND window, int width, int height);
	void Clear();
	void Run();
	void OnWindowSizeChanged(int width, int height);

private:
	void Update(DXRSTimer const& timer);
	void UpdateTransforms(DXRSTimer const& timer);
	void UpdateBuffers(DXRSTimer const& timer);
	void UpdateLights(DXRSTimer const& timer);
	void UpdateControls();
	void UpdateCamera();
	void UpdateImGui();
	
	void InitGbuffer(ID3D12Device* device, DXRS::DescriptorHeapManager* descriptorManager);
	void InitShadowMapping(ID3D12Device* device, DXRS::DescriptorHeapManager* descriptorManager);
	void InitReflectiveShadowMapping(ID3D12Device* device, DXRS::DescriptorHeapManager* descriptorManager);
	void InitLightPropagationVolume(ID3D12Device* device, DXRS::DescriptorHeapManager* descriptorManager);
	void InitVoxelConeTracing(ID3D12Device* device, DXRS::DescriptorHeapManager* descriptorManager);
	void InitLighting(ID3D12Device* device, DXRS::DescriptorHeapManager* descriptorManager);
	void InitComposite(ID3D12Device* device, DXRS::DescriptorHeapManager* descriptorManager);

	void Render();
	void RenderGbuffer(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, DXRS::GPUDescriptorHeap* gpuDescriptorHeap);
	void RenderShadowMapping(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, DXRS::GPUDescriptorHeap* gpuDescriptorHeap);
	void RenderReflectiveShadowMapping(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, DXRS::GPUDescriptorHeap* gpuDescriptorHeap);
	void RenderLightPropagationVolume(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, DXRS::GPUDescriptorHeap* gpuDescriptorHeap);
	void RenderVoxelConeTracing(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, DXRS::GPUDescriptorHeap* gpuDescriptorHeap);
	void RenderLighting(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, DXRS::GPUDescriptorHeap* gpuDescriptorHeap);
	void RenderComposite(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, DXRS::GPUDescriptorHeap* gpuDescriptorHeap);
	void RenderObject(U_PTR<DXRSModel>& aModel, std::function<void(U_PTR<DXRSModel>&)> aCallback);

	void SetProjectionMatrix();
	void ThrowFailedErrorBlob(ID3DBlob* blob);

	DXRSGraphics* mSandboxFramework;

	U_PTR<GamePad>                                       mGamePad;
	U_PTR<Keyboard>                                      mKeyboard;
	U_PTR<Mouse>                                         mMouse;
	DirectX::GamePad::ButtonStateTracker                 mGamePadButtons;
	DirectX::Keyboard::KeyboardStateTracker              mKeyboardButtons;

	DXRSTimer                                            mTimer;

	std::vector<CD3DX12_RESOURCE_BARRIER>                mBarriers;

	U_PTR<GraphicsMemory>                                mGraphicsMemory;
	U_PTR<CommonStates>                                  mStates;

	std::vector<U_PTR<DXRSModel>>						 mObjects;

	// Gbuffer
	RootSignature                                        mGbufferRS;
	std::vector<DXRSRenderTarget*>                       mGbufferRTs;
	GraphicsPSO                                          mGbufferPSO;
	DXRSBuffer* mGbufferCB;
	DXRSDepthBuffer* mDepthStencil;
	DXRS::DescriptorHandle                               mNullDescriptor;

	__declspec(align(16)) struct GBufferCBData
	{
		XMMATRIX ViewProjection;
		XMMATRIX InvViewProjection;
		XMFLOAT4 CameraPos;
		XMFLOAT4 ScreenSize;
		XMFLOAT4 LightColor;
	};

	// RSM
	RootSignature                                        mRSMRS;
	RootSignature                                        mRSMRS_Compute;
	RootSignature                                        mRSMBuffersRS;
	RootSignature                                        mRSMUpsampleAndBlurRS;
	RootSignature                                        mRSMDownsampleRS;
	RootSignature                                        mRSMDownsampleRS_Compute;
	DXRSRenderTarget*			                         mRSMRT;
	std::vector<DXRSRenderTarget*>                       mRSMBuffersRTs;
	std::vector<DXRSRenderTarget*>                       mRSMDownsampledBuffersRTs;
	DXRSRenderTarget*			                         mRSMUpsampleAndBlurRT;
	GraphicsPSO                                          mRSMPSO;
	ComputePSO                                           mRSMPSO_Compute;
	GraphicsPSO											 mRSMBuffersPSO;
	GraphicsPSO											 mRSMDownsamplePSO;
	ComputePSO											 mRSMDownsamplePSO_Compute;
	ComputePSO											 mRSMUpsampleAndBlurPSO;
	DXRSBuffer* mRSMCB;
	DXRSBuffer* mRSMCB2;
	DXRSBuffer* mRSMDownsampleCB;

	__declspec(align(16)) struct RSMCBData
	{
		XMMATRIX ShadowViewProjection;
		float RSMIntensity;
		float RSMRMax;
		XMFLOAT2 UpsampleRatio;
	};

	__declspec(align(16)) struct RSMCBDataRandomValues
	{
		XMFLOAT4 xi[RSM_SAMPLES_COUNT];
	};

	__declspec(align(16)) struct RSMCBDataDownsample
	{
		XMFLOAT4 LightDir;
		int ScaleSize;
	};

	// LPV
	RootSignature                                        mLPVInjectionRS;
	RootSignature                                        mLPVPropagationRS;
	GraphicsPSO											 mLPVInjectionPSO;
	GraphicsPSO											 mLPVPropagationPSO;
	std::vector<DXRSRenderTarget*>                       mLPVSHColorsRTs;
	std::vector<DXRSRenderTarget*>                       mLPVAccumulationSHColorsRTs;
	__declspec(align(16)) struct LPVCBData
	{
		XMMATRIX worldToLPV;
		float LPVCutoff;
		float LPVPower;
		float LPVAttenuation;
	};
	DXRSBuffer* mLPVCB;

	RootSignature                                        mVCTVoxelizationRS;
	GraphicsPSO											 mVCTVoxelizationPSO;
	DXRSRenderTarget*									 mVCTVoxelization3DRT;
	__declspec(align(16)) struct VCTVoxelizationCBData
	{
		XMMATRIX ProjectionX;
		XMMATRIX ProjectionY;
		XMMATRIX ProjectionZ;
	};
	DXRSBuffer* mVCTVoxelizationCB;

	// Composite
	RootSignature mCompositeRS;
	GraphicsPSO mCompositePSO;

	// UI
	ComPtr<ID3D12DescriptorHeap> mUIDescriptorHeap;

	// Lighting
	RootSignature mLightingRS;
	std::vector<DXRSRenderTarget*> mLightingRTs;
	GraphicsPSO mLightingPSO;
	DXRSBuffer* mLightingCB;
	DXRSBuffer* mLightsInfoCB;
	DXRSBuffer* mIlluminationFlagsCB;

	__declspec(align(16)) struct LightingCBData
	{
		XMMATRIX InvViewProjection;
		XMMATRIX ShadowViewProjection;
		XMFLOAT4 CameraPos;
		XMFLOAT4 ScreenSize;
		XMFLOAT2 ShadowTexelSize;
		float ShadowIntensity;
		float pad;
	};
	__declspec(align(16)) struct LightsInfoCBData
	{
		XMFLOAT4 LightDirection;
		XMFLOAT4 LightColor;
		float LightIntensity;
		XMFLOAT3 pad;
	};

	__declspec(align(16)) struct IlluminationFlagsCBData
	{
		int useDirect;
		int useShadows;
		int useRSM;
		int useLPV;
	};

	// Directional light
	float mDirectionalLightColor[4]{ 0.9, 0.9, 0.9, 1.0 };
	float mDirectionalLightDir[4]{ 0.191, 1.0f, 0.574f, 1.0 };
	float mDirectionalLightIntensity = 1.7f;
	bool mDynamicDirectionalLight = false;
	float mDynamicDirectionalLightSpeed = 1.0f;

	bool mUseDirectLight = true;
	bool mUseShadows = true;
	bool mUseRSM = false;
	bool mUseLPV = false;

	// Shadows
	GraphicsPSO mShadowMappingPSO;
	DXRSDepthBuffer* mShadowDepth;
	XMMATRIX mLightViewProjection;
	XMMATRIX mLightView;
	XMMATRIX mLightProj;
	DXRSBuffer* mShadowMappingCB;
	RootSignature mShadowMappingRS;
	float mShadowIntensity = 0.5f;

	__declspec(align(16)) struct ShadowMappingCBData
	{
		XMMATRIX LightViewProj;
		XMFLOAT4 LightColor;
		XMFLOAT4 LightDir;
	};

	// Camera
	__declspec(align(16)) struct CameraBuffer
	{
		XMMATRIX view;
		XMMATRIX projection;
		XMMATRIX viewI;
		XMMATRIX projectionI;
		XMMATRIX camToWorld;
		XMFLOAT4 camPosition;
		XMFLOAT2 resolution;
	};

	DXRSBuffer* mCameraBuffer;
	float mCameraTheta = -1.5f * XM_PI;
	float mCameraPhi = XM_PI / 3;
	float mCameraRadius = 45.0f;
	Vector3 mCameraEye{ 0.0f, 0.0f, 0.0f };
	XMMATRIX mCameraView;
	XMMATRIX mCameraProjection;
	XMFLOAT2 mLastMousePosition;

	float mRSMIntensity = 0.146f;
	float mRSMRMax = 0.015f;
	float mRSMRTRatio = 0.33333f; // from MAX_SCREEN_WIDTH/HEIGHT
	bool mRSMEnabled = true;
	bool mRSMUseUpsampleAndBlur = true;
	bool mRSMComputeVersion = false;
	UINT mRSMDownsampleScaleSize = 4;
	bool mRSMDownsampleForLPV = false;
	bool mRSMDownsampleUseCS = false;

	bool mLPVEnabled = true;
	int mLPVPropagationSteps = 50;
	float mLPVCutoff = 0.2f;
	float mLPVPower = 1.8f;
	float mLPVAttenuation = 1.0f;
	XMMATRIX mWorldToLPV;

	D3D12_DEPTH_STENCIL_DESC mDepthStateRW;
	D3D12_DEPTH_STENCIL_DESC mDepthStateRead;
	D3D12_DEPTH_STENCIL_DESC mDepthStateDisabled;
	D3D12_BLEND_DESC mBlendState;
	D3D12_BLEND_DESC mBlendStateLPVPropagation;
	D3D12_RASTERIZER_DESC mRasterizerState;
	D3D12_RASTERIZER_DESC mRasterizerStateShadow;
	D3D12_SAMPLER_DESC mBilinearSampler;
};
