#pragma once

#include "Common.h"

#include <map>

struct aiMaterial;
class DXRSModel;

enum TextureType
{
	TextureTypeDifffuse = 0,
	TextureTypeSpecularMap,
	TextureTypeAmbient,
	TextureTypeEmissive,
	TextureTypeHeightmap,
	TextureTypeNormalMap,
	TextureTypeSpecularPowerMap,
	TextureTypeDisplacementMap,
	TextureTypeLightMap,
	TextureTypeEnd
};

class DXRSModelMaterial
{
public:
	DXRSModelMaterial(DXRSModel& model);
	DXRSModelMaterial(DXRSModel& model, aiMaterial* material);
	~DXRSModelMaterial();

	DXRSModel& GetModel();
	const std::string& Name() const;
	const std::map<TextureType, std::vector<std::wstring>*> Textures() const;
	std::vector<std::wstring>* GetTexturesByType(TextureType type);

private:
	static void InitializeTextureTypeMappings();
	static std::map<TextureType, UINT> sTextureTypeMappings;

	DXRSModelMaterial(const DXRSModelMaterial& rhs);
	DXRSModelMaterial& operator=(const DXRSModelMaterial& rhs);

	DXRSModel& mModel;
	std::string mName;
	std::map<TextureType, std::vector<std::wstring>*> mTextures;
};
