#pragma once

#include "DXRSGraphics.h"
#include "DXRSTimer.h"
#include "DXRSModel.h"
#include "DXRSRenderTarget.h"
#include "DXRSDepthBuffer.h"
#include "DXRSBuffer.h"

#include "RootSignature.h"
#include "PipelineStateObject.h"

#define RSM_SAMPLES_COUNT 512

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
	void UpdateBuffers(DXRSTimer const& timer);
	void Render();
	void SetProjectionMatrix();
	void UpdateLights();
	void UpdateControls();
	void UpdateCamera();
	void UpdateShadow();
	void UpdateImGui();

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

	U_PTR<DXRSModel>                                     mDragonModel;
	U_PTR<DXRSModel>                                     mSphereModel_1;
	U_PTR<DXRSModel>                                     mSphereModel_2;
	U_PTR<DXRSModel>                                     mBlockModel;
	U_PTR<DXRSModel>                                     mRoomModel;

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
	RootSignature                                        mRSMBuffersRS;
	RootSignature                                        mRSMUpsampleAndBlurRS;
	DXRSRenderTarget*			                         mRSMRT;
	std::vector<DXRSRenderTarget*>                       mRSMBuffersRTs;
	DXRSRenderTarget*			                         mRSMUpsampleAndBlurRT;
	GraphicsPSO                                          mRSMPSO;
	GraphicsPSO											 mRSMBuffersPSO;
	ComputePSO											 mRSMUpsampleAndBlurPSO;
	DXRSBuffer* mRSMCB;
	DXRSBuffer* mRSMCB2;
	DXRSBuffer* mRSMUpsampleAndBlurCB;

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

	__declspec(align(16)) struct RSMCBDataUpsampleAndBlur
	{
		XMMATRIX ShadowViewProjection;
		float RSMIntensity;
		float RSMRMax;
		XMFLOAT2 UpsampleRatio;
	};

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

	// Directional light
	float mDirectionalLightColor[4]{ 0.9, 0.9, 0.9, 1.0 };
	float mDirectionalLightDir[4]{ 0.191, 1.0f, 0.574f, 1.0 };
	float mDirectionalLightIntensity = 1.7f;

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
	bool mRSMEnabled = true;
	bool mRSMUseUpsampleAndBlur = true;

	XMMATRIX mWorld;
};
