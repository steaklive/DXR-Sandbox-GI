# About
Simple C++/DX12 framework for my bachelor thesis on real-time global illumination techniques
(WIP)

![picture](screenshots/lpv.png)

# Features
- Deferred Rendering
- Shadow Mapping
- GI w/ Reflective Shadow Mapping (indirect diffuse)
- GI w/ Light Propagation Volume (indirect diffuse)
- GI w/ Voxel Cone Tracing (indirect diffuse + specular + AO)

# Optimizations
Reflective Shadow Mapping:
- main pass in compute (or pixel)
- efficient upsample & blur of main pass' output in compute

Light Propagation Volumes:
- flux downsample in compute (or pixel)

Voxel Cone Tracing:
- main pass in compute (or pixel)
- anisotropic mipmapping passes in compute
- efficient upsample & blur of main pass' output in compute

# Additional dependencies
- Assimp
- DirectXTK12
- ImGUI

# Requirements
- VS2019
- DirectX12 with DXR
- latest Windows SDK
- GPU with RTX support (if using DXR)
