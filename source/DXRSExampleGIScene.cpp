#include "DXRSExampleGIScene.h"

#include "DescriptorHeap.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

namespace {
	D3D12_HEAP_PROPERTIES UploadHeapProps = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0, 0 };
	D3D12_HEAP_PROPERTIES DefaultHeapProps = { D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0, 0 };
}

DXRSExampleGIScene::DXRSExampleGIScene()
{
	mSandboxFramework = new DXRSGraphics();
}

DXRSExampleGIScene::~DXRSExampleGIScene()
{
	if (mSandboxFramework)
		mSandboxFramework->WaitForGpu();

	delete mLightingCB;
	delete mLightsInfoCB;
	delete mShadowMappingCB;
	delete mGbufferCB;
	delete mCameraBuffer;
	delete mRSMCB;
	delete mRSMCB2;
}

void DXRSExampleGIScene::Init(HWND window, int width, int height)
{
	mGamePad = std::make_unique<DirectX::GamePad>();
	mKeyboard = std::make_unique<DirectX::Keyboard>();
	mMouse = std::make_unique<DirectX::Mouse>();
	mMouse->SetWindow(window);

	mSandboxFramework->SetWindow(window, width, height);

	mSandboxFramework->CreateResources();
	mSandboxFramework->CreateFullscreenQuadBuffers();

	mDragonModel = U_PTR<DXRSModel>(new DXRSModel(*mSandboxFramework, mSandboxFramework->GetFilePath("content\\models\\dragon.fbx"), true, XMMatrixIdentity(), XMFLOAT4(0, 1, 0, 0.0)));
	mRoomModel = U_PTR<DXRSModel>(new DXRSModel(*mSandboxFramework, mSandboxFramework->GetFilePath("content\\models\\room.fbx"), true, XMMatrixIdentity(), XMFLOAT4(0.7, 0.7, 0.7, 0.0)));
	mSphereModel_1 = U_PTR<DXRSModel>(new DXRSModel(*mSandboxFramework, mSandboxFramework->GetFilePath("content\\models\\sphere.fbx"), true, XMMatrixIdentity(), XMFLOAT4(0.0, 0.2, 0.9, 0.0)));
	mSphereModel_2 = U_PTR<DXRSModel>(new DXRSModel(*mSandboxFramework, mSandboxFramework->GetFilePath("content\\models\\sphere.fbx"), true, XMMatrixIdentity(), XMFLOAT4(0.8, 0.4, 0.1, 0.0)));
	mBlockModel = U_PTR<DXRSModel>(new DXRSModel(*mSandboxFramework, mSandboxFramework->GetFilePath("content\\models\\block.fbx"), true, XMMatrixIdentity(), XMFLOAT4(0.9, 0.15, 1.0, 0.0)));

	mSandboxFramework->FinalizeResources();
	ID3D12Device* device = mSandboxFramework->GetD3DDevice();

	mStates = std::make_unique<CommonStates>(device);
	mGraphicsMemory = std::make_unique<GraphicsMemory>(device);

	mSandboxFramework->CreateWindowResources();
	SetProjectionMatrix();

	auto descriptorManager = mSandboxFramework->GetDescriptorHeapManager();

	#pragma region ImGui

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	// Setup Platform/Renderer bindings
	ImGui_ImplWin32_Init(window);

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NumDescriptors = 1;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mUIDescriptorHeap)));
	ImGui_ImplDX12_Init(device, 3, DXGI_FORMAT_R8G8B8A8_UNORM, mUIDescriptorHeap.Get(), mUIDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), mUIDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

#pragma endregion

	// init camera CB
	DXRSBuffer::Description cbDescDXR;
	cbDescDXR.mElementSize = sizeof(CameraBuffer);
	cbDescDXR.mState = D3D12_RESOURCE_STATE_GENERIC_READ;
	cbDescDXR.mDescriptorType = DXRSBuffer::DescriptorType::CBV;
	mCameraBuffer = new DXRSBuffer(mSandboxFramework->GetD3DDevice(), descriptorManager, mSandboxFramework->GetCommandList(), cbDescDXR, L"DXR CB");

	// create a null descriptor for unbound textures
	mNullDescriptor = descriptorManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// DS buffer
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	device->CreateShaderResourceView(nullptr, &srvDesc, mNullDescriptor.GetCPUHandle());
	mDepthStencil = new DXRSDepthBuffer(device, descriptorManager, MAX_SCREEN_WIDTH, MAX_SCREEN_HEIGHT, DXGI_FORMAT_D32_FLOAT);

	#pragma region States
	D3D12_DEPTH_STENCIL_DESC depthStateRW;
	depthStateRW.DepthEnable = TRUE;
	depthStateRW.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStateRW.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	depthStateRW.StencilEnable = FALSE;
	depthStateRW.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	depthStateRW.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	depthStateRW.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	depthStateRW.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStateRW.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStateRW.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStateRW.BackFace = depthStateRW.FrontFace;

	D3D12_DEPTH_STENCIL_DESC depthStateRead = depthStateRW;
	depthStateRead.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

	D3D12_DEPTH_STENCIL_DESC depthStateDisabled;
	depthStateDisabled.DepthEnable = FALSE;
	depthStateDisabled.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depthStateDisabled.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	depthStateDisabled.StencilEnable = FALSE;
	depthStateDisabled.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	depthStateDisabled.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	depthStateDisabled.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	depthStateDisabled.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	depthStateDisabled.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	depthStateDisabled.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	depthStateDisabled.BackFace = depthStateRW.FrontFace;

	D3D12_BLEND_DESC blend = {};
	blend.IndependentBlendEnable = FALSE;
	blend.RenderTarget[0].BlendEnable = FALSE;
	blend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	blend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_BLEND_DESC blendLPVPropagation = {};
	blendLPVPropagation.IndependentBlendEnable = TRUE;
	for (size_t i = 0; i < 6; i++)
	{
		blendLPVPropagation.RenderTarget[i].BlendEnable = (i < 3) ? FALSE : TRUE;
		blendLPVPropagation.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
		blendLPVPropagation.RenderTarget[i].DestBlend = D3D12_BLEND_ONE;
		blendLPVPropagation.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
		blendLPVPropagation.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
		blendLPVPropagation.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ONE;
		blendLPVPropagation.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		blendLPVPropagation.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}

	D3D12_RASTERIZER_DESC rasterizer;
	rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizer.CullMode = D3D12_CULL_MODE_BACK;
	rasterizer.FrontCounterClockwise = FALSE;
	rasterizer.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	rasterizer.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	rasterizer.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rasterizer.DepthClipEnable = TRUE;
	rasterizer.MultisampleEnable = FALSE;
	rasterizer.AntialiasedLineEnable = FALSE;
	rasterizer.ForcedSampleCount = 0;
	rasterizer.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	D3D12_RASTERIZER_DESC shadowrasterizer;
	shadowrasterizer.FillMode = D3D12_FILL_MODE_SOLID;
	shadowrasterizer.CullMode = D3D12_CULL_MODE_BACK;
	shadowrasterizer.FrontCounterClockwise = FALSE;
	shadowrasterizer.SlopeScaledDepthBias = 10.0f;
	shadowrasterizer.DepthBias = 0.05f;
	shadowrasterizer.DepthClipEnable = FALSE;
	shadowrasterizer.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	shadowrasterizer.MultisampleEnable = FALSE;
	shadowrasterizer.AntialiasedLineEnable = FALSE;
	shadowrasterizer.ForcedSampleCount = 0;
	shadowrasterizer.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	D3D12_SAMPLER_DESC bilinearSampler;
	bilinearSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	bilinearSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	bilinearSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	bilinearSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	//bilinearSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_EQUAL;
	bilinearSampler.MipLODBias = 0;
	bilinearSampler.MaxAnisotropy = 16;
	bilinearSampler.MinLOD = 0.0f;
	bilinearSampler.MaxLOD = D3D12_FLOAT32_MAX;
	bilinearSampler.BorderColor[0] = 0.0f;
	bilinearSampler.BorderColor[1] = 0.0f;
	bilinearSampler.BorderColor[2] = 0.0f;
	bilinearSampler.BorderColor[3] = 0.0f;

