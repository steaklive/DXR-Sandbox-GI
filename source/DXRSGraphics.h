#pragma once

#define NOMINMAX
#include <Windows.h>
// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>
using namespace Microsoft::WRL;

// DirectX 12 specific headers.
#include <d3d12.h>
#include <dxgi1_6.h>
// D3D12 extension library.
#include <d3dx12.h>

#include <dxc/dxcapi.h>

#include "Common.h"

#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>

#define MAX_SCREEN_WIDTH 1920
#define MAX_SCREEN_HEIGHT 1080

namespace DXRS {
    class DescriptorHeapManager;
}

class DXRSGraphics
{
public:
    DXRSGraphics(DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT, UINT backBufferCount = 2, D3D_FEATURE_LEVEL minFeatureLevel = D3D_FEATURE_LEVEL_11_0, unsigned int flags = 0);
    ~DXRSGraphics();

    void CreateResources();
    void FinalizeResources();
    void CreateWindowResources();
    void CreateFullscreenQuadBuffers();
    void SetWindow(HWND window, int width, int height);
    bool WindowSizeChanged(int width, int height);
    void Prepare(D3D12_RESOURCE_STATES beforeState = D3D12_RESOURCE_STATE_PRESENT);
    void Present(D3D12_RESOURCE_STATES beforeState = D3D12_RESOURCE_STATE_RENDER_TARGET);
    void WaitForGpu();

    ID3D12Device*               GetD3DDevice() const { return mDevice.Get(); }
    ID3D12Device5*              GetDXRDevice() const { return (ID3D12Device5*)(mDevice.Get());}
    IDXGISwapChain3*            GetSwapChain() const { return mSwapChain.Get(); }
    IDXGIFactory4*              GetDXGIFactory() const { return mDXGIFactory.Get(); }
    D3D_FEATURE_LEVEL           GetDeviceFeatureLevel() const { return mD3DFeatureLevel; }
    
    ID3D12CommandQueue*         GetCommandQueue() const { return mCommandQueue.Get(); }
    ID3D12CommandAllocator*     GetCommandAllocator() const { return mCommandAllocators[mBackBufferIndex].Get(); }
    ID3D12GraphicsCommandList*  GetCommandList() const { return mCommandList.Get(); }
   
    DXGI_FORMAT                 GetBackBufferFormat() const { return mBackBufferFormat; }
    DXGI_FORMAT                 GetDepthBufferFormat() const { return mDepthBufferFormat; }
    D3D12_VIEWPORT              GetScreenViewport() const { return mScreenViewport; }
    D3D12_RECT                  GetScissorRect() const { return mScissorRect; }
    UINT                        GetCurrentFrameIndex() const { return mBackBufferIndex; }
    UINT                        GetBackBufferCount() const { return mBackBufferCount; }
    RECT                        GetOutputSize() const { return mOutputSize; }

    ID3D12Resource*             GetRenderTarget() const { return mRenderTargets[mBackBufferIndex].Get(); }
    ID3D12Resource*             GetDepthStencil() const { return mDepthStencilTarget.Get(); }
    inline CD3DX12_CPU_DESCRIPTOR_HANDLE GetRenderTargetView() const { return CD3DX12_CPU_DESCRIPTOR_HANDLE(mRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(mBackBufferIndex), mRTVDescriptorSize); }
    inline CD3DX12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() const { return CD3DX12_CPU_DESCRIPTOR_HANDLE(mDSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart()); }

    D3D12_VERTEX_BUFFER_VIEW& GetFullscreenQuadBufferView() { return mFullscreenQuadVertexBufferView; }

    void ResourceBarriersBegin(std::vector<CD3DX12_RESOURCE_BARRIER>& barriers) { barriers.clear(); }
    void ResourceBarriersEnd(std::vector<CD3DX12_RESOURCE_BARRIER>& barriers, ID3D12GraphicsCommandList* commandList) {
        size_t num = barriers.size();
        if (num > 0)
            commandList->ResourceBarrier(num, barriers.data());
    }

    DXRS::DescriptorHeapManager* GetDescriptorHeapManager() { return mDescriptorHeapManager; }
    IDxcBlob* CompileShaderLibrary(LPCWSTR fileName);

    static const size_t                 MAX_BACK_BUFFER_COUNT = 3;
    static UINT                         mBackBufferIndex;

    std::string GetFilePath(const std::string& input);
    std::wstring GetFilePath(const std::wstring& input);

private:

    DXRSGraphics(const DXRSGraphics& rhs);
    DXRSGraphics& operator=(const DXRSGraphics& rhs);

    void MoveToNextFrame();
    void GetAdapter(IDXGIAdapter1** ppAdapter);
    
    ComPtr<IDXGIFactory4>               mDXGIFactory;
    ComPtr<IDXGISwapChain3>             mSwapChain;
    ComPtr<ID3D12Device>                mDevice;

    DXRS::DescriptorHeapManager*        mDescriptorHeapManager;

    ComPtr<ID3D12CommandQueue>          mCommandQueue;
    ComPtr<ID3D12GraphicsCommandList>   mCommandList;
    ComPtr<ID3D12CommandAllocator>      mCommandAllocators[MAX_BACK_BUFFER_COUNT];

    ComPtr<ID3D12Fence>                 mFence;
    UINT64                              mFenceValues[MAX_BACK_BUFFER_COUNT];
    Wrappers::Event                     mFenceEvent;

    ComPtr<ID3D12Resource>              mRenderTargets[MAX_BACK_BUFFER_COUNT];
    ComPtr<ID3D12Resource>              mDepthStencilTarget;
    ComPtr<ID3D12DescriptorHeap>        mRTVDescriptorHeap;
    ComPtr<ID3D12DescriptorHeap>        mDSVDescriptorHeap;
    UINT                                mRTVDescriptorSize;
    D3D12_VIEWPORT                      mScreenViewport;
    D3D12_RECT                          mScissorRect;

    DXGI_FORMAT                         mBackBufferFormat;
    DXGI_FORMAT                         mDepthBufferFormat;
    UINT                                mBackBufferCount;
    D3D_FEATURE_LEVEL                   mD3DMinimumFeatureLevel;

    HWND                                mAppWindow;
    RECT                                mOutputSize;
    D3D_FEATURE_LEVEL                   mD3DFeatureLevel;
    DWORD                               mDXGIFactoryFlags;

    // Fullscreen Quad
    ComPtr<ID3D12Resource>              mFullscreenQuadVertexBuffer;
    ComPtr<ID3D12Resource>              mFullscreenQuadVertexBufferUpload;
    D3D12_VERTEX_BUFFER_VIEW            mFullscreenQuadVertexBufferView;

    std::filesystem::path               mCurrentPath;
    std::wstring ExecutableDirectory();
};

