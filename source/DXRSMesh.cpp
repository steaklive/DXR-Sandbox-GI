#define NOMINMAX

#include "DXRSMesh.h"
#include "DXRSModel.h"
#include <assimp/scene.h>

DXRSMesh::DXRSMesh(ID3D12Device* device, DXRSModel& model, aiMesh& mesh)
	: mModel(model), mMaterial(nullptr), mName(mesh.mName.C_Str()), mPositions(), mNormals(), mTangents(), mBiNormals(), mTextureCoordinates(), mVertexColors(), mFaceCount(0), mIndices()
{
	mMaterial = mModel.Materials().at(mesh.mMaterialIndex);

	// Vertices
	mPositions.reserve(mesh.mNumVertices);
	for (UINT i = 0; i < mesh.mNumVertices; i++)
	{
		mPositions.push_back(XMFLOAT3(reinterpret_cast<const float*>(&mesh.mVertices[i])));
	}

	// Normals
	if (mesh.HasNormals())
	{
		mNormals.reserve(mesh.mNumVertices);
		for (UINT i = 0; i < mesh.mNumVertices; i++)
		{
			mNormals.push_back(XMFLOAT3(reinterpret_cast<const float*>(&mesh.mNormals[i])));
		}
	}

	// Tangents and Binormals
	if (mesh.HasTangentsAndBitangents())
	{
		mTangents.reserve(mesh.mNumVertices);
		mBiNormals.reserve(mesh.mNumVertices);
		for (UINT i = 0; i < mesh.mNumVertices; i++)
		{
			mTangents.push_back(XMFLOAT3(reinterpret_cast<const float*>(&mesh.mTangents[i])));
			mBiNormals.push_back(XMFLOAT3(reinterpret_cast<const float*>(&mesh.mBitangents[i])));
		}
	}

	// Texture Coordinates
	UINT uvChannelCount = mesh.GetNumUVChannels();
	for (UINT i = 0; i < uvChannelCount; i++)
	{
		std::vector<XMFLOAT3>* textureCoordinates = new std::vector<XMFLOAT3>();
		textureCoordinates->reserve(mesh.mNumVertices);
		mTextureCoordinates.push_back(textureCoordinates);

		aiVector3D* aiTextureCoordinates = mesh.mTextureCoords[i];
		for (UINT j = 0; j < mesh.mNumVertices; j++)
		{
			textureCoordinates->push_back(XMFLOAT3(reinterpret_cast<const float*>(&aiTextureCoordinates[j])));
		}
	}

	// Vertex Colors
	UINT colorChannelCount = mesh.GetNumColorChannels();
	for (UINT i = 0; i < colorChannelCount; i++)
	{
		std::vector<XMFLOAT4>* vertexColors = new std::vector<XMFLOAT4>();
		vertexColors->reserve(mesh.mNumVertices);
		mVertexColors.push_back(vertexColors);

		aiColor4D* aiVertexColors = mesh.mColors[i];
		for (UINT j = 0; j < mesh.mNumVertices; j++)
		{
			vertexColors->push_back(XMFLOAT4(reinterpret_cast<const float*>(&aiVertexColors[j])));
		}
	}

	// Faces (note: could pre-reserve if we limit primitive types)
	if (mesh.HasFaces())
	{
		mFaceCount = mesh.mNumFaces;
		for (UINT i = 0; i < mFaceCount; i++)
		{
			aiFace* face = &mesh.mFaces[i];

			for (UINT j = 0; j < face->mNumIndices; j++)
			{
				mIndices.push_back(face->mIndices[j]);
			}
		}
	}

	mNumOfIndices = static_cast<UINT>(mIndices.size());

	for (size_t i = 0; i < mesh.mNumVertices; i++)
	{
		XMFLOAT3 uv = mTextureCoordinates.at(0)->at(i);

		Vertex vertex
		{
			mPositions[i],
			mNormals[i],
			mTangents[i],
			XMFLOAT2(uv.x, uv.y)
		};
		mVertices.push_back(vertex);
	}

	const UINT vertexBufferSize = static_cast<UINT>(mVertices.size()) * sizeof(Vertex);
	const UINT indexBufferSize = mNumOfIndices * sizeof(mIndices[0]);

	// Note: using upload heaps to transfer static data like vert buffers is not 
	// recommended. Every time the GPU needs it, the upload heap will be marshalled 
	// over. Please read up on Default Heap usage. An upload heap is used here for 
	// code simplicity and because there are very few verts to actually transfer.
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mVertexBuffer)));

	// Copy the triangle data to the vertex buffer.
	UINT8* pVertexDataBegin;
	CD3DX12_RANGE readRange(0, 0);

	ThrowIfFailed(mVertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
	memcpy(pVertexDataBegin, &mVertices[0], vertexBufferSize);
	mVertexBuffer->Unmap(0, nullptr);

	// Initialize the vertex buffer view.
	mVertexBufferView.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
	mVertexBufferView.StrideInBytes = sizeof(Vertex);
	mVertexBufferView.SizeInBytes = vertexBufferSize;

	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mIndexBuffer)));

	// Copy the triangle data to the vertex buffer.
	UINT8* pIndexDataBegin;

	ThrowIfFailed(mIndexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin)));
	memcpy(pIndexDataBegin, &mIndices[0], indexBufferSize);
	mIndexBuffer->Unmap(0, nullptr);

	// Initialize the vertex buffer view.
	mIndexBufferView.BufferLocation = mIndexBuffer->GetGPUVirtualAddress();
	mIndexBufferView.Format = DXGI_FORMAT_R32_UINT/*DXGI_FORMAT_R16_UINT*/;
	mIndexBufferView.SizeInBytes = indexBufferSize;

	auto descriptorManager =  mModel.GetDXWrapper().GetDescriptorHeapManager();

	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	SRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.Buffer.NumElements = mNumOfIndices;
	SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
	mIndexBufferSRV = descriptorManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	device->CreateShaderResourceView(mIndexBuffer.Get(), &SRVDesc, mIndexBufferSRV.GetCPUHandle());

	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDescVB = {};
	SRVDescVB.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	SRVDescVB.Format = DXGI_FORMAT_UNKNOWN;
	SRVDescVB.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDescVB.Buffer.NumElements = mVertices.size();
	SRVDescVB.Buffer.StructureByteStride = sizeof(Vertex);
	SRVDescVB.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	mVertexBufferSRV = descriptorManager->CreateCPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	device->CreateShaderResourceView(mVertexBuffer.Get(), &SRVDescVB, mVertexBufferSRV.GetCPUHandle());

	//create constant buffer for mehs info
	DXRSBuffer::Description cbDesc;
	cbDesc.mElementSize = sizeof(MeshInfo);
	cbDesc.mState = D3D12_RESOURCE_STATE_GENERIC_READ;
	cbDesc.mDescriptorType = DXRSBuffer::DescriptorType::CBV;
	mMeshInfo = new DXRSBuffer(mModel.GetDXWrapper().GetD3DDevice(), descriptorManager, mModel.GetDXWrapper().GetCommandListGraphics(), cbDesc, L"Mesh Info CB");
	
	MeshInfo buffer;
	buffer.color = mModel.GetDiffuseColor(); // for now just color from the model

	// Copy the contents
	memcpy(mMeshInfo->Map(), &buffer, sizeof(MeshInfo));
}

