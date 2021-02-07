#include "DescriptorHeap.h"
#include "DXRSGraphics.h"

namespace DXRS
{
	DescriptorHeap::DescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool isReferencedByShader)
		: mHeapType(heapType)
		, mMaxNumDescriptors(numDescriptors)
		, mIsReferencedByShader(isReferencedByShader)
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
		heapDesc.NumDescriptors = mMaxNumDescriptors;
		heapDesc.Type = mHeapType;
		heapDesc.Flags = mIsReferencedByShader ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		heapDesc.NodeMask = 0;

		ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mDescriptorHeap)));

		mDescriptorHeapCPUStart = mDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

		if (mIsReferencedByShader)
		{
			mDescriptorHeapGPUStart = mDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		}

		mDescriptorSize = device->GetDescriptorHandleIncrementSize(mHeapType);
	}

	DescriptorHeap::~DescriptorHeap()
	{
	}

	CPUDescriptorHeap::CPUDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors)
		: DescriptorHeap(device, heapType, numDescriptors, false)
	{
		mCurrentDescriptorIndex = 0;
		mActiveHandleCount = 0;
	}

	CPUDescriptorHeap::~CPUDescriptorHeap()
	{
		mFreeDescriptors.clear();
	}

	DescriptorHandle CPUDescriptorHeap::GetNewHandle()
	{
		UINT newHandleID = 0;

		if (mCurrentDescriptorIndex < mMaxNumDescriptors)
		{
			newHandleID = mCurrentDescriptorIndex;
			mCurrentDescriptorIndex++;
		}
		else if (mFreeDescriptors.size() > 0)
		{
			newHandleID = mFreeDescriptors.back();
			mFreeDescriptors.pop_back();
		}
		else
		{
			std::exception("Ran out of dynamic descriptor heap handles, need to increase heap size.");
		}

		DescriptorHandle newHandle;
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = mDescriptorHeapCPUStart;
		cpuHandle.ptr += newHandleID * mDescriptorSize;
		newHandle.SetCPUHandle(cpuHandle);
		newHandle.SetHeapIndex(newHandleID);
		mActiveHandleCount++;

		return newHandle;
	}

	void CPUDescriptorHeap::FreeHandle(DescriptorHandle handle)
	{
		mFreeDescriptors.push_back(handle.GetHeapIndex());

		if (mActiveHandleCount == 0)
		{
			std::exception("Freeing heap handles when there should be none left");
		}
		mActiveHandleCount--;
	}

	GPUDescriptorHeap::GPUDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors)
		: DescriptorHeap(device, heapType, numDescriptors, true)
	{
		mCurrentDescriptorIndex = 0;
	}

	DescriptorHandle GPUDescriptorHeap::GetHandleBlock(UINT count)
	{
		UINT newHandleID = 0;
		UINT blockEnd = mCurrentDescriptorIndex + count;

		if (blockEnd < mMaxNumDescriptors)
		{
			newHandleID = mCurrentDescriptorIndex;
			mCurrentDescriptorIndex = blockEnd;
		}
		else
		{
			std::exception("Ran out of GPU descriptor heap handles, need to increase heap size.");
		}

		DescriptorHandle newHandle;
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = mDescriptorHeapCPUStart;
		cpuHandle.ptr += newHandleID * mDescriptorSize;
		newHandle.SetCPUHandle(cpuHandle);

		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = mDescriptorHeapGPUStart;
		gpuHandle.ptr += newHandleID * mDescriptorSize;
		newHandle.SetGPUHandle(gpuHandle);

		newHandle.SetHeapIndex(newHandleID);

		return newHandle;
	}

	void GPUDescriptorHeap::Reset()
	{
		mCurrentDescriptorIndex = 0;
	}

	DescriptorHeapManager::DescriptorHeapManager(ID3D12Device* device)
	{
		ZeroMemory(mCPUDescriptorHeaps, sizeof(mCPUDescriptorHeaps));

		static const int MaxNoofSRVDescriptors = 4 * 4096;

		mCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = new CPUDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MaxNoofSRVDescriptors);
		mCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV] = new CPUDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 128);
		mCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV] = new CPUDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 128);
		mCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = new CPUDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 16);

		for (UINT i = 0; i < DXRSGraphics::MAX_BACK_BUFFER_COUNT; i++)
		{
			ZeroMemory(mGPUDescriptorHeaps[i], sizeof(mGPUDescriptorHeaps[i]));
			mGPUDescriptorHeaps[i][D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = new GPUDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MaxNoofSRVDescriptors);
			mGPUDescriptorHeaps[i][D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = new GPUDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 16);
		}
	}

	DescriptorHeapManager::~DescriptorHeapManager()
	{
		for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++)
		{
			if (mCPUDescriptorHeaps[i])
				delete mCPUDescriptorHeaps[i];

			for (UINT j = 0; j < DXRSGraphics::MAX_BACK_BUFFER_COUNT; j++)
			{
				if (mGPUDescriptorHeaps[j][i])
					delete mGPUDescriptorHeaps[j][i];
			}
		}
	}

	DescriptorHandle DescriptorHeapManager::CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE heapType)
	{
		const UINT currentFrame = DXRSGraphics::mBackBufferIndex;

		return mCPUDescriptorHeaps[heapType]->GetNewHandle();
	}

	DescriptorHandle DescriptorHeapManager::CreateGPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT count)
	{
		const UINT currentFrame = DXRSGraphics::mBackBufferIndex;

		return mGPUDescriptorHeaps[currentFrame][heapType]->GetHandleBlock(count);
	}
}