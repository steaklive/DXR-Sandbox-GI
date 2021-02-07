#include "DXRSExampleRTScene.h"

#include "DescriptorHeap.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

namespace {
    D3D12_HEAP_PROPERTIES UploadHeapProps = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0, 0 };
    D3D12_HEAP_PROPERTIES DefaultHeapProps = { D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 0, 0 };
}

DXRSExampleRTScene::DXRSExampleRTScene()
{
    mSandboxFramework = new DXRSGraphics();
}

DXRSExampleRTScene::~DXRSExampleRTScene()
{
	if (mSandboxFramework)
		mSandboxFramework->WaitForGpu();

    delete mLightingCB;
    delete mLightsInfoCB;
    delete mGbufferCB;
    delete mTLASBuffer;
    delete mCameraBuffer;
}

void DXRSExampleRTScene::Init(HWND window, int width, int height)
{
    mGamePad = std::make_unique<DirectX::GamePad>();
    mKeyboard = std::make_unique<DirectX::Keyboard>();
    mMouse = std::make_unique<DirectX::Mouse>();
    mMouse->SetWindow(window);

    mSandboxFramework->SetWindow(window, width, height);

    mSandboxFramework->CreateResources();
    mSandboxFramework->CreateFullscreenQuadBuffers();

    mDragonModel = U_PTR<DXRSModel>(new DXRSModel(*mSandboxFramework, mSandboxFramework->GetFilePath("content\\models\\dragon.fbx"), true, XMMatrixIdentity(), XMFLOAT4(0, 1, 0, 0))); //storing reflectivity in w of color
    mPlaneModel = U_PTR<DXRSModel>(new DXRSModel(*mSandboxFramework, mSandboxFramework->GetFilePath("content\\models\\plane.fbx"), true, XMMatrixIdentity(), XMFLOAT4(0.2, 0.2, 0.2, 0.15))); //storing reflectivity in w of color

    CreateRaytracingAccelerationStructures();
    CreateRaytracingShaders();
    CreateRaytracingPSO();

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
    mDepthStencil = new DXRSDepthBuffer(device, descriptorManager, 1920, 1080, DXGI_FORMAT_D32_FLOAT);

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

        mGbufferRTs.push_back(new DXRSRenderTarget(device, descriptorManager, 1920, 1080, rtFormat, flags, L"Albedo"));

        rtFormat = DXGI_FORMAT_R16G16B16A16_SNORM;
        mGbufferRTs.push_back(new DXRSRenderTarget(device, descriptorManager, 1920, 1080, rtFormat, flags, L"Normals"));

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

        ThrowIfFailed(D3DCompileFromFile(mSandboxFramework->GetFilePath(L"content\\shaders\\GBuffer.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_1", compileFlags, 0, &vertexShader, nullptr));

        compileFlags |= D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES;

        ThrowIfFailed(D3DCompileFromFile(mSandboxFramework->GetFilePath(L"content\\shaders\\GBuffer.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_1", compileFlags, 0, &pixelShader, &errorBlob));

        // Define the vertex input layout.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // Describe and create the graphics pipeline state object (PSO).
        DXGI_FORMAT formats[2];
        formats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        formats[1] = DXGI_FORMAT_R16G16B16A16_SNORM;

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

    // create resources lighting pass
    {
        //RTs
        mLightingRTs.push_back(new DXRSRenderTarget(device, descriptorManager, 1920, 1080, DXGI_FORMAT_R11G11B10_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"Light Diffuse"));
        mLightingRTs.push_back(new DXRSRenderTarget(device, descriptorManager, 1920, 1080, DXGI_FORMAT_R11G11B10_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, L"Light Specular"));

        //create root signature
        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            //	D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

        mLightingRS.Reset(2, 0);
        mLightingRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 3, D3D12_SHADER_VISIBILITY_ALL);
        mLightingRS[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 6, D3D12_SHADER_VISIBILITY_PIXEL);
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

        DXGI_FORMAT m_rtFormats[2];
        m_rtFormats[0] = DXGI_FORMAT_R11G11B10_FLOAT;
        m_rtFormats[1] = DXGI_FORMAT_R11G11B10_FLOAT;

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
        mCompositeRS[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 4, D3D12_SHADER_VISIBILITY_PIXEL);
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

    CreateRaytracingResourceHeap();
    CreateRaytracingShaderTable();
}

void DXRSExampleRTScene::Clear()
{
    auto commandList = mSandboxFramework->GetCommandList();

    auto rtvDescriptor = mSandboxFramework->GetRenderTargetView();
    auto dsvDescriptor = mSandboxFramework->GetDepthStencilView();

    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
    commandList->ClearRenderTargetView(rtvDescriptor, Colors::Gray, 0, nullptr);
    commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    auto viewport = mSandboxFramework->GetScreenViewport();
    auto scissorRect = mSandboxFramework->GetScissorRect();
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);
}

void DXRSExampleRTScene::Run()
{
    mTimer.Run([&]()
    {
        Update(mTimer);
    });

    Render();
}

void DXRSExampleRTScene::CreateRaytracingPSO()
{
    ID3D12Device5* device = mSandboxFramework->GetDXRDevice();
    RayTracingPipelineGenerator pipeline(device);

    CD3DX12_ROOT_PARAMETER1 globalRootSignatureParameters[2];
    globalRootSignatureParameters[0].InitAsConstantBufferView(0); // camera buffer
    globalRootSignatureParameters[1].InitAsConstantBufferView(1); // light buffer
    auto globalRootSignatureDesc = CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC(ARRAYSIZE(globalRootSignatureParameters), globalRootSignatureParameters);

    ComPtr<ID3DBlob> pGlobalRootSignatureBlob;
    ComPtr<ID3DBlob> pErrorBlob;
    if (FAILED(D3D12SerializeVersionedRootSignature(&globalRootSignatureDesc, &pGlobalRootSignatureBlob, &pErrorBlob)))
        OutputDebugStringA((LPCSTR)pErrorBlob->GetBufferPointer());
    device->CreateRootSignature(0, pGlobalRootSignatureBlob->GetBufferPointer(), pGlobalRootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&mGlobalRaytracingRootSignature));
    mGlobalRaytracingRootSignature->SetName(L"Global RS");

    pipeline.SetGlobalRootSignature(mGlobalRaytracingRootSignature.Get());

    pipeline.AddLibrary(mRaygenBlob, { L"RayGen", L"ShadowRayGen" });
    pipeline.AddLibrary(mMissBlob, { L"Miss", L"ShadowMiss" });
    pipeline.AddLibrary(mClosestHitBlob, {L"ClosestHit", L"ShadowClosestHit" });

    pipeline.AddHitGroup(L"HitGroup", L"ClosestHit");
    pipeline.AddHitGroup(L"ShadowHitGroup", L"ShadowClosestHit");
    
    pipeline.AddRootSignatureAssociation(mRaygenRS.GetSignature(), { L"RayGen", L"ShadowRayGen" });
    pipeline.AddRootSignatureAssociation(mMissRS.GetSignature(), { L"Miss", L"ShadowMiss"});
    pipeline.AddRootSignatureAssociation(mClosestHitRS.GetSignature(), { L"HitGroup", L"ShadowHitGroup" });
    
    pipeline.SetMaxPayloadSize(/*sizeof(XMFLOAT4)*/8);
    pipeline.SetMaxAttributeSize(sizeof(XMFLOAT2)); // barycentric coordinates - not used
    pipeline.SetMaxRecursionDepth(1);
    
    // Compile the pipeline for execution on the GPU
    mRaytracingPSO = pipeline.Generate();
    
    // Cast the state object into a properties object, allowing to later access
    // the shader pointers by name
    ThrowIfFailed(mRaytracingPSO->QueryInterface(IID_PPV_ARGS(&mRaytracingPSOProperties)));
}

