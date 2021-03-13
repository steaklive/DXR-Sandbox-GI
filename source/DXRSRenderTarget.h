#pragma once
#include "Common.h"
#include "DescriptorHeap.h"

class DXRSRenderTarget
{
public:
	DXRSRenderTarget(ID3D12Device* device, DXRS::DescriptorHeapManager* descriptorManger, int width, int height, DXGI_FORMAT aFormat, D3D12_RESOURCE_FLAGS flags, LPCWSTR name, int depth = -1);
	~DXRSRenderTarget();

	ID3D12Resource* GetResource() { return mRenderTarget.Get(); }

	int GetWidth() { return mWidth; }
	int GetHeight() { return mHeight; }
	int GetDepth() { return mDepth; }
	void TransitionTo(std::vector<CD3DX12_RESOURCE_BARRIER>& barriers, ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES stateAfter);
	//void SetCurrentState(D3D12_RESOURCE_STATES state) { mCurrentResourceState = state; }

	DXRS::DescriptorHandle& GetRTV()
	{
		return mDescriptorRTV;
	}

	DXRS::DescriptorHandle& GetSRV()
	{
		return mDescriptorSRV;
	}

	DXRS::DescriptorHandle& GetUAV()
	{
		return mDescriptorUAV;
	}

private:

	int mWidth, mHeight, mDepth;
	DXGI_FORMAT mFormat;
	D3D12_RESOURCE_STATES mCurrentResourceState;

	DXRS::DescriptorHandle mDescriptorUAV;
	DXRS::DescriptorHandle mDescriptorSRV;
	DXRS::DescriptorHandle mDescriptorRTV;
	ComPtr<ID3D12Resource> mRenderTarget;
};