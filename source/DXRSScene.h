#pragma once
#include "Common.h"
#include "DXRSGraphics.h"
#include "DXRSTimer.h"

class DXRSScene
{
public:
	virtual void Init(HWND window, int width, int height);
	virtual void Run();
	virtual void Clear();

private:
	virtual void Update(DXRSTimer const& timer);
	virtual void Render();
	virtual void CreateDeviceResources();
	virtual void CreateWindowResources();
	   
};