#pragma endregion

	// create resources for g-buffer pass 
	{
		D3D12_SAMPLER_DESC sampler;
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.MipLODBias = 0;
		sampler.MaxAnisotropy = 0;
		sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		sampler.MinLOD = 0.0f;
		sampler.MaxLOD = D3D12_FLOAT32_MAX;

		DXGI_FORMAT rtFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		mGbufferRTs.push_back(new DXRSRenderTarget(device, descriptorManager, MAX_SCREEN_WIDTH, MAX_SCREEN_HEIGHT, rtFormat, flags, L"Albedo"));

		rtFormat = DXGI_FORMAT_R16G16B16A16_SNORM;
		mGbufferRTs.push_back(new DXRSRenderTarget(device, descriptorManager, MAX_SCREEN_WIDTH, MAX_SCREEN_HEIGHT, rtFormat, flags, L"Normals"));
		
		rtFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
		mGbufferRTs.push_back(new DXRSRenderTarget(device, descriptorManager, MAX_SCREEN_WIDTH, MAX_SCREEN_HEIGHT, rtFormat, flags, L"World Positions"));

		// root signature
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		mGbufferRS.Reset(2, 1);
		mGbufferRS.InitStaticSampler(0, sampler, D3D12_SHADER_VISIBILITY_PIXEL);
		mGbufferRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 2, D3D12_SHADER_VISIBILITY_ALL);
		mGbufferRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 2, D3D12_SHADER_VISIBILITY_PIXEL);
		mGbufferRS.Finalize(device, L"GPrepassRS", rootSignatureFlags);

		//Create Pipeline State Object
		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

		ID3DBlob* errorBlob = nullptr;

		ThrowIfFailed(D3DCompileFromFile(mSandboxFramework->GetFilePath(L"content\\shaders\\GBuffer.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_1", compileFlags, 0, &vertexShader, nullptr));

		compileFlags |= D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES;

		ThrowIfFailed(D3DCompileFromFile(mSandboxFramework->GetFilePath(L"content\\shaders\\GBuffer.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_1", compileFlags, 0, &pixelShader, &errorBlob));

		// Define the vertex input layout.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		// Describe and create the graphics pipeline state object (PSO).
		DXGI_FORMAT formats[3];
		formats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		formats[1] = DXGI_FORMAT_R16G16B16A16_SNORM;
		formats[2] = DXGI_FORMAT_R32G32B32A32_FLOAT;

		mGbufferPSO.SetRootSignature(mGbufferRS);
		mGbufferPSO.SetRasterizerState(rasterizer);
		mGbufferPSO.SetBlendState(blend);
		mGbufferPSO.SetDepthStencilState(depthStateRW);
		mGbufferPSO.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
		mGbufferPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		mGbufferPSO.SetRenderTargetFormats(_countof(formats), formats, DXGI_FORMAT_D32_FLOAT);
		mGbufferPSO.SetVertexShader(vertexShader->GetBufferPointer(), vertexShader->GetBufferSize());
		mGbufferPSO.SetPixelShader(pixelShader->GetBufferPointer(), pixelShader->GetBufferSize());
		mGbufferPSO.Finalize(device);

		//create constant buffer for pass
		DXRSBuffer::Description cbDesc;
		cbDesc.mElementSize = sizeof(GBufferCBData);
		cbDesc.mState = D3D12_RESOURCE_STATE_GENERIC_READ;
		cbDesc.mDescriptorType = DXRSBuffer::DescriptorType::CBV;

		mGbufferCB = new DXRSBuffer(mSandboxFramework->GetD3DDevice(), descriptorManager, mSandboxFramework->GetCommandList(), cbDesc, L"GBuffer CB");
	}

	// create resources for RSM
	{
		//generation
		{

			DXGI_FORMAT rtFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
			D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			mRSMBuffersRTs.push_back(new DXRSRenderTarget(device, descriptorManager, RSM_SIZE, RSM_SIZE, rtFormat, flags, L"RSM World Pos"));
			mRSMBuffersRTs.push_back(new DXRSRenderTarget(device, descriptorManager, RSM_SIZE, RSM_SIZE, rtFormat, flags, L"RSM Normals"));
			mRSMBuffersRTs.push_back(new DXRSRenderTarget(device, descriptorManager, RSM_SIZE, RSM_SIZE, rtFormat, flags, L"RSM Flux"));

			// root signature
			D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
				D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

			mRSMBuffersRS.Reset(1, 0);
			mRSMBuffersRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 2, D3D12_SHADER_VISIBILITY_ALL);
			mRSMBuffersRS.Finalize(device, L"RSM Buffers RS", rootSignatureFlags);

			//Create Pipeline State Object
			ComPtr<ID3DBlob> vertexShader;
			ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
			// Enable better shader debugging with the graphics debugging tools.
			UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
			UINT compileFlags = 0;
#endif

			ID3DBlob* errorBlob = nullptr;

			ThrowIfFailed(D3DCompileFromFile(mSandboxFramework->GetFilePath(L"content\\shaders\\ShadowMapping.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_1", compileFlags, 0, &vertexShader, nullptr));

			compileFlags |= D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES;

			ThrowIfFailed(D3DCompileFromFile(mSandboxFramework->GetFilePath(L"content\\shaders\\ShadowMapping.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSRSM", "ps_5_1", compileFlags, 0, &pixelShader, &errorBlob));

			// Define the vertex input layout.
			D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			};

			// Describe and create the graphics pipeline state object (PSO).
			DXGI_FORMAT formats[3];
			formats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
			formats[1] = DXGI_FORMAT_R32G32B32A32_FLOAT;
			formats[2] = DXGI_FORMAT_R32G32B32A32_FLOAT;

			mRSMBuffersPSO.SetRootSignature(mRSMBuffersRS);
			mRSMBuffersPSO.SetRasterizerState(rasterizer);
			mRSMBuffersPSO.SetBlendState(blend);
			mRSMBuffersPSO.SetDepthStencilState(depthStateRead);
			mRSMBuffersPSO.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
			mRSMBuffersPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
			mRSMBuffersPSO.SetRenderTargetFormats(_countof(formats), formats, DXGI_FORMAT_D32_FLOAT);
			mRSMBuffersPSO.SetVertexShader(vertexShader->GetBufferPointer(), vertexShader->GetBufferSize());
			mRSMBuffersPSO.SetPixelShader(pixelShader->GetBufferPointer(), pixelShader->GetBufferSize());
			mRSMBuffersPSO.Finalize(device);
		}

		//calculation
		{
			//RTs
			mRSMRT = new DXRSRenderTarget(device, descriptorManager, MAX_SCREEN_WIDTH * mRSMRTRatio, MAX_SCREEN_HEIGHT * mRSMRTRatio,
				DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"RSM Indirect Illumination");

			//create root signature
			D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
				D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

			D3D12_SAMPLER_DESC rsmSampler;
			rsmSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			rsmSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			rsmSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			rsmSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			//rsmSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			rsmSampler.MipLODBias = 0;
			rsmSampler.MaxAnisotropy = 16;
			rsmSampler.MinLOD = 0.0f;
			rsmSampler.MaxLOD = D3D12_FLOAT32_MAX;
			rsmSampler.BorderColor[0] = 0.0f;
			rsmSampler.BorderColor[1] = 0.0f;
			rsmSampler.BorderColor[2] = 0.0f;
			rsmSampler.BorderColor[3] = 0.0f;

			// pixel version
			{
				mRSMRS.Reset(2, 1);
				mRSMRS.InitStaticSampler(0, rsmSampler, D3D12_SHADER_VISIBILITY_PIXEL);
				mRSMRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 2, D3D12_SHADER_VISIBILITY_ALL);
				mRSMRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 5, D3D12_SHADER_VISIBILITY_PIXEL);
				mRSMRS.Finalize(device, L"RSM pixel shader pass RS", rootSignatureFlags);

				//PSO
				D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
				{
					{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
					{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
				};

				ComPtr<ID3DBlob> vertexShader;
				ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
				// Enable better shader debugging with the graphics debugging tools.
				UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
				UINT compileFlags = 0;
#endif
				ID3DBlob* errorBlob = nullptr;

				ThrowIfFailed(D3DCompileFromFile(mSandboxFramework->GetFilePath(L"content\\shaders\\ReflectiveShadowMappingPS.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &errorBlob));
				if (errorBlob)
				{
					OutputDebugStringA((char*)errorBlob->GetBufferPointer());
					errorBlob->Release();
				}

				ThrowIfFailed(D3DCompileFromFile(mSandboxFramework->GetFilePath(L"content\\shaders\\ReflectiveShadowMappingPS.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &errorBlob));
				if (errorBlob)
				{
					OutputDebugStringA((char*)errorBlob->GetBufferPointer());
					errorBlob->Release();
				}

				DXGI_FORMAT m_rtFormats[1];
				m_rtFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

				mRSMPSO.SetRootSignature(mRSMRS);
				mRSMPSO.SetRasterizerState(rasterizer);
				mRSMPSO.SetBlendState(blend);
				mRSMPSO.SetDepthStencilState(depthStateDisabled);
				mRSMPSO.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
				mRSMPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
				mRSMPSO.SetRenderTargetFormats(_countof(m_rtFormats), m_rtFormats, DXGI_FORMAT_D32_FLOAT);
				mRSMPSO.SetVertexShader(vertexShader->GetBufferPointer(), vertexShader->GetBufferSize());
				mRSMPSO.SetPixelShader(pixelShader->GetBufferPointer(), pixelShader->GetBufferSize());
				mRSMPSO.Finalize(device);
			}

			// compute version
			{
				mRSMRS_Compute.Reset(3, 1);
				mRSMRS_Compute.InitStaticSampler(0, rsmSampler, D3D12_SHADER_VISIBILITY_ALL);
				mRSMRS_Compute[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 2, D3D12_SHADER_VISIBILITY_ALL);
				mRSMRS_Compute[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 5, D3D12_SHADER_VISIBILITY_ALL);
				mRSMRS_Compute[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
				mRSMRS_Compute.Finalize(device, L"RSM compute shader pass RS", rootSignatureFlags);

				ComPtr<ID3DBlob> computeShader;

#if defined(_DEBUG)
				// Enable better shader debugging with the graphics debugging tools.
				UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
				UINT compileFlags = 0;
#endif
				ID3DBlob* errorBlob = nullptr;

				ThrowIfFailed(D3DCompileFromFile(mSandboxFramework->GetFilePath(L"content\\shaders\\ReflectiveShadowMappingCS.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "CSMain", "cs_5_0", compileFlags, 0, &computeShader, &errorBlob));
				if (errorBlob)
				{
					OutputDebugStringA((char*)errorBlob->GetBufferPointer());
					errorBlob->Release();
				}

				mRSMPSO_Compute.SetRootSignature(mRSMRS_Compute);
				mRSMPSO_Compute.SetComputeShader(computeShader->GetBufferPointer(), computeShader->GetBufferSize());
				mRSMPSO_Compute.Finalize(device);
			}

			//CB
			DXRSBuffer::Description cbDesc;
			cbDesc.mElementSize = sizeof(RSMCBData);
			cbDesc.mState = D3D12_RESOURCE_STATE_GENERIC_READ;
			cbDesc.mDescriptorType = DXRSBuffer::DescriptorType::CBV;

			mRSMCB = new DXRSBuffer(device, descriptorManager, mSandboxFramework->GetCommandList(), cbDesc, L"RSM Pass CB");

			cbDesc.mElementSize = sizeof(RSMCBDataRandomValues);
			mRSMCB2 = new DXRSBuffer(device, descriptorManager, mSandboxFramework->GetCommandList(), cbDesc, L"RSM Pass CB 2");

			RSMCBDataRandomValues rsmPassData2 = {};
			for (int i = 0; i < RSM_SAMPLES_COUNT; i++)
			{
				rsmPassData2.xi[i].x = RandomFloat(0.0f, 1.0f);
				rsmPassData2.xi[i].y = RandomFloat(0.0f, 1.0f);
				rsmPassData2.xi[i].z = 0.0f;
				rsmPassData2.xi[i].w = 0.0f;
			}
			memcpy(mRSMCB2->Map(), &rsmPassData2, sizeof(rsmPassData2));
		}

		//blur
		{
			//RTs
			mRSMUpsampleAndBlurRT = new DXRSRenderTarget(device, descriptorManager, MAX_SCREEN_WIDTH, MAX_SCREEN_HEIGHT,
				DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"RSM Upsampled & Blurred");

			//create root signature
			D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
				D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

			mRSMUpsampleAndBlurRS.Reset(2, 1);
			mRSMUpsampleAndBlurRS.InitStaticSampler(0, bilinearSampler, D3D12_SHADER_VISIBILITY_ALL);
			mRSMUpsampleAndBlurRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
			mRSMUpsampleAndBlurRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
			mRSMUpsampleAndBlurRS.Finalize(device, L"RSM Blur pass RS", rootSignatureFlags);

			ComPtr<ID3DBlob> computeShader;

#if defined(_DEBUG)
			// Enable better shader debugging with the graphics debugging tools.
			UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
			UINT compileFlags = 0;
#endif
			ID3DBlob* errorBlob = nullptr;

			ThrowIfFailed(D3DCompileFromFile(mSandboxFramework->GetFilePath(L"content\\shaders\\UpsampleBlurCS.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "CSMain", "cs_5_0", compileFlags, 0, &computeShader, &errorBlob));
			if (errorBlob)
			{
				OutputDebugStringA((char*)errorBlob->GetBufferPointer());
				errorBlob->Release();
			}

			mRSMUpsampleAndBlurPSO.SetRootSignature(mRSMUpsampleAndBlurRS);
			mRSMUpsampleAndBlurPSO.SetComputeShader(computeShader->GetBufferPointer(), computeShader->GetBufferSize());
			mRSMUpsampleAndBlurPSO.Finalize(device);
		}
	}

	// create resources for shadow mapping
	{
		mShadowDepth = new DXRSDepthBuffer(device, descriptorManager, SHADOWMAP_SIZE, SHADOWMAP_SIZE, DXGI_FORMAT_D32_FLOAT);

		//create root signature
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		mShadowMappingRS.Reset(1, 0);
		mShadowMappingRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 2, D3D12_SHADER_VISIBILITY_VERTEX);
		mShadowMappingRS.Finalize(device, L"Shadow Mapping pass RS", rootSignatureFlags);

		ComPtr<ID3DBlob> vertexShader;
		//ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

		ID3DBlob* errorBlob = nullptr;

		HRESULT res = D3DCompileFromFile(mSandboxFramework->GetFilePath(L"content\\shaders\\ShadowMapping.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSOnlyMain", "vs_5_1", compileFlags, 0, &vertexShader, &errorBlob);

		//if (errorBlob) {
		//	std::string resultMessasge;
		//	resultMessasge.append((char*)errorBlob->GetBufferPointer());
		//}

		ThrowIfFailed(res);

		// Define the vertex input layout.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		mShadowMappingPSO.SetRootSignature(mShadowMappingRS);
		mShadowMappingPSO.SetRasterizerState(shadowrasterizer);
		mShadowMappingPSO.SetRenderTargetFormats(0, nullptr, mShadowDepth->GetFormat());
		mShadowMappingPSO.SetBlendState(blend);
		mShadowMappingPSO.SetDepthStencilState(depthStateRW);
		mShadowMappingPSO.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
		mShadowMappingPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		mShadowMappingPSO.SetVertexShader(vertexShader->GetBufferPointer(), vertexShader->GetBufferSize());
		//mShadowMappingPSO.SetPixelShader(pixelShader->GetBufferPointer(), pixelShader->GetBufferSize());
		mShadowMappingPSO.Finalize(device);

		//create constant buffer for pass
		DXRSBuffer::Description cbDesc;
		cbDesc.mElementSize = sizeof(ShadowMappingCBData);
		cbDesc.mState = D3D12_RESOURCE_STATE_GENERIC_READ;
		cbDesc.mDescriptorType = DXRSBuffer::DescriptorType::CBV;

		mShadowMappingCB = new DXRSBuffer(mSandboxFramework->GetD3DDevice(), descriptorManager, mSandboxFramework->GetCommandList(), cbDesc, L"Shadow Mapping CB");
	}
	
	// create lpv
	{
		// injection
		{
			DXGI_FORMAT format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			//red
			mLPVSHColorsRTs.push_back(new DXRSRenderTarget(device, descriptorManager, LPV_SIZE, LPV_SIZE, format, flags, L"Red SH LPV", LPV_SIZE));
			mLPVSHColorsRTs.push_back(new DXRSRenderTarget(device, descriptorManager, LPV_SIZE, LPV_SIZE, format, flags, L"Green SH LPV", LPV_SIZE));
			mLPVSHColorsRTs.push_back(new DXRSRenderTarget(device, descriptorManager, LPV_SIZE, LPV_SIZE, format, flags, L"Blue SH LPV", LPV_SIZE));

			//create root signature
			D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
				D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;

			mLPVInjectionRS.Reset(2, 0);
			mLPVInjectionRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
			mLPVInjectionRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 3, D3D12_SHADER_VISIBILITY_ALL);
			mLPVInjectionRS.Finalize(device, L"LPV Injection pass RS", rootSignatureFlags);

			ComPtr<ID3DBlob> vertexShader;
			ComPtr<ID3DBlob> geometryShader;
			ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
			// Enable better shader debugging with the graphics debugging tools.
			UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
			UINT compileFlags = 0;
#endif

			ID3DBlob* errorBlob = nullptr;

			ThrowIfFailed(D3DCompileFromFile(mSandboxFramework->GetFilePath(L"content\\shaders\\LPVInjection.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_1", compileFlags, 0, &vertexShader, &errorBlob));
			ThrowIfFailed(D3DCompileFromFile(mSandboxFramework->GetFilePath(L"content\\shaders\\LPVInjection.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "GSMain", "gs_5_1", compileFlags, 0, &geometryShader, &errorBlob));
			ThrowIfFailed(D3DCompileFromFile(mSandboxFramework->GetFilePath(L"content\\shaders\\LPVInjection.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_1", compileFlags, 0, &pixelShader, &errorBlob));

			DXGI_FORMAT formats[3];
			formats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
			formats[1] = DXGI_FORMAT_R32G32B32A32_FLOAT;
			formats[2] = DXGI_FORMAT_R32G32B32A32_FLOAT;

			mLPVInjectionPSO.SetRootSignature(mLPVInjectionRS);
			mLPVInjectionPSO.SetRasterizerState(rasterizer);
			mLPVInjectionPSO.SetRenderTargetFormats(_countof(formats), formats, DXGI_FORMAT_D32_FLOAT);
			mLPVInjectionPSO.SetBlendState(blend);
			mLPVInjectionPSO.SetDepthStencilState(depthStateDisabled);
			mLPVInjectionPSO.SetInputLayout(0, nullptr);
			mLPVInjectionPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT);
			mLPVInjectionPSO.SetVertexShader(vertexShader->GetBufferPointer(), vertexShader->GetBufferSize());
			mLPVInjectionPSO.SetGeometryShader(geometryShader->GetBufferPointer(), geometryShader->GetBufferSize());
			mLPVInjectionPSO.SetPixelShader(pixelShader->GetBufferPointer(), pixelShader->GetBufferSize());
			mLPVInjectionPSO.Finalize(device);

			//CB
			DXRSBuffer::Description cbDesc;
			cbDesc.mElementSize = sizeof(LPVCBData);
			cbDesc.mState = D3D12_RESOURCE_STATE_GENERIC_READ;
			cbDesc.mDescriptorType = DXRSBuffer::DescriptorType::CBV;

			mLPVCB = new DXRSBuffer(device, descriptorManager, mSandboxFramework->GetCommandList(), cbDesc, L"LPV Injection Pass CB");

			LPVCBData data = {};

			XMFLOAT3 lpv_max = { LPV_SIZE / 2,LPV_SIZE / 2,LPV_SIZE / 2 };
			XMFLOAT3 lpv_min = { -LPV_SIZE / 2,-LPV_SIZE / 2,-LPV_SIZE / 2 };
			XMVECTOR diag = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&lpv_max), DirectX::XMLoadFloat3(&lpv_min));

			XMFLOAT3 d;
			XMStoreFloat3(&d, diag);
			XMMATRIX scale = DirectX::XMMatrixScaling(1.f / d.x, 1.f / d.y, 1.f / d.z);
			XMMATRIX trans = DirectX::XMMatrixTranslation(-lpv_min.x, -lpv_min.y, -lpv_min.z);

			data.worldToLPV = trans * scale;
			mWorldToLPV = data.worldToLPV;
			memcpy(mLPVCB->Map(), &data, sizeof(data));
		}

		// propagation
		{
			DXGI_FORMAT format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			//red
			mLPVAccumulationSHColorsRTs.push_back(new DXRSRenderTarget(device, descriptorManager, LPV_SIZE, LPV_SIZE, format, flags, L"Accumulation Red SH LPV", LPV_SIZE));
			mLPVAccumulationSHColorsRTs.push_back(new DXRSRenderTarget(device, descriptorManager, LPV_SIZE, LPV_SIZE, format, flags, L"Accumulation Green SH LPV", LPV_SIZE));
			mLPVAccumulationSHColorsRTs.push_back(new DXRSRenderTarget(device, descriptorManager, LPV_SIZE, LPV_SIZE, format, flags, L"Accumulation Blue SH LPV", LPV_SIZE));

			//create root signature
			D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
				D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;

			mLPVPropagationRS.Reset(1, 0);
			mLPVPropagationRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 3, D3D12_SHADER_VISIBILITY_ALL);
			mLPVPropagationRS.Finalize(device, L"LPV Propagation pass RS", rootSignatureFlags);

			ComPtr<ID3DBlob> vertexShader;
			ComPtr<ID3DBlob> geometryShader;
			ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
			// Enable better shader debugging with the graphics debugging tools.
			UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
			UINT compileFlags = 0;
#endif

			ID3DBlob* errorBlob = nullptr;

			ThrowIfFailed(D3DCompileFromFile(mSandboxFramework->GetFilePath(L"content\\shaders\\LPVPropagation.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_1", compileFlags, 0, &vertexShader, &errorBlob));
			ThrowIfFailed(D3DCompileFromFile(mSandboxFramework->GetFilePath(L"content\\shaders\\LPVPropagation.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "GSMain", "gs_5_1", compileFlags, 0, &geometryShader, &errorBlob));
			ThrowIfFailed(D3DCompileFromFile(mSandboxFramework->GetFilePath(L"content\\shaders\\LPVPropagation.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_1", compileFlags, 0, &pixelShader, &errorBlob));

			DXGI_FORMAT formats[6];
			formats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
			formats[1] = DXGI_FORMAT_R32G32B32A32_FLOAT;
			formats[2] = DXGI_FORMAT_R32G32B32A32_FLOAT;
			formats[3] = DXGI_FORMAT_R32G32B32A32_FLOAT;
			formats[4] = DXGI_FORMAT_R32G32B32A32_FLOAT;
			formats[5] = DXGI_FORMAT_R32G32B32A32_FLOAT;

			mLPVPropagationPSO.SetRootSignature(mLPVPropagationRS);
			mLPVPropagationPSO.SetRasterizerState(rasterizer);
			mLPVPropagationPSO.SetRenderTargetFormats(_countof(formats), formats, DXGI_FORMAT_D32_FLOAT);
			mLPVPropagationPSO.SetBlendState(blendLPVPropagation); //TODO add additive blending - fix 
			mLPVPropagationPSO.SetDepthStencilState(depthStateDisabled);
			mLPVPropagationPSO.SetInputLayout(0, nullptr);
			mLPVPropagationPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
			mLPVPropagationPSO.SetVertexShader(vertexShader->GetBufferPointer(), vertexShader->GetBufferSize());
			mLPVPropagationPSO.SetGeometryShader(geometryShader->GetBufferPointer(), geometryShader->GetBufferSize());
			mLPVPropagationPSO.SetPixelShader(pixelShader->GetBufferPointer(), pixelShader->GetBufferSize());
			mLPVPropagationPSO.Finalize(device);
		}
	}

	// create resources lighting pass
	{
		//RTs
		mLightingRTs.push_back(new DXRSRenderTarget(device, descriptorManager, MAX_SCREEN_WIDTH, MAX_SCREEN_HEIGHT,
			DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"Lighting"));

		//create root signature
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
		
		D3D12_SAMPLER_DESC shadowSampler;
		shadowSampler.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		shadowSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		shadowSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		shadowSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		shadowSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		shadowSampler.MipLODBias = 0;
		shadowSampler.MaxAnisotropy = 16;
		shadowSampler.MinLOD = 0.0f;
		shadowSampler.MaxLOD = D3D12_FLOAT32_MAX;
		shadowSampler.BorderColor[0] = 1.0f;
		shadowSampler.BorderColor[1] = 1.0f;
		shadowSampler.BorderColor[2] = 1.0f;
		shadowSampler.BorderColor[3] = 1.0f; 

		D3D12_SAMPLER_DESC lpvSampler;
		lpvSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		lpvSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		lpvSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		lpvSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		//lpvSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		lpvSampler.MipLODBias = 0;
		lpvSampler.MaxAnisotropy = 16;
		lpvSampler.MinLOD = 0.0f;
		lpvSampler.MaxLOD = D3D12_FLOAT32_MAX;
		lpvSampler.BorderColor[0] = 0.0f;
		lpvSampler.BorderColor[1] = 0.0f;
		lpvSampler.BorderColor[2] = 0.0f;
		lpvSampler.BorderColor[3] = 0.0f;

		mLightingRS.Reset(2, 3);
		mLightingRS.InitStaticSampler(0, bilinearSampler, D3D12_SHADER_VISIBILITY_PIXEL);
		mLightingRS.InitStaticSampler(1, shadowSampler, D3D12_SHADER_VISIBILITY_PIXEL);
		mLightingRS.InitStaticSampler(2, lpvSampler, D3D12_SHADER_VISIBILITY_PIXEL);
		mLightingRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 3, D3D12_SHADER_VISIBILITY_ALL);
		mLightingRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 9, D3D12_SHADER_VISIBILITY_PIXEL);
		mLightingRS.Finalize(device, L"Lighting pass RS", rootSignatureFlags);

		//PSO
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif
		ID3DBlob* errorBlob = nullptr;

		ThrowIfFailed(D3DCompileFromFile(mSandboxFramework->GetFilePath(L"content\\shaders\\Lighting.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &errorBlob));
		if (errorBlob)
		{
			OutputDebugStringA((char*)errorBlob->GetBufferPointer());
			errorBlob->Release();
		}

		ThrowIfFailed(D3DCompileFromFile(mSandboxFramework->GetFilePath(L"content\\shaders\\Lighting.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &errorBlob));
		if (errorBlob)
		{
			OutputDebugStringA((char*)errorBlob->GetBufferPointer());
			errorBlob->Release();
		}

		DXGI_FORMAT m_rtFormats[1];
		m_rtFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

		mLightingPSO.SetRootSignature(mLightingRS);
		mLightingPSO.SetRasterizerState(rasterizer);
		mLightingPSO.SetBlendState(blend);
		mLightingPSO.SetDepthStencilState(depthStateDisabled);
		mLightingPSO.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
		mLightingPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		mLightingPSO.SetRenderTargetFormats(_countof(m_rtFormats), m_rtFormats, DXGI_FORMAT_D32_FLOAT);
		mLightingPSO.SetVertexShader(vertexShader->GetBufferPointer(), vertexShader->GetBufferSize());
		mLightingPSO.SetPixelShader(pixelShader->GetBufferPointer(), pixelShader->GetBufferSize());
		mLightingPSO.Finalize(device);

		//CB
		DXRSBuffer::Description cbDesc;
		cbDesc.mElementSize = sizeof(LightingCBData);
		cbDesc.mState = D3D12_RESOURCE_STATE_GENERIC_READ;
		cbDesc.mDescriptorType = DXRSBuffer::DescriptorType::CBV;

		mLightingCB = new DXRSBuffer(device, descriptorManager, mSandboxFramework->GetCommandList(), cbDesc, L"Lighting Pass CB");

		cbDesc.mElementSize = sizeof(LightsInfoCBData);
		mLightsInfoCB = new DXRSBuffer(device, descriptorManager, mSandboxFramework->GetCommandList(), cbDesc, L"Lights Info CB");
	}

	// create resources composite pass
	{
		//create root signature
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		mCompositeRS.Reset(1, 0);
		//mCompositeRS.InitStaticSampler(0, SamplerLinearClampDesc, D3D12_SHADER_VISIBILITY_PIXEL);
		mCompositeRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
		mCompositeRS.Finalize(device, L"Composite RS", rootSignatureFlags);

		//create pipeline state object
		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif
		ID3DBlob* errorBlob = nullptr;

		ThrowIfFailed(D3DCompileFromFile(mSandboxFramework->GetFilePath(L"content\\shaders\\Composite.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &errorBlob));
		if (errorBlob)
		{
			OutputDebugStringA((char*)errorBlob->GetBufferPointer());
			errorBlob->Release();
		}

		ThrowIfFailed(D3DCompileFromFile(mSandboxFramework->GetFilePath(L"content\\shaders\\Composite.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &errorBlob));

		mCompositePSO = mLightingPSO;

		mCompositePSO.SetRootSignature(mCompositeRS);
		mCompositePSO.SetRenderTargetFormat(mSandboxFramework->GetBackBufferFormat(), DXGI_FORMAT_D32_FLOAT);
		mCompositePSO.SetVertexShader(vertexShader->GetBufferPointer(), vertexShader->GetBufferSize());
		mCompositePSO.SetPixelShader(pixelShader->GetBufferPointer(), pixelShader->GetBufferSize());
		mCompositePSO.Finalize(device);
	}
}

void DXRSExampleGIScene::Clear()
{
	auto commandList = mSandboxFramework->GetCommandList();

	auto rtvDescriptor = mSandboxFramework->GetRenderTargetView();
	auto dsvDescriptor = mSandboxFramework->GetDepthStencilView();

	commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
	commandList->ClearRenderTargetView(rtvDescriptor, Colors::Green, 0, nullptr);
	commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	auto viewport = mSandboxFramework->GetScreenViewport();
	auto scissorRect = mSandboxFramework->GetScissorRect();
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);
}

void DXRSExampleGIScene::Run()
{
	mTimer.Run([&]()
	{
		Update(mTimer);
	});

	Render();
}

void DXRSExampleGIScene::Render()
{
	const float clearColorBlack[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	const float clearColorWhite[] = { 1.0f,1.0f,1.0f,1.0f };
	
	if (mTimer.GetFrameCount() == 0)
		return;

	// Prepare the command list to render a new frame.
	mSandboxFramework->Prepare();
	Clear();

	auto commandList = mSandboxFramework->GetCommandList();
	auto device = mSandboxFramework->GetD3DDevice();
	auto descriptorHeapManager = mSandboxFramework->GetDescriptorHeapManager();

	DXRS::GPUDescriptorHeap* gpuDescriptorHeap = descriptorHeapManager->GetGPUHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	gpuDescriptorHeap->Reset();

	ID3D12DescriptorHeap* ppHeaps[] = { gpuDescriptorHeap->GetHeap() };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	CD3DX12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, mSandboxFramework->GetOutputSize().right, mSandboxFramework->GetOutputSize().bottom);
	CD3DX12_RECT rect = CD3DX12_RECT(0.0f, 0.0f, mSandboxFramework->GetOutputSize().right, mSandboxFramework->GetOutputSize().bottom);
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &rect);

	//gbuffer
	PIXBeginEvent(commandList, 0, "GBuffer");
	{
		commandList->SetPipelineState(mGbufferPSO.GetPipelineStateObject());
		commandList->SetGraphicsRootSignature(mGbufferRS.GetSignature());

		//transition buffers to rendertarget outputs
		mSandboxFramework->ResourceBarriersBegin(mBarriers);
		mGbufferRTs[0]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mGbufferRTs[1]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mGbufferRTs[2]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mLightingRTs[0]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET); // ???? why here
		mSandboxFramework->ResourceBarriersEnd(mBarriers, commandList);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[] =
		{
			mGbufferRTs[0]->GetRTV().GetCPUHandle(),
			mGbufferRTs[1]->GetRTV().GetCPUHandle(),
			mGbufferRTs[2]->GetRTV().GetCPUHandle()
		};

		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(mDepthStencil->GetDSV().GetCPUHandle());
		commandList->OMSetRenderTargets(_countof(rtvHandles), rtvHandles, FALSE, &mSandboxFramework->GetDepthStencilView());
		commandList->ClearRenderTargetView(rtvHandles[0], clearColorBlack, 0, nullptr);
		commandList->ClearRenderTargetView(rtvHandles[1], clearColorBlack, 0, nullptr);
		commandList->ClearRenderTargetView(rtvHandles[2], clearColorBlack, 0, nullptr);

		DXRS::DescriptorHandle cbvHandle;
		DXRS::DescriptorHandle srvHandle;

		srvHandle = gpuDescriptorHeap->GetHandleBlock(0);

		// no textures
		gpuDescriptorHeap->AddToHandle(device, srvHandle, mNullDescriptor);

		//room
		{
			cbvHandle = gpuDescriptorHeap->GetHandleBlock(2);
			gpuDescriptorHeap->AddToHandle(device, cbvHandle, mGbufferCB->GetCBV());
			gpuDescriptorHeap->AddToHandle(device, cbvHandle, mRoomModel->GetCB()->GetCBV());

			commandList->SetGraphicsRootDescriptorTable(0, cbvHandle.GetGPUHandle());
			commandList->SetGraphicsRootDescriptorTable(1, srvHandle.GetGPUHandle());

			mRoomModel->Render(commandList);
		}
		//dragon
		{
			cbvHandle = gpuDescriptorHeap->GetHandleBlock(2);
			gpuDescriptorHeap->AddToHandle(device, cbvHandle, mGbufferCB->GetCBV());
			gpuDescriptorHeap->AddToHandle(device, cbvHandle, mDragonModel->GetCB()->GetCBV());

			commandList->SetGraphicsRootDescriptorTable(0, cbvHandle.GetGPUHandle());
			commandList->SetGraphicsRootDescriptorTable(1, srvHandle.GetGPUHandle());

			mDragonModel->Render(commandList);
		}	
		//sphere_1
		{
			cbvHandle = gpuDescriptorHeap->GetHandleBlock(2);
			gpuDescriptorHeap->AddToHandle(device, cbvHandle, mGbufferCB->GetCBV());
			gpuDescriptorHeap->AddToHandle(device, cbvHandle, mSphereModel_1->GetCB()->GetCBV());

			commandList->SetGraphicsRootDescriptorTable(0, cbvHandle.GetGPUHandle());
			commandList->SetGraphicsRootDescriptorTable(1, srvHandle.GetGPUHandle());

			mSphereModel_1->Render(commandList);
		}	
		//sphere_2
		{
			cbvHandle = gpuDescriptorHeap->GetHandleBlock(2);
			gpuDescriptorHeap->AddToHandle(device, cbvHandle, mGbufferCB->GetCBV());
			gpuDescriptorHeap->AddToHandle(device, cbvHandle, mSphereModel_2->GetCB()->GetCBV());

			commandList->SetGraphicsRootDescriptorTable(0, cbvHandle.GetGPUHandle());
			commandList->SetGraphicsRootDescriptorTable(1, srvHandle.GetGPUHandle());

			mSphereModel_2->Render(commandList);
		}	
		//block
		{
			cbvHandle = gpuDescriptorHeap->GetHandleBlock(2);
			gpuDescriptorHeap->AddToHandle(device, cbvHandle, mGbufferCB->GetCBV());
			gpuDescriptorHeap->AddToHandle(device, cbvHandle, mBlockModel->GetCB()->GetCBV());

			commandList->SetGraphicsRootDescriptorTable(0, cbvHandle.GetGPUHandle());
			commandList->SetGraphicsRootDescriptorTable(1, srvHandle.GetGPUHandle());

			mBlockModel->Render(commandList);
		}

	}
	PIXEndEvent(commandList);

	//shadows
	PIXBeginEvent(commandList, 0, "Shadows");
	{
		CD3DX12_VIEWPORT shadowMapViewport = CD3DX12_VIEWPORT(0.0f, 0.0f, mShadowDepth->GetWidth(), mShadowDepth->GetHeight());
		CD3DX12_RECT shadowRect = CD3DX12_RECT(0.0f, 0.0f, mShadowDepth->GetWidth(), mShadowDepth->GetHeight());
		commandList->RSSetViewports(1, &shadowMapViewport);
		commandList->RSSetScissorRects(1, &shadowRect);

		UpdateShadow();

		commandList->SetPipelineState(mShadowMappingPSO.GetPipelineStateObject());
		commandList->SetGraphicsRootSignature(mShadowMappingRS.GetSignature());

		mSandboxFramework->ResourceBarriersBegin(mBarriers);
		mShadowDepth->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		mSandboxFramework->ResourceBarriersEnd(mBarriers, commandList);

		commandList->OMSetRenderTargets(0, nullptr, FALSE, &mShadowDepth->GetDSV().GetCPUHandle());
		commandList->ClearDepthStencilView(mShadowDepth->GetDSV().GetCPUHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		
		DXRS::DescriptorHandle cbvHandle;

		//room
		{
			cbvHandle = gpuDescriptorHeap->GetHandleBlock(2);
			gpuDescriptorHeap->AddToHandle(device, cbvHandle, mShadowMappingCB->GetCBV());
			gpuDescriptorHeap->AddToHandle(device, cbvHandle, mRoomModel->GetCB()->GetCBV());

			commandList->SetGraphicsRootDescriptorTable(0, cbvHandle.GetGPUHandle());
			mRoomModel->Render(commandList);
		}
		//dragon
		{
			cbvHandle = gpuDescriptorHeap->GetHandleBlock(2);
			gpuDescriptorHeap->AddToHandle(device, cbvHandle, mShadowMappingCB->GetCBV());
			gpuDescriptorHeap->AddToHandle(device, cbvHandle, mDragonModel->GetCB()->GetCBV());

			commandList->SetGraphicsRootDescriptorTable(0, cbvHandle.GetGPUHandle());
			mDragonModel->Render(commandList);
		}		
		//sphere_1
		{
			cbvHandle = gpuDescriptorHeap->GetHandleBlock(2);
			gpuDescriptorHeap->AddToHandle(device, cbvHandle, mShadowMappingCB->GetCBV());
			gpuDescriptorHeap->AddToHandle(device, cbvHandle, mSphereModel_1->GetCB()->GetCBV());

			commandList->SetGraphicsRootDescriptorTable(0, cbvHandle.GetGPUHandle());
			mSphereModel_1->Render(commandList);
		}
		//sphere_2
		{
			cbvHandle = gpuDescriptorHeap->GetHandleBlock(2);
			gpuDescriptorHeap->AddToHandle(device, cbvHandle, mShadowMappingCB->GetCBV());
			gpuDescriptorHeap->AddToHandle(device, cbvHandle, mSphereModel_2->GetCB()->GetCBV());

			commandList->SetGraphicsRootDescriptorTable(0, cbvHandle.GetGPUHandle());
			mSphereModel_2->Render(commandList);
		}	
		//block
		{
			cbvHandle = gpuDescriptorHeap->GetHandleBlock(2);
			gpuDescriptorHeap->AddToHandle(device, cbvHandle, mShadowMappingCB->GetCBV());
			gpuDescriptorHeap->AddToHandle(device, cbvHandle, mBlockModel->GetCB()->GetCBV());

			commandList->SetGraphicsRootDescriptorTable(0, cbvHandle.GetGPUHandle());
			mBlockModel->Render(commandList);
		}

		//reset back
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &rect);
	}
	PIXEndEvent(commandList);

	//copy depth-stencil to custom depth
	PIXBeginEvent(commandList, 0, "Copy Depth-Stencil to texture");
	{
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(mSandboxFramework->GetDepthStencil(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COPY_SOURCE);
		commandList->ResourceBarrier(1, &barrier);
		commandList->CopyResource(mDepthStencil->GetResource(), mSandboxFramework->GetDepthStencil());
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(mSandboxFramework->GetDepthStencil(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		commandList->ResourceBarrier(1, &barrier);
	}
	PIXEndEvent(commandList);

	//rsm
	auto clearRSMRT = [this, commandList, clearColorBlack]() {
		commandList->SetPipelineState(mRSMPSO.GetPipelineStateObject());
		commandList->SetGraphicsRootSignature(mRSMRS.GetSignature());

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandlesRSM[] = { mRSMRT->GetRTV().GetCPUHandle() };
		D3D12_CPU_DESCRIPTOR_HANDLE uavHandlesRSM[] = { mRSMUpsampleAndBlurRT->GetUAV().GetCPUHandle() };

		//transition buffers to rendertarget outputs
		mSandboxFramework->ResourceBarriersBegin(mBarriers);
		mRSMRT->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
		mSandboxFramework->ResourceBarriersEnd(mBarriers, commandList);

		commandList->OMSetRenderTargets(_countof(rtvHandlesRSM), rtvHandlesRSM, FALSE, nullptr);
		commandList->ClearRenderTargetView(rtvHandlesRSM[0], clearColorBlack, 0, nullptr);

		//TODO fix null GPU Handle in UAV
		//commandList->ClearUnorderedAccessViewFloat(mRSMUpsampleAndBlurRT->GetUAV().GetGPUHandle(), mRSMUpsampleAndBlurRT->GetUAV().GetCPUHandle(),
		//	mRSMUpsampleAndBlurRT->GetResource(), clearColor, 0, nullptr);
	};

	if (mRSMEnabled) {
		// buffers generation (pos, normals, flux)
		PIXBeginEvent(commandList, 0, "RSM buffers generation");
		{
			CD3DX12_VIEWPORT rsmBuffersViewport = CD3DX12_VIEWPORT(0.0f, 0.0f, mRSMBuffersRTs[0]->GetWidth(), mRSMBuffersRTs[0]->GetHeight());
			CD3DX12_RECT rsmRect = CD3DX12_RECT(0.0f, 0.0f, mRSMBuffersRTs[0]->GetWidth(), mRSMBuffersRTs[0]->GetHeight());
			commandList->RSSetViewports(1, &rsmBuffersViewport);
			commandList->RSSetScissorRects(1, &rsmRect);

			UpdateShadow();

			commandList->SetPipelineState(mRSMBuffersPSO.GetPipelineStateObject());
			commandList->SetGraphicsRootSignature(mRSMBuffersRS.GetSignature());

			mSandboxFramework->ResourceBarriersBegin(mBarriers);
			mRSMBuffersRTs[0]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
			mRSMBuffersRTs[1]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
			mRSMBuffersRTs[2]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
			//mShadowDepth->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_DEPTH_READ);
			mSandboxFramework->ResourceBarriersEnd(mBarriers, commandList);

			D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[] =
			{
				mRSMBuffersRTs[0]->GetRTV().GetCPUHandle(),
				mRSMBuffersRTs[1]->GetRTV().GetCPUHandle(),
				mRSMBuffersRTs[2]->GetRTV().GetCPUHandle()
			};

			commandList->OMSetRenderTargets(_countof(rtvHandles), rtvHandles, FALSE, &mShadowDepth->GetDSV().GetCPUHandle());
			commandList->ClearRenderTargetView(rtvHandles[0], clearColorBlack, 0, nullptr);
			commandList->ClearRenderTargetView(rtvHandles[1], clearColorBlack, 0, nullptr);
			commandList->ClearRenderTargetView(rtvHandles[2], clearColorBlack, 0, nullptr);

			DXRS::DescriptorHandle cbvHandle;

			//room
			{
				cbvHandle = gpuDescriptorHeap->GetHandleBlock(2);
				gpuDescriptorHeap->AddToHandle(device, cbvHandle, mShadowMappingCB->GetCBV());
				gpuDescriptorHeap->AddToHandle(device, cbvHandle, mRoomModel->GetCB()->GetCBV());

				commandList->SetGraphicsRootDescriptorTable(0, cbvHandle.GetGPUHandle());
				mRoomModel->Render(commandList);
			}
			//dragon
			{
				cbvHandle = gpuDescriptorHeap->GetHandleBlock(2);
				gpuDescriptorHeap->AddToHandle(device, cbvHandle, mShadowMappingCB->GetCBV());
				gpuDescriptorHeap->AddToHandle(device, cbvHandle, mDragonModel->GetCB()->GetCBV());

				commandList->SetGraphicsRootDescriptorTable(0, cbvHandle.GetGPUHandle());
				mDragonModel->Render(commandList);
			}
			//sphere_1
			{
				cbvHandle = gpuDescriptorHeap->GetHandleBlock(2);
				gpuDescriptorHeap->AddToHandle(device, cbvHandle, mShadowMappingCB->GetCBV());
				gpuDescriptorHeap->AddToHandle(device, cbvHandle, mSphereModel_1->GetCB()->GetCBV());

				commandList->SetGraphicsRootDescriptorTable(0, cbvHandle.GetGPUHandle());
				mSphereModel_1->Render(commandList);
			}
			//sphere_2
			{
				cbvHandle = gpuDescriptorHeap->GetHandleBlock(2);
				gpuDescriptorHeap->AddToHandle(device, cbvHandle, mShadowMappingCB->GetCBV());
				gpuDescriptorHeap->AddToHandle(device, cbvHandle, mSphereModel_2->GetCB()->GetCBV());

				commandList->SetGraphicsRootDescriptorTable(0, cbvHandle.GetGPUHandle());
				mSphereModel_2->Render(commandList);
			}	
			//block
			{
				cbvHandle = gpuDescriptorHeap->GetHandleBlock(2);
				gpuDescriptorHeap->AddToHandle(device, cbvHandle, mShadowMappingCB->GetCBV());
				gpuDescriptorHeap->AddToHandle(device, cbvHandle, mBlockModel->GetCB()->GetCBV());

				commandList->SetGraphicsRootDescriptorTable(0, cbvHandle.GetGPUHandle());
				mBlockModel->Render(commandList);
			}

			//reset back
			commandList->RSSetViewports(1, &viewport);
			commandList->RSSetScissorRects(1, &rect);
		}
		PIXEndEvent(commandList);

		// calculation
		if (!mLPVEnabled) {
			PIXBeginEvent(commandList, 0, "RSM main calculation");
			{
				if (!mRSMComputeVersion) {
					CD3DX12_VIEWPORT rsmResViewport = CD3DX12_VIEWPORT(0.0f, 0.0f, mRSMRT->GetWidth(), mRSMRT->GetHeight());
					CD3DX12_RECT rsmRect = CD3DX12_RECT(0.0f, 0.0f, mRSMRT->GetWidth(), mRSMRT->GetHeight());
					commandList->RSSetViewports(1, &rsmResViewport);
					commandList->RSSetScissorRects(1, &rsmRect);

					clearRSMRT();

					DXRS::DescriptorHandle cbvHandleRSM = gpuDescriptorHeap->GetHandleBlock(2);
					gpuDescriptorHeap->AddToHandle(device, cbvHandleRSM, mRSMCB->GetCBV());
					gpuDescriptorHeap->AddToHandle(device, cbvHandleRSM, mRSMCB2->GetCBV());

					DXRS::DescriptorHandle srvHandleRSM = gpuDescriptorHeap->GetHandleBlock(5);
					gpuDescriptorHeap->AddToHandle(device, srvHandleRSM, mRSMBuffersRTs[0]->GetSRV());
					gpuDescriptorHeap->AddToHandle(device, srvHandleRSM, mRSMBuffersRTs[1]->GetSRV());
					gpuDescriptorHeap->AddToHandle(device, srvHandleRSM, mRSMBuffersRTs[2]->GetSRV());
					gpuDescriptorHeap->AddToHandle(device, srvHandleRSM, mGbufferRTs[2]->GetSRV());
					gpuDescriptorHeap->AddToHandle(device, srvHandleRSM, mGbufferRTs[1]->GetSRV());

					commandList->SetGraphicsRootDescriptorTable(0, cbvHandleRSM.GetGPUHandle());
					commandList->SetGraphicsRootDescriptorTable(1, srvHandleRSM.GetGPUHandle());

					commandList->IASetVertexBuffers(0, 1, &mSandboxFramework->GetFullscreenQuadBufferView());
					commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
					commandList->DrawInstanced(4, 1, 0, 0);

					//reset back
					commandList->RSSetViewports(1, &viewport);
					commandList->RSSetScissorRects(1, &rect);
				}
				else {
					commandList->SetPipelineState(mRSMPSO_Compute.GetPipelineStateObject());
					commandList->SetComputeRootSignature(mRSMRS_Compute.GetSignature());

					//mSandboxFramework->ResourceBarriersBegin(mBarriers);
					//mRSMRT->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
					//mSandboxFramework->ResourceBarriersEnd(mBarriers, commandList);

					DXRS::DescriptorHandle cbvHandleRSM = gpuDescriptorHeap->GetHandleBlock(2);
					gpuDescriptorHeap->AddToHandle(device, cbvHandleRSM, mRSMCB->GetCBV());
					gpuDescriptorHeap->AddToHandle(device, cbvHandleRSM, mRSMCB2->GetCBV());

					DXRS::DescriptorHandle srvHandleRSM = gpuDescriptorHeap->GetHandleBlock(5);
					gpuDescriptorHeap->AddToHandle(device, srvHandleRSM, mRSMBuffersRTs[0]->GetSRV());
					gpuDescriptorHeap->AddToHandle(device, srvHandleRSM, mRSMBuffersRTs[1]->GetSRV());
					gpuDescriptorHeap->AddToHandle(device, srvHandleRSM, mRSMBuffersRTs[2]->GetSRV());
					gpuDescriptorHeap->AddToHandle(device, srvHandleRSM, mGbufferRTs[2]->GetSRV());
					gpuDescriptorHeap->AddToHandle(device, srvHandleRSM, mGbufferRTs[1]->GetSRV());

					DXRS::DescriptorHandle uavHandleRSM = gpuDescriptorHeap->GetHandleBlock(1);
					gpuDescriptorHeap->AddToHandle(device, uavHandleRSM, mRSMRT->GetUAV());

					commandList->SetComputeRootDescriptorTable(0, cbvHandleRSM.GetGPUHandle());
					commandList->SetComputeRootDescriptorTable(1, srvHandleRSM.GetGPUHandle());
					commandList->SetComputeRootDescriptorTable(2, uavHandleRSM.GetGPUHandle());

					commandList->Dispatch(DivideByMultiple(static_cast<UINT>(mRSMRT->GetWidth()), 8u), DivideByMultiple(static_cast<UINT>(mRSMRT->GetHeight()), 8u), 1u);
				}
			}
			PIXEndEvent(commandList);

			// upsample & blur
			PIXBeginEvent(commandList, 0, "RSM upsample & blur");
			{
				commandList->SetPipelineState(mRSMUpsampleAndBlurPSO.GetPipelineStateObject());
				commandList->SetComputeRootSignature(mRSMUpsampleAndBlurRS.GetSignature());

				mSandboxFramework->ResourceBarriersBegin(mBarriers);
				mRSMUpsampleAndBlurRT->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
				mSandboxFramework->ResourceBarriersEnd(mBarriers, commandList);

				DXRS::DescriptorHandle srvHandleBlurRSM = gpuDescriptorHeap->GetHandleBlock(1);
				gpuDescriptorHeap->AddToHandle(device, srvHandleBlurRSM, mRSMRT->GetSRV());

				DXRS::DescriptorHandle uavHandleBlurRSM = gpuDescriptorHeap->GetHandleBlock(1);
				gpuDescriptorHeap->AddToHandle(device, uavHandleBlurRSM, mRSMUpsampleAndBlurRT->GetUAV());

				commandList->SetComputeRootDescriptorTable(0, srvHandleBlurRSM.GetGPUHandle());
				commandList->SetComputeRootDescriptorTable(1, uavHandleBlurRSM.GetGPUHandle());

				commandList->Dispatch(DivideByMultiple(static_cast<UINT>(mRSMUpsampleAndBlurRT->GetWidth()), 8u), DivideByMultiple(static_cast<UINT>(mRSMUpsampleAndBlurRT->GetHeight()), 8u), 1u);
			}
			PIXEndEvent(commandList);
		}
	}
	else 
		clearRSMRT();
	
	//lpv
	if (mLPVEnabled) {
		PIXBeginEvent(commandList, 0, "LPV Injection");
		{
			CD3DX12_VIEWPORT lpvBuffersViewport = CD3DX12_VIEWPORT(0.0f, 0.0f, LPV_SIZE, LPV_SIZE);
			CD3DX12_RECT lpvRect = CD3DX12_RECT(0.0f, 0.0f, LPV_SIZE, LPV_SIZE);
			commandList->RSSetViewports(1, &lpvBuffersViewport);
			commandList->RSSetScissorRects(1, &lpvRect);

			commandList->SetPipelineState(mLPVInjectionPSO.GetPipelineStateObject());
			commandList->SetGraphicsRootSignature(mLPVInjectionRS.GetSignature());

			mSandboxFramework->ResourceBarriersBegin(mBarriers);
			mLPVSHColorsRTs[0]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
			mLPVSHColorsRTs[1]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
			mLPVSHColorsRTs[2]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
			mSandboxFramework->ResourceBarriersEnd(mBarriers, commandList);

			D3D12_CPU_DESCRIPTOR_HANDLE rtvHandlesLPVInjection[] =
			{
				mLPVSHColorsRTs[0]->GetRTV().GetCPUHandle(),
				mLPVSHColorsRTs[1]->GetRTV().GetCPUHandle(),
				mLPVSHColorsRTs[2]->GetRTV().GetCPUHandle()
			};

			commandList->OMSetRenderTargets(_countof(rtvHandlesLPVInjection), rtvHandlesLPVInjection, FALSE, nullptr);
			commandList->ClearRenderTargetView(rtvHandlesLPVInjection[0], clearColorBlack, 0, nullptr);
			commandList->ClearRenderTargetView(rtvHandlesLPVInjection[1], clearColorBlack, 0, nullptr);
			commandList->ClearRenderTargetView(rtvHandlesLPVInjection[2], clearColorBlack, 0, nullptr);

			DXRS::DescriptorHandle cbvHandleLPVInjection = gpuDescriptorHeap->GetHandleBlock(1);
			gpuDescriptorHeap->AddToHandle(device, cbvHandleLPVInjection, mLPVCB->GetCBV());

			DXRS::DescriptorHandle srvHandleLPVInjection = gpuDescriptorHeap->GetHandleBlock(3);
			gpuDescriptorHeap->AddToHandle(device, srvHandleLPVInjection, mRSMBuffersRTs[0]->GetSRV());
			gpuDescriptorHeap->AddToHandle(device, srvHandleLPVInjection, mRSMBuffersRTs[1]->GetSRV());
			gpuDescriptorHeap->AddToHandle(device, srvHandleLPVInjection, mRSMBuffersRTs[2]->GetSRV());

			commandList->SetGraphicsRootDescriptorTable(0, cbvHandleLPVInjection.GetGPUHandle());
			commandList->SetGraphicsRootDescriptorTable(1, srvHandleLPVInjection.GetGPUHandle());

			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
			commandList->DrawInstanced(RSM_SIZE * RSM_SIZE, 1, 0, 0);

			//reset back
			commandList->RSSetViewports(1, &viewport);
			commandList->RSSetScissorRects(1, &rect);
		}
		PIXEndEvent(commandList);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandlesLPVPropagation[] =
		{
			mLPVSHColorsRTs[0]->GetRTV().GetCPUHandle(),
			mLPVSHColorsRTs[1]->GetRTV().GetCPUHandle(),
			mLPVSHColorsRTs[2]->GetRTV().GetCPUHandle(),
			mLPVAccumulationSHColorsRTs[0]->GetRTV().GetCPUHandle(),
			mLPVAccumulationSHColorsRTs[1]->GetRTV().GetCPUHandle(),
			mLPVAccumulationSHColorsRTs[2]->GetRTV().GetCPUHandle()
		};
		CD3DX12_VIEWPORT lpvBuffersViewport = CD3DX12_VIEWPORT(0.0f, 0.0f, LPV_SIZE, LPV_SIZE);
		CD3DX12_RECT lpvRect = CD3DX12_RECT(0.0f, 0.0f, LPV_SIZE, LPV_SIZE);
		commandList->RSSetViewports(1, &lpvBuffersViewport);
		commandList->RSSetScissorRects(1, &lpvRect);

		for (int step = 0; step < mLPVPropagationSteps; step++) {
			std::string titlePix = "LPV Propagation " + std::to_string(step);
			PIXBeginEvent(commandList, 0, titlePix.c_str());
			{
				if (step == 0) {
					commandList->SetPipelineState(mLPVPropagationPSO.GetPipelineStateObject());
					commandList->SetGraphicsRootSignature(mLPVPropagationRS.GetSignature());

					mSandboxFramework->ResourceBarriersBegin(mBarriers);
					mLPVSHColorsRTs[0]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
					mLPVSHColorsRTs[1]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
					mLPVSHColorsRTs[2]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
					mLPVAccumulationSHColorsRTs[0]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
					mLPVAccumulationSHColorsRTs[1]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
					mLPVAccumulationSHColorsRTs[2]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
					mSandboxFramework->ResourceBarriersEnd(mBarriers, commandList);
				}

				commandList->OMSetRenderTargets(_countof(rtvHandlesLPVPropagation), rtvHandlesLPVPropagation, FALSE, nullptr);
				commandList->OMSetBlendFactor(clearColorWhite);
				if (step == 0) {
					commandList->ClearRenderTargetView(rtvHandlesLPVPropagation[3], clearColorBlack, 0, nullptr);
					commandList->ClearRenderTargetView(rtvHandlesLPVPropagation[4], clearColorBlack, 0, nullptr);
					commandList->ClearRenderTargetView(rtvHandlesLPVPropagation[5], clearColorBlack, 0, nullptr);
				}

				DXRS::DescriptorHandle srvHandleLPVInjection = gpuDescriptorHeap->GetHandleBlock(3);
				gpuDescriptorHeap->AddToHandle(device, srvHandleLPVInjection, mLPVSHColorsRTs[0]->GetSRV());
				gpuDescriptorHeap->AddToHandle(device, srvHandleLPVInjection, mLPVSHColorsRTs[1]->GetSRV());
				gpuDescriptorHeap->AddToHandle(device, srvHandleLPVInjection, mLPVSHColorsRTs[2]->GetSRV());

				commandList->SetGraphicsRootDescriptorTable(0, srvHandleLPVInjection.GetGPUHandle());

				commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				commandList->DrawInstanced(3, LPV_SIZE, 0, 0);
			}
			PIXEndEvent(commandList);
		}
		//reset back
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &rect);
	}

	//lighting pass
	PIXBeginEvent(commandList, 0, "Lighting");
	{
		commandList->SetPipelineState(mLightingPSO.GetPipelineStateObject());
		commandList->SetGraphicsRootSignature(mLightingRS.GetSignature());

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandlesLighting[] =
		{
			mLightingRTs[0]->GetRTV().GetCPUHandle()
		};

		commandList->OMSetRenderTargets(_countof(rtvHandlesLighting), rtvHandlesLighting, FALSE, nullptr);
		commandList->ClearRenderTargetView(rtvHandlesLighting[0], clearColorBlack, 0, nullptr);

		DXRS::DescriptorHandle cbvHandleLighting = gpuDescriptorHeap->GetHandleBlock(3);
		gpuDescriptorHeap->AddToHandle(device, cbvHandleLighting, mLightingCB->GetCBV());
		gpuDescriptorHeap->AddToHandle(device, cbvHandleLighting, mLightsInfoCB->GetCBV());
		gpuDescriptorHeap->AddToHandle(device, cbvHandleLighting, mLPVCB->GetCBV());

		DXRS::DescriptorHandle srvHandleLighting = gpuDescriptorHeap->GetHandleBlock(9);
		gpuDescriptorHeap->AddToHandle(device, srvHandleLighting, mGbufferRTs[0]->GetSRV());
		gpuDescriptorHeap->AddToHandle(device, srvHandleLighting, mGbufferRTs[1]->GetSRV());		
		gpuDescriptorHeap->AddToHandle(device, srvHandleLighting, mGbufferRTs[2]->GetSRV());		
		if (mRSMEnabled) {
			if (mRSMUseUpsampleAndBlur) {
				gpuDescriptorHeap->AddToHandle(device, srvHandleLighting, mRSMUpsampleAndBlurRT->GetSRV());
			}
			else
				gpuDescriptorHeap->AddToHandle(device, srvHandleLighting, mRSMRT->GetSRV());
		}
		else 
			gpuDescriptorHeap->AddToHandle(device, srvHandleLighting, mRSMRT->GetSRV());
		gpuDescriptorHeap->AddToHandle(device, srvHandleLighting, mDepthStencil->GetSRV());
		gpuDescriptorHeap->AddToHandle(device, srvHandleLighting, mShadowDepth->GetSRV());
		gpuDescriptorHeap->AddToHandle(device, srvHandleLighting, mLPVAccumulationSHColorsRTs[0]->GetSRV());
		gpuDescriptorHeap->AddToHandle(device, srvHandleLighting, mLPVAccumulationSHColorsRTs[1]->GetSRV());
		gpuDescriptorHeap->AddToHandle(device, srvHandleLighting, mLPVAccumulationSHColorsRTs[2]->GetSRV());

		commandList->SetGraphicsRootDescriptorTable(0, cbvHandleLighting.GetGPUHandle());
		commandList->SetGraphicsRootDescriptorTable(1, srvHandleLighting.GetGPUHandle());

		commandList->IASetVertexBuffers(0, 1, &mSandboxFramework->GetFullscreenQuadBufferView());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		commandList->DrawInstanced(4, 1, 0, 0);
	}
	PIXEndEvent(commandList);

	// composite pass
	PIXBeginEvent(commandList, 0, "Composite");
	{
		commandList->SetPipelineState(mCompositePSO.GetPipelineStateObject());
		commandList->SetGraphicsRootSignature(mCompositeRS.GetSignature());

		mSandboxFramework->ResourceBarriersBegin(mBarriers);
		mLightingRTs[0]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		mSandboxFramework->ResourceBarriersEnd(mBarriers, commandList);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandlesFinal[] =
		{
			 mSandboxFramework->GetRenderTargetView()
		};

		commandList->OMSetRenderTargets(_countof(rtvHandlesFinal), rtvHandlesFinal, FALSE, nullptr);

		DXRS::DescriptorHandle srvHandleComposite = gpuDescriptorHeap->GetHandleBlock(1);
		gpuDescriptorHeap->AddToHandle(device, srvHandleComposite, mLightingRTs[0]->GetSRV());

		commandList->SetGraphicsRootDescriptorTable(0, srvHandleComposite.GetGPUHandle());
		commandList->IASetVertexBuffers(0, 1, &mSandboxFramework->GetFullscreenQuadBufferView());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		commandList->DrawInstanced(4, 1, 0, 0);
	}
	PIXEndEvent(commandList);

	//draw imgui 
	PIXBeginEvent(commandList, 0, "ImGui");
	{
		ID3D12DescriptorHeap* ppHeaps[] = { mUIDescriptorHeap.Get() };
		commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
	}
	PIXEndEvent(commandList);

	// Show the new frame.
	mSandboxFramework->Present();
	mGraphicsMemory->Commit(mSandboxFramework->GetCommandQueue());
}

// *** UPDATES *** //
void DXRSExampleGIScene::Update(DXRSTimer const& timer)
{
	UpdateControls();
	UpdateCamera();
	UpdateLights();
	UpdateBuffers(timer);
	UpdateImGui();

	mWorld = XMMatrixIdentity(); // TODO remove from update
	XMMATRIX local = mWorld * XMMatrixScaling(0.4f, 0.4f, 0.4f);
	mDragonModel->UpdateWorldMatrix(local);	
	
	local = mWorld * XMMatrixTranslation(-25.0f, -1.0f, -36.0f) * XMMatrixScaling(0.45f, 0.45f, 0.45f);
	mSphereModel_1->UpdateWorldMatrix(local);

	local = mWorld * XMMatrixTranslation(-15.0f, -1.0f, -21.0f) * XMMatrixScaling(1.15f, 1.15f, 1.15f);
	mSphereModel_2->UpdateWorldMatrix(local);

	local = mWorld * XMMatrixScaling(8.0f, 8.0f, 8.0f) * XMMatrixRotationX(-3.14f / 2.0f);
	mRoomModel->UpdateWorldMatrix(local);	
	
	local = local = mWorld * XMMatrixTranslation(0.5f, -12.0f, -15.0f) * XMMatrixScaling(2.5f, 2.5f, 1.5f) * XMMatrixRotationX(3.14f / 2.0f);
	mBlockModel->UpdateWorldMatrix(local);

}

void DXRSExampleGIScene::UpdateBuffers(DXRSTimer const& timer)
{
	float width = mSandboxFramework->GetOutputSize().right;
	float height = mSandboxFramework->GetOutputSize().bottom;
	GBufferCBData gbufferPassData;
	gbufferPassData.ViewProjection = mCameraView * mCameraProjection;
	gbufferPassData.InvViewProjection = XMMatrixInverse(nullptr, gbufferPassData.ViewProjection);
	gbufferPassData.CameraPos = XMFLOAT4(mCameraEye.x, mCameraEye.y, mCameraEye.z, 1);
	gbufferPassData.ScreenSize = { width, height, 1.0f / width, 1.0f / height };
	gbufferPassData.LightColor = XMFLOAT4(mDirectionalLightColor[0], mDirectionalLightColor[1], mDirectionalLightColor[2], mDirectionalLightColor[3]);
	memcpy(mGbufferCB->Map(), &gbufferPassData, sizeof(gbufferPassData));

	LightingCBData lightPassData = {};
	lightPassData.InvViewProjection = XMMatrixInverse(nullptr, gbufferPassData.ViewProjection);
	lightPassData.ShadowViewProjection = mLightViewProjection;
	lightPassData.ShadowTexelSize = XMFLOAT2(1.0f / mShadowDepth->GetWidth(), 1.0f / mShadowDepth->GetHeight());
	lightPassData.ShadowIntensity = mShadowIntensity;
	lightPassData.CameraPos = XMFLOAT4(mCameraEye.x, mCameraEye.y, mCameraEye.z, 1);
	lightPassData.ScreenSize = { width, height, 1.0f / width, 1.0f / height };
	memcpy(mLightingCB->Map(), &lightPassData, sizeof(lightPassData));

	RSMCBData rsmPassData = {};
	rsmPassData.ShadowViewProjection = mLightViewProjection;
	rsmPassData.RSMIntensity = mRSMIntensity;
	rsmPassData.RSMRMax = mRSMRMax;
	rsmPassData.UpsampleRatio = XMFLOAT2(mGbufferRTs[0]->GetWidth() / mRSMRT->GetWidth(), mGbufferRTs[0]->GetHeight() / mRSMRT->GetHeight());
	memcpy(mRSMCB->Map(), &rsmPassData, sizeof(rsmPassData));

	LPVCBData lpvData = {};
	lpvData.worldToLPV = mWorldToLPV;
	lpvData.LPVCutoff = mLPVCutoff;
	lpvData.LPVPower = mLPVPower;
	lpvData.LPVAttenuation = mLPVAttenuation;
	memcpy(mLPVCB->Map(), &lpvData, sizeof(lpvData));
}

void DXRSExampleGIScene::UpdateImGui()
{
	//capture mouse clicks
	ImGui::GetIO().MouseDown[0] = (GetKeyState(VK_LBUTTON) & 0x8000) != 0;
	ImGui::GetIO().MouseDown[1] = (GetKeyState(VK_RBUTTON) & 0x8000) != 0;
	ImGui::GetIO().MouseDown[2] = (GetKeyState(VK_MBUTTON) & 0x8000) != 0;

	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	{
		ImGui::Begin("DirectX GI Sandbox");
		ImGui::TextColored(ImVec4(0.95f, 0.5f, 0.0f, 1), "FPS: (%.1f FPS), %.3f ms/frame", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
		ImGui::SliderFloat4("Light Color", mDirectionalLightColor, 0.0f, 1.0f);
		ImGui::SliderFloat4("Light Direction", mDirectionalLightDir, -1.0f, 1.0f);
		ImGui::SliderFloat("Light Intensity", &mDirectionalLightIntensity, 0.0f, 5.0f);
		ImGui::SliderFloat("Orbit camera radius", &mCameraRadius, 5.0f, 175.0f);

		ImGui::Separator();
		if (ImGui::CollapsingHeader("Global Illumination Config"))
		{
			if (ImGui::CollapsingHeader("Reflective Shadow Mapping")) {
				ImGui::Checkbox("Enabled", &mRSMEnabled);
				ImGui::Checkbox("Use CS version", &mRSMComputeVersion);
				ImGui::SliderFloat("RSM Intensity", &mRSMIntensity, 0.0f, 15.0f);
				ImGui::SliderFloat("RSM Rmax", &mRSMRMax, 0.0f, 1.0f);
				ImGui::Checkbox("Upsample and Blur CS", &mRSMUseUpsampleAndBlur);
				//ImGui::SliderInt("RSM Samples Count", &mRSMSamplesCount, 1, 1000);
			}
			if (ImGui::CollapsingHeader("Light Propagation Volume")) {
				ImGui::Checkbox("Enabled", &mLPVEnabled);
				ImGui::SliderInt("Propagation steps", &mLPVPropagationSteps, 0, 100);
				ImGui::SliderFloat("Cutoff", &mLPVCutoff, 0.0f, 1.0f);
				ImGui::SliderFloat("Power", &mLPVPower, 0.0f, 2.0f);
				ImGui::SliderFloat("Attenuation", &mLPVAttenuation, 0.0f, 5.0f);
			}
		}
		ImGui::Separator();

		ImGui::End();
	}
}

void DXRSExampleGIScene::UpdateLights()
{
	LightsInfoCBData lightData = {};
	lightData.LightColor = XMFLOAT4(mDirectionalLightColor[0], mDirectionalLightColor[1], mDirectionalLightColor[2], mDirectionalLightColor[3]);
	lightData.LightDirection = XMFLOAT4(mDirectionalLightDir[0], mDirectionalLightDir[1], mDirectionalLightDir[2], mDirectionalLightDir[3]);
	lightData.LightIntensity = mDirectionalLightIntensity;

	memcpy(mLightsInfoCB->Map(), &lightData, sizeof(lightData));
}

void DXRSExampleGIScene::UpdateControls()
{
	auto mouse = mMouse->GetState();
	if (mouse.rightButton)
	{
		float dx = XMConvertToRadians(0.25f * static_cast<float>(mouse.x - mLastMousePosition.x));
		float dy = -XMConvertToRadians(0.25f * static_cast<float>(mouse.y - mLastMousePosition.y));

		mCameraTheta += dx;
		mCameraPhi += dy;

		mCameraPhi = std::clamp(mCameraPhi, 0.1f, 3.14f - 0.1f);
	}
	mLastMousePosition.x = mouse.x;
	mLastMousePosition.y = mouse.y;
	mouse;
}

void DXRSExampleGIScene::UpdateCamera()
{
	float x = mCameraRadius * sinf(mCameraPhi) * cosf(mCameraTheta);
	float y = mCameraRadius * cosf(mCameraPhi);
	float z = mCameraRadius * sinf(mCameraPhi) * sinf(mCameraTheta);

	mCameraEye.x = x;
	mCameraEye.y = y;
	mCameraEye.z = z;

	Vector3 at(0.0f, 0.0f, 0.0f);

	mCameraView = Matrix::CreateLookAt(mCameraEye, at, Vector3::UnitY);

	CameraBuffer buffer;
	float width = mSandboxFramework->GetOutputSize().right;
	float height = mSandboxFramework->GetOutputSize().bottom;

	buffer.view = mCameraView;
	buffer.projection = mCameraProjection;
	buffer.viewI = XMMatrixInverse(nullptr, mCameraView);
	buffer.projectionI = XMMatrixInverse(nullptr, mCameraProjection);
	buffer.camToWorld = XMMatrixTranspose(XMMatrixInverse(nullptr, mCameraProjection * mCameraView));
	buffer.camPosition = XMFLOAT4(mCameraEye.x, mCameraEye.y, mCameraEye.z, 1.0f);
	buffer.resolution = XMFLOAT2(width, height);

	// Copy the contents
	memcpy(mCameraBuffer->Map(), &buffer, sizeof(CameraBuffer));
}

void DXRSExampleGIScene::UpdateShadow()
{
	XMFLOAT3 shadowCenter(0.0f, 0.0f, 0.0f);
	XMVECTOR eyePosition = XMLoadFloat3(&shadowCenter);
	XMVECTOR direction = XMVECTOR{ -mDirectionalLightDir[0], -mDirectionalLightDir[1], -mDirectionalLightDir[2], mDirectionalLightDir[3] };
	XMVECTOR upDirection = XMVECTOR{0.0f, 1.0f, 0.0f};

	XMMATRIX viewMatrix = XMMatrixLookToRH(eyePosition, direction, upDirection);
	XMMATRIX projectionMatrix = XMMatrixOrthographicRH(256.0f, 256.0f, -256.0f, 256.0f);
	mLightViewProjection = viewMatrix * projectionMatrix;
	mLightView = viewMatrix;
	mLightProj = projectionMatrix;

	ShadowMappingCBData shadowPassData = {};
	shadowPassData.LightViewProj = mLightViewProjection;
	shadowPassData.LightColor = XMFLOAT4(mDirectionalLightColor);
	shadowPassData.LightDir = XMFLOAT4(-mDirectionalLightDir[0], -mDirectionalLightDir[1], -mDirectionalLightDir[2], mDirectionalLightDir[3]);
	memcpy(mShadowMappingCB->Map(), &shadowPassData, sizeof(shadowPassData));
}

void DXRSExampleGIScene::SetProjectionMatrix()
{
	auto size = mSandboxFramework->GetOutputSize();
	float aspectRatio = float(size.right) / float(size.bottom);
	float fovAngleY = 60.0f * XM_PI / 180.0f;

	if (aspectRatio < 1.0f)
		fovAngleY *= 2.0f;

	mCameraProjection = Matrix::CreatePerspectiveFieldOfView(fovAngleY, aspectRatio, 0.01f, 500.0f);
}

void DXRSExampleGIScene::OnWindowSizeChanged(int width, int height)
{
	if (!mSandboxFramework->WindowSizeChanged(width, height))
		return;

	SetProjectionMatrix();
}

void DXRSExampleGIScene::ThrowFailedErrorBlob(ID3DBlob* blob)
{
	std::string message = "";
	if (blob)
		message.append((char*)blob->GetBufferPointer());

	throw std::runtime_error(message.c_str());
}