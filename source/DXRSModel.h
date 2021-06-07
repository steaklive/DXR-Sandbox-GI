#pragma once

#include "Common.h"
#include "DXRSGraphics.h"
#include "DXRSMesh.h"
#include "DXRSModelMaterial.h"
#include "DXRSBuffer.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <map>

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>
using namespace Microsoft::WRL;
using namespace DirectX;


class DXRSModel
{
	__declspec(align(16)) struct ModelConstantBuffer
	{
		XMMATRIX	World;
		XMFLOAT4	DiffuseColor;
	};

public:
	DXRSModel(DXRSGraphics& dxWrapper, const std::string& filename, bool flipUVs = false, XMMATRIX tranformWorld = XMMatrixIdentity(), XMFLOAT4 color = XMFLOAT4(1, 0, 1, 1), bool isDynamic = false, float speed = 0.0f, float amplitude = 1.0f);
	~DXRSModel();

	void UpdateWorldMatrix(XMMATRIX matrix);

	DXRSGraphics& GetDXWrapper();

	DXRSBuffer* GetCB() { return mBufferCB; }
	bool HasMeshes() const;
	bool HasMaterials() const;

	void Render(ID3D12GraphicsCommandList* commandList);

	const std::vector<DXRSMesh*>& Meshes() const;
	const std::vector<DXRSModelMaterial*>& Materials() const;
	const std::string GetFileName() { return mFilename; }
	const char* GetFileNameChar() { return mFilename.c_str(); }
	std::vector<XMFLOAT3> GenerateAABB();
	XMMATRIX GetWorldMatrix() { return mWorldMatrix; }
	XMFLOAT3 GetTranslation();

	void SetBlasBuffer(DXRSBuffer* buffer) { mBLASBuffer = buffer; }
	DXRSBuffer* GetBlasBuffer() { return mBLASBuffer; }

	XMFLOAT4 GetDiffuseColor() { return mDiffuseColor; }
	bool GetIsDynamic() { return mIsDynamic; }
	float GetSpeed() { return mSpeed; }
	float GetAmplitude() { return mAmplitude; }
private:
	DXRSModel(const DXRSModel& rhs);
	DXRSModel& operator=(const DXRSModel& rhs);

	DXRSGraphics& mDXWrapper;

	DXRSBuffer* mBufferCB;

	std::vector<DXRSMesh*> mMeshes;
	std::vector<DXRSModelMaterial*> mMaterials;
	std::string mFilename;

	XMFLOAT4 mDiffuseColor;
	DXRSBuffer* mBLASBuffer = nullptr;
	XMMATRIX mWorldMatrix = XMMatrixIdentity();
	bool mIsDynamic;
	float mSpeed;
	float mAmplitude;
};

