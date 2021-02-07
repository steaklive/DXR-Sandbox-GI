#pragma once

#include "Common.h"
#include "DescriptorHeap.h"

struct aiMesh;
class DXRSModel;
class DXRSModelMaterial;
class DXRSBuffer;

class DXRSMesh
{
public:
	struct Vertex
	{
		XMFLOAT3 position;
		XMFLOAT3 normal;
		XMFLOAT3 tangent;
		XMFLOAT2 texcoord;
	};

	struct MeshInfo
	{
		XMFLOAT4 color;
	};

	DXRSMesh(ID3D12Device* device, DXRSModel& model, DXRSModelMaterial* material);
	DXRSMesh(ID3D12Device* device, DXRSModel& model, aiMesh& mesh);
	~DXRSMesh();

	DXRSModel& GetModel();
	DXRSModelMaterial* GetMaterial();
	const std::string& Name() const;

	ID3D12Resource* GetVertexBuffer() { return mVertexBuffer.Get(); }
	ID3D12Resource* GetIndexBuffer() { return mIndexBuffer.Get(); }

	D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() { return mVertexBufferView; }
	D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() { return mIndexBufferView; }

	const std::vector<XMFLOAT3>& Vertices() const;
	const std::vector<XMFLOAT3>& Normals() const;
	const std::vector<XMFLOAT3>& Tangents() const;
	const std::vector<XMFLOAT3>& BiNormals() const;
	const std::vector<std::vector<XMFLOAT3>*>& TextureCoordinates() const;
	const std::vector<std::vector<XMFLOAT4>*>& VertexColors() const;
	UINT FaceCount() const;
	const std::vector<UINT>& Indices() const;

	UINT GetIndicesNum() { return mIndices.size(); }

	void CreateVertexBuffer(ComPtr<ID3D12Resource> vertexBuffer, ComPtr<ID3D12Resource> vertexBufferUploadHeap);
	void CreateIndexBuffer(ComPtr<ID3D12Resource> indexBuffer, ComPtr<ID3D12Resource> indexBufferUploadHeap);

	DXRS::DescriptorHandle& GetIndexBufferSRV() { return mIndexBufferSRV; }
	DXRS::DescriptorHandle& GetVertexBufferSRV() { return mVertexBufferSRV; }

	DXRSBuffer* GetMeshInfoBuffer() { return mMeshInfo; }
		
private:

	DXRSMesh(const DXRSMesh& rhs);
	DXRSMesh& operator=(const DXRSMesh& rhs);

	DXRSModel& mModel;
	DXRSModelMaterial* mMaterial;

	std::string mName;

	std::vector<Vertex> mVertices;

	std::vector<XMFLOAT3> mPositions;
	std::vector<XMFLOAT3> mNormals;
	std::vector<XMFLOAT3> mTangents;
	std::vector<XMFLOAT3> mBiNormals;
	std::vector<std::vector<XMFLOAT3>*> mTextureCoordinates;
	std::vector<std::vector<XMFLOAT4>*> mVertexColors;
	UINT mFaceCount;
	std::vector<UINT> mIndices;

	UINT mNumOfIndices;

	ComPtr<ID3D12Resource> mVertexBuffer;
	ComPtr<ID3D12Resource> mIndexBuffer;

	D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;
	D3D12_INDEX_BUFFER_VIEW mIndexBufferView;

	DXRS::DescriptorHandle mIndexBufferSRV;
	DXRS::DescriptorHandle mVertexBufferSRV;

	DXRSBuffer* mMeshInfo;
};