DXRSMesh::~DXRSMesh()
{
	for (std::vector<XMFLOAT3>* textureCoordinates : mTextureCoordinates)
	{
		delete textureCoordinates;
	}

	for (std::vector<XMFLOAT4>* vertexColors : mVertexColors)
	{
		delete vertexColors;
	}
}

DXRSModel& DXRSMesh::GetModel()
{
	return mModel;
}

DXRSModelMaterial* DXRSMesh::GetMaterial()
{
	return mMaterial;
}

const std::string& DXRSMesh::Name() const
{
	return mName;
}

const std::vector<XMFLOAT3>& DXRSMesh::Vertices() const
{
	return mPositions;
}

const std::vector<XMFLOAT3>& DXRSMesh::Normals() const
{
	return mNormals;
}

const std::vector<XMFLOAT3>& DXRSMesh::Tangents() const
{
	return mTangents;
}

const std::vector<XMFLOAT3>& DXRSMesh::BiNormals() const
{
	return mBiNormals;
}

const std::vector<std::vector<XMFLOAT3>*>& DXRSMesh::TextureCoordinates() const
{
	return mTextureCoordinates;
}

const std::vector<std::vector<XMFLOAT4>*>& DXRSMesh::VertexColors() const
{
	return mVertexColors;
}

UINT DXRSMesh::FaceCount() const
{
	return mFaceCount;
}

const std::vector<UINT>& DXRSMesh::Indices() const
{
	return mIndices;
}


void DXRSMesh::CreateVertexBuffer(ComPtr<ID3D12Resource> vertexBuffer, ComPtr<ID3D12Resource> vertexBufferUploadHeap)
{
	int vBufferSize = sizeof(mPositions);

	mModel.GetDXWrapper().GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // a default heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(vBufferSize), // resource description for a buffer
		D3D12_RESOURCE_STATE_COPY_DEST, // we will start this heap in the copy destination state since we will copy data
										// from the upload heap to this heap
		nullptr, // optimized clear value must be null for this type of resource. used for render targets and depth/stencil buffers
		IID_PPV_ARGS(&vertexBuffer));

	// we can give resource heaps a name so when we debug with the graphics debugger we know what resource we are looking at
	vertexBuffer->SetName(L"Vertex Buffer Resource Heap");

	mModel.GetDXWrapper().GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(vBufferSize), // resource description for a buffer
		D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
		nullptr,
		IID_PPV_ARGS(&vertexBufferUploadHeap));
	vertexBufferUploadHeap->SetName(L"Vertex Buffer Upload Resource Heap");


}
void DXRSMesh::CreateIndexBuffer(ComPtr<ID3D12Resource> indexBuffer, ComPtr<ID3D12Resource> indexBufferUploadHeap)
{
	// create default heap to hold index buffer
	mModel.GetDXWrapper().GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // a default heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(mIndices.size()), // resource description for a buffer
		D3D12_RESOURCE_STATE_COPY_DEST, // start in the copy destination state
		nullptr, // optimized clear value must be null for this type of resource
		IID_PPV_ARGS(&indexBuffer));

	indexBuffer->SetName(L"Index Buffer Resource Heap");

	// create upload heap to upload index buffer
	mModel.GetDXWrapper().GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(mIndices.size()), // resource description for a buffer
		D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
		nullptr,
		IID_PPV_ARGS(&indexBufferUploadHeap));
	indexBufferUploadHeap->SetName(L"Index Buffer Upload Resource Heap");



}