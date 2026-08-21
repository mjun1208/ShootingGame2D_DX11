# DirectX11 Project Context

## Environment
- Windows Desktop
- Visual Studio 2022
- C++
- DirectX 11
- HLSL Shader Model 5
- DirectXMath

## Code conventions
- DirectX namespace 사용
- Matrix: XMMATRIX
- Stored matrix: XMFLOAT4X4
- COM object: ComPtr
- nullptr 사용
- D3DX 사용 금지

## Rendering architecture

Game
 └─ Renderer
     ├─ Device
     ├─ DeviceContext
     ├─ SwapChain
     ├─ RenderTarget
     ├─ DepthStencil
     ├─ Shader
     ├─ Mesh
     └─ Texture

## Shader convention

Vertex Shader:
- b0 = Transform constant buffer

Pixel Shader:
- b0 = Material
- t0 = Main Texture
- s0 = Main Sampler