#define NOMINMAX

#include "DXRSModelMaterial.h"

#include <assimp/scene.h>


std::map<TextureType, UINT> DXRSModelMaterial::sTextureTypeMappings;

DXRSModelMaterial::DXRSModelMaterial(DXRSModel& model)
	: mModel(model), mTextures()
{
	InitializeTextureTypeMappings();
}

DXRSModelMaterial::DXRSModelMaterial(DXRSModel& model, aiMaterial* material)
	: mModel(model), mTextures()
{
	InitializeTextureTypeMappings();

	aiString name;
	material->Get(AI_MATKEY_NAME, name);
	mName = name.C_Str();

	for (TextureType textureType = (TextureType)0; textureType < TextureTypeEnd; textureType = (TextureType)(textureType + 1))
	{
		aiTextureType mappedTextureType = (aiTextureType)sTextureTypeMappings[textureType];

		UINT textureCount = material->GetTextureCount(mappedTextureType);
		if (textureCount > 0)
		{
			std::vector<std::wstring>* textures = new std::vector<std::wstring>();
			mTextures.insert(std::pair<TextureType, std::vector<std::wstring>*>(textureType, textures));

			textures->reserve(textureCount);
			for (UINT textureIndex = 0; textureIndex < textureCount; textureIndex++)
			{
				aiString path;
				if (material->GetTexture(mappedTextureType, textureIndex, &path) == AI_SUCCESS)
				{
					std::wstring wPath;
					std::string pathString = path.C_Str();
					wPath.assign(pathString.begin(), pathString.end());
					textures->push_back(wPath);
				}
			}
		}
	}
}

DXRSModelMaterial::~DXRSModelMaterial()
{
	for (std::pair<TextureType, std::vector<std::wstring>*> textures : mTextures)
	{
		DeleteObject(textures.second);
	}
}

DXRSModel& DXRSModelMaterial::GetModel()
{
	return mModel;
}

const std::string& DXRSModelMaterial::Name() const
{
	return mName;
}

const std::map<TextureType, std::vector<std::wstring>*> DXRSModelMaterial::Textures() const
{
	return mTextures;
}

std::vector<std::wstring>* DXRSModelMaterial::GetTexturesByType(TextureType type)
{
	auto it = mTextures.find(type);
	if (it != mTextures.end())
		return mTextures[type];
	else
		return nullptr;
}

void DXRSModelMaterial::InitializeTextureTypeMappings()
{
	if (sTextureTypeMappings.size() != TextureTypeEnd)
	{
		sTextureTypeMappings[TextureTypeDifffuse] = aiTextureType_DIFFUSE;
		sTextureTypeMappings[TextureTypeSpecularMap] = aiTextureType_SPECULAR;
		sTextureTypeMappings[TextureTypeAmbient] = aiTextureType_AMBIENT;
		sTextureTypeMappings[TextureTypeHeightmap] = aiTextureType_HEIGHT;
		sTextureTypeMappings[TextureTypeNormalMap] = aiTextureType_NORMALS;
		sTextureTypeMappings[TextureTypeSpecularPowerMap] = aiTextureType_SHININESS;
		sTextureTypeMappings[TextureTypeDisplacementMap] = aiTextureType_DISPLACEMENT;
		sTextureTypeMappings[TextureTypeLightMap] = aiTextureType_LIGHTMAP;
	}
}