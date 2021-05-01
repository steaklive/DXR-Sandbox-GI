#pragma once

#include <DirectXMath.h>
#include <DirectXColors.h>

#include "d3dx12.h"
#include "pix3.h"

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

#include <d3dcompiler.h>

#include <algorithm>
#include <exception>
#include <memory>
#include <stdexcept>
#include <stdio.h>
#include <iostream>
#include <system_error>
#include <cmath>

#include "Audio.h"
#include "CommonStates.h"
#include "DirectXHelpers.h"
#include "DDSTextureLoader.h"
#include "Effects.h"
#include "GamePad.h"
#include "GeometricPrimitive.h"
#include "GraphicsMemory.h"
#include "Keyboard.h"
#include "Model.h"
#include "Mouse.h"
#include "PrimitiveBatch.h"
#include "ResourceUploadBatch.h"
#include "RenderTargetState.h"
#include "SimpleMath.h"
#include "SpriteBatch.h"
#include "SpriteFont.h" 
#include "VertexTypes.h"

#define Align(value, alignment) (((value + alignment - 1) / alignment) * alignment)

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>
using namespace Microsoft::WRL;

using namespace DirectX;
using namespace DirectX::SimpleMath;

// sorry, but I am very lazy to type std::unique_ptr<>...
template<class T> using U_PTR = std::unique_ptr<T>;

inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
		std::string message = std::system_category().message(hr);
		//throw std::runtime_error(message.c_str());
    }
}

inline float RandomFloat(float a, float b) {
	float random = ((float)rand()) / (float)RAND_MAX;
	float diff = b - a;
	float r = random * diff;
	return a + r;
}

template <typename T>
inline T DivideByMultiple(T value, size_t alignment)
{
	return (T)((value + alignment - 1) / alignment);
}

template <typename T>
inline bool IsPowerOfTwo(T value)
{
	return 0 == (value & (value - 1));
}