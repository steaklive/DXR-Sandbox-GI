#pragma once
#include "Common.h"
#include "DescriptorHeap.h"

class DXRSDepthBuffer
{
public:
	DXRSDepthBuffer(ID3D12Device* device, DXRS::DescriptorHeapManager* descriptorManager, int width, int height, DXGI_FORMAT format);
	~DXRSDepthBuffer();

	ID3D12Resource* GetResource() { return mDepthStencilResource.Get(); }
	DXGI_FORMAT GetFormat() { return mFormat; }
	void TransitionTo(std::vector<CD3DX12_RESOURCE_BARRIER>& barriers, ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES stateAfter);

	DXRS::DescriptorHandle GetDSV()
	{
		return mDescriptorDSV;
	}

	DXRS::DescriptorHandle& GetSRV()
	{
		return mDescriptorSRV;
	}

	const int GetWidth() { return mWidth; }
	const int GetHeight() { return mHeight; }

private:

	int mWidth, mHeight;
	DXGI_FORMAT mFormat;
	D3D12_RESOURCE_STATES mCurrentResourceState;

	DXRS::DescriptorHandle mDescriptorDSV;
	DXRS::DescriptorHandle mDescriptorSRV;
	ComPtr<ID3D12Resource> mDepthStencilResource;
};