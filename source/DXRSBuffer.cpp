#include "DXRSBuffer.h"
#include <string>

DXRSBuffer::DXRSBuffer(ID3D12Device* device, DXRS::DescriptorHeapManager* descriptorManager, ID3D12GraphicsCommandList* commandList, Description& description, LPCWSTR name, unsigned char* data)
	: mDescription(description)
	, mData(data)
	, mCBVMappedData(nullptr)
{
	mBufferSize = mDescription.mNumElements * mDescription.mElementSize;

	if (mDescription.mDescriptorType & DescriptorType::CBV)
	{
		mBufferSize = Align(mBufferSize, 256);
	}

	CreateResources(device, descriptorManager, commandList);

	mBuffer->SetName(name);
}


void DXRSBuffer::CreateResources(ID3D12Device* device, DXRS::DescriptorHeapManager* descriptorManager, ID3D12GraphicsCommandList* commandList)
{
	D3D12_RESOURCE_DESC desc = {};
	desc.Alignment = mDescription.mAlignment;
	desc.DepthOrArraySize = 1;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Flags = mDescription.mResourceFlags;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.Height = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Width = (UINT64)mBufferSize;

	D3D12_HEAP_PROPERTIES heapProperties;
	heapProperties.Type = (mDescription.mDescriptorType & DescriptorType::CBV) ? D3D12_HEAP_TYPE_UPLOAD : mDescription.mHeapType;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	ThrowIfFailed(
		device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			mDescription.mState,
			nullptr,
			IID_PPV_ARGS(&mBuffer)
		)
	);

	if (mData)
	{
		// Create the GPU upload buffer.
		ThrowIfFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(mBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&mBufferUpload)));

		D3D12_SUBRESOURCE_DATA data = {};
		data.pData = mData;
		data.RowPitch = mBufferSize;
		data.SlicePitch = 0;

		UpdateSubresources(commandList, mBuffer.Get(), mBufferUpload.Get(), 0, 0, 1, &data);
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
	}
	D3D12_DESCRIPTOR_HEAP_FLAGS flagsHeap = D3D12_DESCRIPTOR_HEAP_FLAGS::D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	if (mDescription.mDescriptorType & DescriptorType::SRV)
	{
		mDescriptorSRV = descriptorManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		// Describe and create a SRV for the texture.
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = mDescription.mFormat;
		if (mDescription.mState == D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE)
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
			srvDesc.RaytracingAccelerationStructure.Location = mBuffer->GetGPUVirtualAddress();
			device->CreateShaderResourceView(nullptr, &srvDesc, mDescriptorSRV.GetCPUHandle());
		}
		else
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Buffer.NumElements = mDescription.mNumElements;
			srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			if (mDescription.mDescriptorType & DXRSBuffer::DescriptorType::Structured)
			{
				srvDesc.Buffer.StructureByteStride = mDescription.mElementSize;
			}
			else if (mDescription.mDescriptorType & DXRSBuffer::DescriptorType::Raw)
			{
				srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
				srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
			}
			device->CreateShaderResourceView(mBuffer.Get(), &srvDesc, mDescriptorSRV.GetCPUHandle());
		}
	}

	if (mDescription.mDescriptorType & DescriptorType::CBV)
	{
		mDescriptorCBV = descriptorManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		// Describe and create a constant buffer view.
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = mBuffer->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = mBufferSize;    // CB size is required to be 256-byte aligned.
		device->CreateConstantBufferView(&cbvDesc, mDescriptorCBV.GetCPUHandle());
	}

}

DXRSBuffer::~DXRSBuffer()
{
	if (mCBVMappedData)
		mBuffer->Unmap(0, nullptr);

	mCBVMappedData = nullptr;
}
