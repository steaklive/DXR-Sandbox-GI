#include "DXRSRenderTarget.h"

DXRSRenderTarget::DXRSRenderTarget(ID3D12Device* device, DXRS::DescriptorHeapManager* descriptorManager, int width, int height, DXGI_FORMAT aFormat, D3D12_RESOURCE_FLAGS flags, LPCWSTR name, int depth, int mips)
{
	mWidth = width;
	mHeight = height;
	mDepth = depth;
	mFormat = aFormat;

	XMFLOAT4 clearColor = { 0, 0, 0, 1 };
	DXGI_FORMAT format = aFormat;

	// Describe and create a Texture2D/3D
	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.MipLevels = mips;
	textureDesc.Format = format;
	textureDesc.Width = width;
	textureDesc.Height = height;
	textureDesc.DepthOrArraySize = (depth > 0) ? depth : 1;
	textureDesc.Flags = flags;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Dimension = (depth > 0) ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	D3D12_CLEAR_VALUE optimizedClearValue = {};
	optimizedClearValue.Format = format;
	optimizedClearValue.Color[0] = clearColor.x;
	optimizedClearValue.Color[1] = clearColor.y;
	optimizedClearValue.Color[2] = clearColor.z;
	optimizedClearValue.Color[3] = clearColor.w;

	mCurrentResourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		mCurrentResourceState,
		&optimizedClearValue,
		IID_PPV_ARGS(&mRenderTarget)));

	mRenderTarget->SetName(name);

	D3D12_DESCRIPTOR_HEAP_FLAGS flagsHeap = D3D12_DESCRIPTOR_HEAP_FLAGS::D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	// Describe and create a SRV for the texture.
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = aFormat;
	if (depth > 0) {
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Texture3D.MipLevels = mips;
	}
	else {
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = mips;
	}

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = aFormat;
	if (depth > 0) {
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
		rtvDesc.Texture3D.MipSlice = 0;
		rtvDesc.Texture3D.WSize = depth;
	}
	else {
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;
	}

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = aFormat;
	if (depth > 0) {
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		uavDesc.Texture3D.MipSlice = 0;
		uavDesc.Texture3D.WSize = depth;
	}
	else {
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;
	}

	mDescriptorRTV = descriptorManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	device->CreateRenderTargetView(mRenderTarget.Get(), &rtvDesc, mDescriptorRTV.GetCPUHandle());

	mDescriptorSRV = descriptorManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	device->CreateShaderResourceView(mRenderTarget.Get(), &srvDesc, mDescriptorSRV.GetCPUHandle());

	if (flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
	{
		mDescriptorUAV = descriptorManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		device->CreateUnorderedAccessView(mRenderTarget.Get(), nullptr, &uavDesc, mDescriptorUAV.GetCPUHandle());
	}
}

void DXRSRenderTarget::TransitionTo(std::vector<CD3DX12_RESOURCE_BARRIER>& barriers, ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES stateAfter)
{
	if (stateAfter != mCurrentResourceState)
	{
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(GetResource(), mCurrentResourceState, stateAfter));
		mCurrentResourceState = stateAfter;
	}
}