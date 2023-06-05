# About
Simple C++/DX12 framework for my bachelor thesis "Dynamic real-time global illumination algorithms on modern GPU hardware and software" ([paper](thesis.pdf)).

Video: https://youtu.be/4YUWw8x4XXc

![picture](screenshots/main.png)

# Features
- Deferred Rendering
- Shadow Mapping
- GI w/ Reflective Shadow Mapping (indirect diffuse)
- GI w/ Light Propagation Volume (indirect diffuse)
- GI w/ Voxel Cone Tracing (indirect diffuse + specular + AO)
- SSAO
- Asynchronous compute
- DXR Reflections + Blur

# Optimizations
Reflective Shadow Mapping:
- main pass in compute (+async)
- efficient upsample & blur of main pass' output in compute (+async)

![picture](screenshots/RSM_async.png)

Light Propagation Volumes:
- flux downsample in compute
- DX12 bundle for propagation passes

![picture](screenshots/LPV_w_downsampling.png)

Voxel Cone Tracing:
- main pass in compute (+async)
- anisotropic mipmapping passes in compute (+async)
- efficient upsample & blur of main pass' output in compute (+async)

![picture](screenshots/VCT_async.png)

SSAO:
- TODO: downscaled version
- TODO: move to compute with LDS optimizations
- TODO: move to async

# Comparison
No GI -> RSM -> LPV -> VCT -> offline path-tracer

![picture](screenshots/comparison.png)

# Additional screenshots
![picture](screenshots/1.png)
![picture](screenshots/2.png)
![picture](screenshots/3.png)

# Dependencies
- Assimp
- DirectXTK12
- ImGUI

# Requirements
- VS2019
- DirectX12
- Windows 10 (1809+)
- latest Windows SDK
- NVIDIA GPU with RTX support (if using DXR)