void DXRSExampleRTScene::CreateRaytracingAccelerationStructures()
{
    ID3D12Device5* device = mSandboxFramework->GetDXRDevice();
    ID3D12GraphicsCommandList4* commandList = (ID3D12GraphicsCommandList4*)mSandboxFramework->GetCommandList();

    //Create BLAS
    {
        //dragon mesh
        {
            DXRSMesh* mesh = mDragonModel->Meshes()[0];
        
            D3D12_RAYTRACING_GEOMETRY_DESC desc;
            desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
            desc.Triangles.VertexBuffer.StartAddress = mesh->GetVertexBuffer()->GetGPUVirtualAddress();
            desc.Triangles.VertexBuffer.StrideInBytes = mesh->GetVertexBufferView().StrideInBytes;
            desc.Triangles.VertexCount = static_cast<UINT>(mesh->Vertices().size());
            desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
            desc.Triangles.IndexBuffer = mesh->GetIndexBuffer()->GetGPUVirtualAddress();
            desc.Triangles.IndexFormat = mesh->GetIndexBufferView().Format;
            desc.Triangles.IndexCount = static_cast<UINT>(mesh->Indices().size());
            desc.Triangles.Transform3x4 = 0;
            desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
        
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        
            // Get the size requirements for the BLAS buffers
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
            ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
            ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
            ASInputs.pGeometryDescs = &desc;
            ASInputs.NumDescs = 1;
            ASInputs.Flags = buildFlags;
        
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = {};
            device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &ASPreBuildInfo);
        
            ASPreBuildInfo.ScratchDataSizeInBytes = Align(ASPreBuildInfo.ScratchDataSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
            ASPreBuildInfo.ResultDataMaxSizeInBytes = Align(ASPreBuildInfo.ResultDataMaxSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
        
            // Create the BLAS scratch buffer
            DXRSBuffer::Description blasdesc;
            blasdesc.mAlignment = std::max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
            blasdesc.mSize = (UINT)ASPreBuildInfo.ScratchDataSizeInBytes;
            blasdesc.mState = D3D12_RESOURCE_STATE_COMMON;
            blasdesc.mDescriptorType = DXRSBuffer::DescriptorType::Raw;
            blasdesc.mResourceFlags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        
            DXRSBuffer* blasScratchBuffer = new DXRSBuffer(device, mSandboxFramework->GetDescriptorHeapManager(), commandList, blasdesc, L"BLAS Scratch Buffer");
        
            // Create the BLAS buffer
            blasdesc.mElementSize = (UINT)ASPreBuildInfo.ResultDataMaxSizeInBytes;
            blasdesc.mState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        
            DXRSBuffer* blasBuffer = new DXRSBuffer(device, mSandboxFramework->GetDescriptorHeapManager(), commandList, blasdesc, L"BLAS Buffer");
        
            // Describe and build the bottom level acceleration structure
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
            buildDesc.Inputs = ASInputs;
            buildDesc.ScratchAccelerationStructureData = blasScratchBuffer->GetResource()->GetGPUVirtualAddress();
            buildDesc.DestAccelerationStructureData = blasBuffer->GetResource()->GetGPUVirtualAddress();
            
            commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
        
            // Wait for the BLAS build to complete
            D3D12_RESOURCE_BARRIER uavBarrier;
            uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            uavBarrier.UAV.pResource = blasBuffer->GetResource();
            uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            commandList->ResourceBarrier(1, &uavBarrier);
        
            mDragonModel->SetBlasBuffer(blasBuffer);
        
            //delete blasScratchBuffer;
        }
    }

    //Create TLAS
    {
        D3D12_RAYTRACING_INSTANCE_DESC* instanceDescriptions = new D3D12_RAYTRACING_INSTANCE_DESC[2]; //plane and dragon
        int noofInstances = 0;

        //dragon
        {

            D3D12_RAYTRACING_INSTANCE_DESC& instanceDesc = instanceDescriptions[noofInstances];
            // Describe the TLAS geometry instance(s)
            instanceDesc.InstanceID = noofInstances;// This value is exposed to shaders as SV_InstanceID
            instanceDesc.InstanceContributionToHitGroupIndex = noofInstances;
            instanceDesc.InstanceMask = 0xFF;

            DirectX::XMMATRIX m = XMMatrixTranspose(mWorld * Matrix::CreateScale(0.3f, 0.3f, 0.3f) * Matrix::CreateTranslation(0, 0, 0.0f));
            memcpy(instanceDesc.Transform, &m, sizeof(instanceDesc.Transform));

            instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
            instanceDesc.AccelerationStructure = mDragonModel->GetBlasBuffer()->GetResource()->GetGPUVirtualAddress();
            noofInstances++;
        }

        DXRSBuffer::Description desc;
        desc.mSize = noofInstances * sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
        desc.mState = D3D12_RESOURCE_STATE_GENERIC_READ;
        desc.mResourceFlags = D3D12_RESOURCE_FLAG_NONE;
        desc.mHeapType = D3D12_HEAP_TYPE_UPLOAD;
        desc.mDescriptorType = DXRSBuffer::DescriptorType::Raw;
        DXRSBuffer* instanceDescriptionBuffer = new DXRSBuffer(device, mSandboxFramework->GetDescriptorHeapManager(), commandList, desc, L"Instance Description Buffer");

        // Copy the instance data to the buffer
        D3D12_RAYTRACING_INSTANCE_DESC* data;
        instanceDescriptionBuffer->GetResource()->Map(0, nullptr, reinterpret_cast<void**>(&data));
        memcpy(data, instanceDescriptions, noofInstances * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
        instanceDescriptionBuffer->GetResource()->Unmap(0, nullptr);

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

        // Get the size requirements for the TLAS buffers
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
        ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        ASInputs.InstanceDescs = instanceDescriptionBuffer->GetResource()->GetGPUVirtualAddress();
        ASInputs.NumDescs = noofInstances;
        ASInputs.Flags = buildFlags;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = {};
        device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &ASPreBuildInfo);

        ASPreBuildInfo.ResultDataMaxSizeInBytes = Align(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ResultDataMaxSizeInBytes);
        ASPreBuildInfo.ScratchDataSizeInBytes = Align(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ScratchDataSizeInBytes);

        // Create TLAS scratch buffer
        DXRSBuffer::Description tlasDesc;
        tlasDesc.mAlignment = std::max(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
        tlasDesc.mElementSize = (UINT)ASPreBuildInfo.ScratchDataSizeInBytes;
        tlasDesc.mState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        tlasDesc.mResourceFlags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        tlasDesc.mDescriptorType = DXRSBuffer::DescriptorType::Raw;

        DXRSBuffer* tlasScratchBuffer = new DXRSBuffer(device, mSandboxFramework->GetDescriptorHeapManager(), commandList, tlasDesc, L"TLAS Scratch Buffer");

        // Create the TLAS buffer
        tlasDesc.mElementSize = (UINT)ASPreBuildInfo.ResultDataMaxSizeInBytes;
        tlasDesc.mState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        DXRSBuffer* tlasBuffer = new DXRSBuffer(device, mSandboxFramework->GetDescriptorHeapManager(), commandList, tlasDesc, L"TLAS Buffer");

        // Describe and build the TLAS
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
        buildDesc.Inputs = ASInputs;
        buildDesc.ScratchAccelerationStructureData = tlasScratchBuffer->GetResource()->GetGPUVirtualAddress();
        buildDesc.DestAccelerationStructureData = tlasBuffer->GetResource()->GetGPUVirtualAddress();

        commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

        // Wait for the TLAS build to complete
        D3D12_RESOURCE_BARRIER uavBarrier;
        uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        uavBarrier.UAV.pResource = tlasBuffer->GetResource();
        uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        commandList->ResourceBarrier(1, &uavBarrier);

        mTLASBuffer = tlasBuffer;
    }

}

void DXRSExampleRTScene::CreateRaytracingShaders()
{
    auto device = mSandboxFramework->GetDXRDevice();

    //raytracing shaders have to have local root signature
    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

    //compile raygen shader
    {
        mRaygenBlob = mSandboxFramework->CompileShaderLibrary(mSandboxFramework->GetFilePath(L"content\\shaders\\RayGen.hlsl").c_str());

        // create root signature
        mRaygenRS.Reset(1, 0);
        mRaygenRS[0].InitAsDescriptorTable(2);
        mRaygenRS[0].SetTableRange(0, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1, 0);
        mRaygenRS[0].SetTableRange(1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 4, 0);
        mRaygenRS.Finalize(device, L"Raygen RS", rootSignatureFlags);
    }

    //compile hit shader
    {
        mClosestHitBlob = mSandboxFramework->CompileShaderLibrary(mSandboxFramework->GetFilePath(L"content\\shaders\\Hit.hlsl").c_str());

        // create root signature
        mClosestHitRS.Reset(1, 0);
        mClosestHitRS[0].InitAsDescriptorTable(3);
        mClosestHitRS[0].SetTableRange(0, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1, 0);
        mClosestHitRS[0].SetTableRange(1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 6, 0);
        mClosestHitRS[0].SetTableRange(2, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 2, 1, 0);

        mClosestHitRS.Finalize(device, L"Closest Hit RS", rootSignatureFlags);
    }

    //compile miss shader
    {
        mMissBlob = mSandboxFramework->CompileShaderLibrary(mSandboxFramework->GetFilePath(L"content\\shaders\\Miss.hlsl").c_str());
        
        mMissRS.Reset(1, 0);
        mMissRS[0].InitAsDescriptorTable(1);
        mMissRS[0].SetTableRange(0, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1, 0);
        mMissRS.Finalize(device, L"Miss RS", rootSignatureFlags);
    }
}

void DXRSExampleRTScene::CreateRaytracingShaderTable()
{
    auto device = mSandboxFramework->GetD3DDevice();

    // The SBT helper class collects calls to Add*Program.  If called several
    // times, the helper must be emptied before re-adding shaders.
    mRaytracingShaderBindingTableHelper.Reset();

    // The pointer to the beginning of the heap is the only parameter required by shaders without root parameters
    D3D12_GPU_DESCRIPTOR_HANDLE srvUavHeapHandle = mRaytracingDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    
    // The helper treats both root parameter pointers and heap pointers as void*,
    // while DX12 uses the
    // D3D12_GPU_DESCRIPTOR_HANDLE to define heap pointers. The pointer in this
    // struct is a UINT64, which then has to be reinterpreted as a pointer.
    auto heapPointer = reinterpret_cast<UINT64*>(srvUavHeapHandle.ptr);
    
    mRaytracingShaderBindingTableHelper.AddRayGenerationProgram(L"RayGen", { heapPointer });
    mRaytracingShaderBindingTableHelper.AddRayGenerationProgram(L"ShadowRayGen", { heapPointer });
    mRaytracingShaderBindingTableHelper.AddMissProgram(L"Miss", { heapPointer });
    mRaytracingShaderBindingTableHelper.AddMissProgram(L"ShadowMiss", { heapPointer });
    mRaytracingShaderBindingTableHelper.AddHitGroup(L"HitGroup", { heapPointer/*, (void*)(mPlaneModel->Meshes()[0]->GetVertexBuffer())*/});
	//mRaytracingShaderBindingTableHelper.AddHitGroup(L"HitGroup", { heapPointer, (void*)(mDragonModel->Meshes()[0]->GetVertexBuffer()) }); 
	mRaytracingShaderBindingTableHelper.AddHitGroup(L"ShadowHitGroup", { heapPointer/*, (void*)(mPlaneModel->Meshes()[0]->GetVertexBuffer())*/ });
	//mRaytracingShaderBindingTableHelper.AddHitGroup(L"ShadowHitGroup", { heapPointer, (void*)(mDragonModel->Meshes()[0]->GetVertexBuffer()) });
    
    // Compute the size of the SBT given the number of shaders and their
    // parameters
    uint32_t sbtSize = mRaytracingShaderBindingTableHelper.ComputeSBTSize();

    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Alignment = 0;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.Height = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufDesc.MipLevels = 1;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.SampleDesc.Quality = 0;
    bufDesc.Width = sbtSize;
    
    ThrowIfFailed(device->CreateCommittedResource(&UploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mRaytracingShaderTableBuffer)));
    
    // Compile the SBT from the shader and parameters info
    mRaytracingShaderBindingTableHelper.Generate(mRaytracingShaderTableBuffer.Get(), mRaytracingPSOProperties.Get());
}

void DXRSExampleRTScene::CreateRaytracingResourceHeap()
{
    auto device = mSandboxFramework->GetD3DDevice();
    auto descriptorHeapManager = mSandboxFramework->GetDescriptorHeapManager();

    // Create a SRV/UAV/CBV descriptor heap. We need 8 entries
    // 1 - UAV for the RT output
    // 1 - SRV for the TLAS
    // 1 - SRV for normals
    // 1 - SRV for depth
    // 1 - SRV for albedo
    // 1 - SRV for mesh indices
    // 1 - SRV for mesh vertices
    // 1 - CBV for mesh info
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = 8;
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mRaytracingDescriptorHeap)));

    // Get a handle to the heap memory on the CPU side, to be able to write the
    // descriptors directly
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = mRaytracingDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

    // UAV
    device->CopyDescriptorsSimple(1, srvHandle, mLightingRTs[0]->GetUAV().GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Add for TLAS SRV
    srvHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.RaytracingAccelerationStructure.Location = mTLASBuffer->GetResource()->GetGPUVirtualAddress();
    // Write the acceleration structure view in the heap
    device->CreateShaderResourceView(nullptr, &srvDesc, srvHandle);

    // Add for normals texture SRV
    srvHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    device->CopyDescriptorsSimple(1, srvHandle, mGbufferRTs[1]->GetSRV().GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    
    // Add for depth texture SRV
    srvHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    device->CopyDescriptorsSimple(1, srvHandle, mDepthStencil->GetSRV().GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Add for albedo texture SRV
    srvHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    device->CopyDescriptorsSimple(1, srvHandle, mGbufferRTs[0]->GetSRV().GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    //*******//
    // for simplicity the rest is hardcoded for one mesh - dragon model
    //*******//

    // Add for indices buffer SRV
    srvHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    device->CopyDescriptorsSimple(1, srvHandle, mDragonModel->Meshes()[0]->GetIndexBufferSRV().GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Add for vertex buffer SRV
    srvHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    device->CopyDescriptorsSimple(1, srvHandle, mDragonModel->Meshes()[0]->GetVertexBufferSRV().GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Add for mesh info buffer CBV
    srvHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    device->CopyDescriptorsSimple(1, srvHandle,  mDragonModel->Meshes()[0]->GetMeshInfoBuffer()->GetCBV().GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

}

void DXRSExampleRTScene::Render()
{
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
    {
        commandList->SetPipelineState(mGbufferPSO.GetPipelineStateObject());
        commandList->SetGraphicsRootSignature(mGbufferRS.GetSignature());

        //transition buffers to rendertarget outputs
        mSandboxFramework->ResourceBarriersBegin(mBarriers);
        mGbufferRTs[0]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
        mGbufferRTs[1]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
        mLightingRTs[0]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
        mLightingRTs[1]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
        // mDepthStencil->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        mSandboxFramework->ResourceBarriersEnd(mBarriers, commandList);

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[] =
        {
            mGbufferRTs[0]->GetRTV().GetCPUHandle(),
            mGbufferRTs[1]->GetRTV().GetCPUHandle()
        };

        CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(mDepthStencil->GetDSV().GetCPUHandle());
        commandList->OMSetRenderTargets(_countof(rtvHandles), rtvHandles, FALSE, &mSandboxFramework->GetDepthStencilView());

        const float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        const float clearNormal[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        commandList->ClearRenderTargetView(rtvHandles[0], clearColor, 0, nullptr);
        commandList->ClearRenderTargetView(rtvHandles[1], clearNormal, 0, nullptr);
        //commandList->ClearDepthStencilView(mDepthStencil->GetDSV().GetCPUHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        DXRS::DescriptorHandle cbvHandle;
        DXRS::DescriptorHandle srvHandle;

        srvHandle = gpuDescriptorHeap->GetHandleBlock(0);

        // no textures
        gpuDescriptorHeap->AddToHandle(device, srvHandle, mNullDescriptor);

        //plane
        {
            cbvHandle = gpuDescriptorHeap->GetHandleBlock(2);
            gpuDescriptorHeap->AddToHandle(device, cbvHandle, mGbufferCB->GetCBV());
            gpuDescriptorHeap->AddToHandle(device, cbvHandle, mPlaneModel->GetCB()->GetCBV());

            commandList->SetGraphicsRootDescriptorTable(0, cbvHandle.GetGPUHandle());
            commandList->SetGraphicsRootDescriptorTable(1, srvHandle.GetGPUHandle());

            mPlaneModel->Render(commandList);
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

    }

    //lighting pass
    {
        const float clearColorLighting[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandlesLighting[] =
        {
            mLightingRTs[0]->GetRTV().GetCPUHandle(),
            mLightingRTs[1]->GetRTV().GetCPUHandle()
        };

        commandList->OMSetRenderTargets(_countof(rtvHandlesLighting), rtvHandlesLighting, FALSE, nullptr);
        commandList->ClearRenderTargetView(rtvHandlesLighting[0], clearColorLighting, 0, nullptr);
        commandList->ClearRenderTargetView(rtvHandlesLighting[1], clearColorLighting, 0, nullptr);
        commandList->SetPipelineState(mLightingPSO.GetPipelineStateObject());
        commandList->SetGraphicsRootSignature(mLightingRS.GetSignature());

        DXRS::DescriptorHandle cbvHandleLighting = gpuDescriptorHeap->GetHandleBlock(2);
        gpuDescriptorHeap->AddToHandle(device, cbvHandleLighting, mLightingCB->GetCBV());
        gpuDescriptorHeap->AddToHandle(device, cbvHandleLighting, mLightsInfoCB->GetCBV());

        DXRS::DescriptorHandle srvHandleLighting = gpuDescriptorHeap->GetHandleBlock(3);
        gpuDescriptorHeap->AddToHandle(device, srvHandleLighting, mGbufferRTs[0]->GetSRV());
        gpuDescriptorHeap->AddToHandle(device, srvHandleLighting, mGbufferRTs[1]->GetSRV());
        gpuDescriptorHeap->AddToHandle(device, srvHandleLighting, mDepthStencil->GetSRV());

        commandList->SetGraphicsRootDescriptorTable(0, cbvHandleLighting.GetGPUHandle());
        commandList->SetGraphicsRootDescriptorTable(1, srvHandleLighting.GetGPUHandle());

        commandList->IASetVertexBuffers(0, 1, &mSandboxFramework->GetFullscreenQuadBufferView());
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        commandList->DrawInstanced(4, 1, 0, 0);

    }
    
    //DXR pass
    {
        ID3D12GraphicsCommandList4* commandListDXR = (ID3D12GraphicsCommandList4*)mSandboxFramework->GetCommandList();
        ID3D12DescriptorHeap* heaps[] = { mRaytracingDescriptorHeap.Get() };
        commandListDXR->SetDescriptorHeaps(_countof(heaps), heaps);

        //mSandboxFramework->ResourceBarriersBegin(mBarriers);
        //mGbufferRTs[0]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        //mGbufferRTs[1]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        //mDepthStencil->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        //mSandboxFramework->ResourceBarriersEnd(mBarriers, commandList);

        CD3DX12_RESOURCE_BARRIER transitionDepth = CD3DX12_RESOURCE_BARRIER::Transition(mSandboxFramework->GetDepthStencil(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COPY_SOURCE);
        commandListDXR->ResourceBarrier(1, &transitionDepth);

        commandList->CopyResource(mDepthStencil->GetResource(), mSandboxFramework->GetDepthStencil());

        commandListDXR->SetComputeRootSignature(mGlobalRaytracingRootSignature.Get());
        commandListDXR->SetComputeRootConstantBufferView(0, mCameraBuffer->GetResource()->GetGPUVirtualAddress());
        commandListDXR->SetComputeRootConstantBufferView(1, mLightsInfoCB->GetResource()->GetGPUVirtualAddress());
        
        mSandboxFramework->ResourceBarriersBegin(mBarriers);
        mLightingRTs[0]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        mSandboxFramework->ResourceBarriersEnd(mBarriers, commandList);

        // RT dispatch
        D3D12_DISPATCH_RAYS_DESC desc = {};

        // The ray generation shaders are always at the beginning of the SBT.
        uint32_t rayGenerationSectionSizeInBytes = mRaytracingShaderBindingTableHelper.GetRayGenSectionSize();
        desc.RayGenerationShaderRecord.StartAddress = mRaytracingShaderTableBuffer->GetGPUVirtualAddress();
        desc.RayGenerationShaderRecord.SizeInBytes = rayGenerationSectionSizeInBytes;

        uint32_t missSectionSizeInBytes = mRaytracingShaderBindingTableHelper.GetMissSectionSize();
        desc.MissShaderTable.StartAddress = mRaytracingShaderTableBuffer->GetGPUVirtualAddress() + rayGenerationSectionSizeInBytes;
        desc.MissShaderTable.SizeInBytes = missSectionSizeInBytes;
        desc.MissShaderTable.StrideInBytes = mRaytracingShaderBindingTableHelper.GetMissEntrySize();

        uint32_t hitGroupsSectionSize = mRaytracingShaderBindingTableHelper.GetHitGroupSectionSize();
        desc.HitGroupTable.StartAddress = mRaytracingShaderTableBuffer->GetGPUVirtualAddress() + rayGenerationSectionSizeInBytes + missSectionSizeInBytes;
        desc.HitGroupTable.SizeInBytes = hitGroupsSectionSize;
        desc.HitGroupTable.StrideInBytes = mRaytracingShaderBindingTableHelper.GetHitGroupEntrySize();

        // Dimensions of the image to render, identical to a kernel launch dimension
        auto size = mSandboxFramework->GetOutputSize();
        desc.Width = float(size.right);
        desc.Height = float(size.bottom);
        desc.Depth = 1;

        // Bind the raytracing pipeline
        commandListDXR->SetPipelineState1(mRaytracingPSO.Get());

        // Dispatch the rays for reflections and write to the raytracing output
        commandListDXR->DispatchRays(&desc);

        // Dispatch the rays for shadows and write to the raytracing output
        desc.RayGenerationShaderRecord.StartAddress = mRaytracingShaderTableBuffer->GetGPUVirtualAddress() + rayGenerationSectionSizeInBytes / 2; //offset to shadow raygen shader
        commandListDXR->DispatchRays(&desc);

		CD3DX12_RESOURCE_BARRIER transitionDepthBack = CD3DX12_RESOURCE_BARRIER::Transition(mSandboxFramework->GetDepthStencil(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		commandListDXR->ResourceBarrier(1, &transitionDepthBack);
    }

    commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // composite pass
    {
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandlesFinal[] =
        {
             mSandboxFramework->GetRenderTargetView()
        };
        
        commandList->OMSetRenderTargets(_countof(rtvHandlesFinal), rtvHandlesFinal, FALSE, nullptr);
        commandList->SetPipelineState(mCompositePSO.GetPipelineStateObject());
        commandList->SetGraphicsRootSignature(mCompositeRS.GetSignature());
        
		mSandboxFramework->ResourceBarriersBegin(mBarriers);
		mLightingRTs[0]->TransitionTo(mBarriers, commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		mSandboxFramework->ResourceBarriersEnd(mBarriers, commandList);

        DXRS::DescriptorHandle srvHandleComposite = gpuDescriptorHeap->GetHandleBlock(2);
        gpuDescriptorHeap->AddToHandle(device, srvHandleComposite, mLightingRTs[0]->GetSRV());
        gpuDescriptorHeap->AddToHandle(device, srvHandleComposite, mLightingRTs[1]->GetSRV());
        
        commandList->SetGraphicsRootDescriptorTable(0, srvHandleComposite.GetGPUHandle());
        commandList->IASetVertexBuffers(0, 1, &mSandboxFramework->GetFullscreenQuadBufferView());
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        commandList->DrawInstanced(4, 1, 0, 0);
    }

    //draw imgui 
    {
        ID3D12DescriptorHeap* ppHeaps[] = { mUIDescriptorHeap.Get() };
        commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

        ImGui::Render();
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
    }

    // Show the new frame.
    mSandboxFramework->Present();
    mGraphicsMemory->Commit(mSandboxFramework->GetCommandQueue());
}

// *** UPDATES *** //
void DXRSExampleRTScene::Update(DXRSTimer const& timer)
{
    UpdateControls();
    UpdateCamera();

    mWorld = XMMatrixIdentity();

    float width = mSandboxFramework->GetOutputSize().right;
    float height = mSandboxFramework->GetOutputSize().bottom;
    GBufferCBData gbufferPassData;
    gbufferPassData.ViewProjection = mCameraView * mCameraProjection;
    gbufferPassData.InvViewProjection = XMMatrixInverse(nullptr, gbufferPassData.ViewProjection);
    gbufferPassData.MipBias = 0.0f;
    gbufferPassData.CameraPos = XMFLOAT4(mCameraEye.x, mCameraEye.y, mCameraEye.z, 1);
    gbufferPassData.RTSize = { width, height, 1.0f / width, 1.0f / height };
    memcpy(mGbufferCB->Map(), &gbufferPassData, sizeof(gbufferPassData));

    LightingCBData lightPassData = {};
    lightPassData.InvViewProjection = XMMatrixInverse(nullptr, gbufferPassData.ViewProjection);
    lightPassData.CameraPos = XMFLOAT4(mCameraEye.x, mCameraEye.y, mCameraEye.z, 1);
    lightPassData.RTSize = { width, height, 1.0f / width, 1.0f / height };
    memcpy(mLightingCB->Map(), &lightPassData, sizeof(lightPassData));

    UpdateLights();

    XMMATRIX local = mWorld * Matrix::CreateScale(0.3f, 0.3f, 0.3f) * Matrix::CreateTranslation(0, 0, 0.0f);
    mDragonModel->UpdateWorldMatrix(local);

    local = mWorld * Matrix::CreateScale(8.0f, 8.0f, 8.0f) * Matrix::CreateTranslation(0, 0, 0.0f) * Matrix::CreateRotationX(-3.14f / 2.0f);
    mPlaneModel->UpdateWorldMatrix(local);

    UpdateImGui();
}

void DXRSExampleRTScene::UpdateImGui()
{
    //capture mouse clicks
    ImGui::GetIO().MouseDown[0] = (GetKeyState(VK_LBUTTON) & 0x8000) != 0;
    ImGui::GetIO().MouseDown[1] = (GetKeyState(VK_RBUTTON) & 0x8000) != 0;
    ImGui::GetIO().MouseDown[2] = (GetKeyState(VK_MBUTTON) & 0x8000) != 0;

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    {
        ImGui::Begin("DirectX Raytracing Sandbox");
        ImGui::TextColored(ImVec4(0.95f, 0.5f, 0.0f, 1), "FPS: (%.1f FPS), %.3f ms/frame", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
        ImGui::SliderFloat4("Light Color", mDirectionalLightColor, 0.0f, 1.0f);
        ImGui::SliderFloat4("Light Direction", mDirectionalLightDir, -1.0f, 1.0f);
        ImGui::SliderFloat("Light Intensity", &mDirectionalLightIntensity, 0.0f, 5.0f);
        ImGui::SliderFloat("Shadow Intensity", &mDirectionalLightShadowIntensity, 0.0f, 1.0f);
        ImGui::SliderFloat("Orbit camera radius", &mCameraRadius, 5.0f, 50.0f);
        ImGui::End();
    }
}

void DXRSExampleRTScene::UpdateLights()
{
    XMVECTOR skyColour = XMVectorSet(0.9f, 0.8f, 1.0f, 0.0f);

    //TODO move from update
    LightsInfoCBData lightData = {};
    DirectionalLightData dirLight = {};
    dirLight.Color = XMFLOAT4(mDirectionalLightColor[0], mDirectionalLightColor[1], mDirectionalLightColor[2], mDirectionalLightColor[3]);
    dirLight.Direction = XMFLOAT4(mDirectionalLightDir[0], mDirectionalLightDir[1], mDirectionalLightDir[2], mDirectionalLightDir[3]);
    dirLight.Intensity = mDirectionalLightIntensity;
    dirLight.ShadowIntensity = mDirectionalLightShadowIntensity;

    lightData.DirectionalLight.Color = dirLight.Color;
    lightData.DirectionalLight.Direction = dirLight.Direction;
    lightData.DirectionalLight.Intensity = dirLight.Intensity;
    lightData.DirectionalLight.ShadowIntensity = dirLight.ShadowIntensity;

    memcpy(mLightsInfoCB->Map(), &lightData, sizeof(lightData));
}

void DXRSExampleRTScene::UpdateControls()
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

void DXRSExampleRTScene::UpdateCamera()
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

    buffer.view         = mCameraView;
    buffer.projection   = mCameraProjection;
    buffer.viewI        = XMMatrixInverse(nullptr, mCameraView);
    buffer.projectionI  = XMMatrixInverse(nullptr, mCameraProjection);
    buffer.camToWorld   = XMMatrixTranspose(XMMatrixInverse(nullptr, mCameraProjection * mCameraView));
    buffer.camPosition  = XMFLOAT4(mCameraEye.x, mCameraEye.y, mCameraEye.z, 1.0f);
    buffer.resolution   = XMFLOAT2(width, height);

    // Copy the contents
    memcpy(mCameraBuffer->Map(), &buffer, sizeof(CameraBuffer));
}

void DXRSExampleRTScene::SetProjectionMatrix()
{
    auto size = mSandboxFramework->GetOutputSize();
    float aspectRatio = float(size.right) / float(size.bottom);
    float fovAngleY = 60.0f * XM_PI / 180.0f;

    if (aspectRatio < 1.0f)
        fovAngleY *= 2.0f;

    mCameraProjection = Matrix::CreatePerspectiveFieldOfView(fovAngleY, aspectRatio, 1.0f, 10000.0f);
}

void DXRSExampleRTScene::OnWindowSizeChanged(int width, int height)
{
    if (!mSandboxFramework->WindowSizeChanged(width, height))
        return;

    SetProjectionMatrix();
}

