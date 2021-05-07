#pragma once
#include "Common.h"
#include "DescriptorHeap.h"

class DXRSRenderTarget
{
public:
	DXRSRenderTarget(ID3D12Device* device, DXRS::DescriptorHeapManager* descriptorManger, int width, int height, DXGI_FORMAT aFormat, D3D12_RESOURCE_FLAGS flags, LPCWSTR name, int depth = -1, int mips = 1);
	~DXRSRenderTarget();

	ID3D12Resource* GetResource() { return mRenderTarget.Get(); }

	int GetWidth() { return mWidth; }
	int GetHeight() { return mHeight; }
	int GetDepth() { return mDepth; }
	void TransitionTo(std::vector<CD3DX12_RESOURCE_BARRIER>& barriers, ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES stateAfter);
	void SetCurrentState(D3D12_RESOURCE_STATES state) { mCurrentResourceState = state; }

	DXRS::DescriptorHandle& GetRTV(int mip = 0)
	{
		return mDescriptorRTVMipsHandles[mip];
	}

	DXRS::DescriptorHandle& GetSRV()
	{
		return mDescriptorSRV;
	}

	DXRS::DescriptorHandle& GetUAV(int mip = 0)
	{
		return mDescriptorUAVMipsHandles[mip];
	}

private:

	int mWidth, mHeight, mDepth;
	DXGI_FORMAT mFormat;
	D3D12_RESOURCE_STATES mCurrentResourceState;

	//DXRS::DescriptorHandle mDescriptorUAV;
	DXRS::DescriptorHandle mDescriptorSRV;
	//DXRS::DescriptorHandle mDescriptorRTV;
	ComPtr<ID3D12Resource> mRenderTarget;

	std::vector<DXRS::DescriptorHandle> mDescriptorUAVMipsHandles;
	std::vector<DXRS::DescriptorHandle> mDescriptorRTVMipsHandles;
};