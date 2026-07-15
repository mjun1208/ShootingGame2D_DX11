#include "sprite_instanced.h"

#include "config.h"
#include "debug_ostream.h"
#include "direct3d.h"
#include "texture.h"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <vector>

using namespace DirectX;

namespace
{
	constexpr int VERTEX_COUNT = 4;
	constexpr int INSTANCE_CAPACITY = 32768;

	struct Vertex
	{
		XMFLOAT3 Position;
		XMFLOAT2 Texcoord;
	};

	struct SceneConstants
	{
		XMFLOAT4X4 ViewProjection;
	};

	ID3D11Buffer* g_VertexBuffer = nullptr;
	ID3D11Buffer* g_InstanceBuffer = nullptr;
	ID3D11Buffer* g_SceneConstantBuffer = nullptr;
	ID3D11VertexShader* g_VertexShader = nullptr;
	ID3D11PixelShader* g_PixelShader = nullptr;
	ID3D11InputLayout* g_InputLayout = nullptr;
	ID3D11SamplerState* g_SamplerState = nullptr;
	ID3D11BlendState* g_BlendState = nullptr;
	ID3D11DepthStencilState* g_DepthStencilState = nullptr;
	ID3D11RasterizerState* g_RasterizerState = nullptr;
	XMFLOAT4X4 g_ViewMatrix{};

	bool LoadBinary(const char* path, std::vector<unsigned char>& bytes)
	{
		std::ifstream stream(path, std::ios::binary | std::ios::ate);
		if (!stream)
		{
			return false;
		}

		const std::streamsize size = stream.tellg();
		if (size <= 0)
		{
			return false;
		}

		bytes.resize(static_cast<size_t>(size));
		stream.seekg(0, std::ios::beg);
		return static_cast<bool>(stream.read(reinterpret_cast<char*>(bytes.data()), size));
	}
}

bool SpriteInstanced_Initialize()
{
	ID3D11Device* device = Direct3D_GetDevice();
	if (!device)
	{
		return false;
	}

	XMStoreFloat4x4(&g_ViewMatrix, XMMatrixIdentity());

	const Vertex vertices[VERTEX_COUNT] = {
		{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f } },
		{ {  0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f } },
		{ { -0.5f,  0.5f, 0.0f }, { 0.0f, 1.0f } },
		{ {  0.5f,  0.5f, 0.0f }, { 1.0f, 1.0f } },
	};

	D3D11_BUFFER_DESC vertex_desc{};
	vertex_desc.Usage = D3D11_USAGE_IMMUTABLE;
	vertex_desc.ByteWidth = sizeof(vertices);
	vertex_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA vertex_data{};
	vertex_data.pSysMem = vertices;
	HRESULT hr = device->CreateBuffer(&vertex_desc, &vertex_data, &g_VertexBuffer);
	if (FAILED(hr))
	{
		return false;
	}

	D3D11_BUFFER_DESC instance_desc{};
	instance_desc.Usage = D3D11_USAGE_DYNAMIC;
	instance_desc.ByteWidth = sizeof(SpriteInstance) * INSTANCE_CAPACITY;
	instance_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	instance_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr = device->CreateBuffer(&instance_desc, nullptr, &g_InstanceBuffer);
	if (FAILED(hr))
	{
		SpriteInstanced_Finalize();
		return false;
	}

	D3D11_BUFFER_DESC constant_desc{};
	constant_desc.ByteWidth = sizeof(SceneConstants);
	constant_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = device->CreateBuffer(&constant_desc, nullptr, &g_SceneConstantBuffer);
	if (FAILED(hr))
	{
		SpriteInstanced_Finalize();
		return false;
	}

	std::vector<unsigned char> vertex_shader_binary;
	std::vector<unsigned char> pixel_shader_binary;
	if (!LoadBinary("asset/shader/shader_vertex_instanced_2d.cso", vertex_shader_binary) ||
		!LoadBinary("asset/shader/shader_pixel_instanced_2d.cso", pixel_shader_binary))
	{
		hal::dout << "Instanced sprite shader loading failed." << std::endl;
		SpriteInstanced_Finalize();
		return false;
	}

	hr = device->CreateVertexShader(
		vertex_shader_binary.data(), vertex_shader_binary.size(), nullptr, &g_VertexShader);
	if (FAILED(hr))
	{
		SpriteInstanced_Finalize();
		return false;
	}

	hr = device->CreatePixelShader(
		pixel_shader_binary.data(), pixel_shader_binary.size(), nullptr, &g_PixelShader);
	if (FAILED(hr))
	{
		SpriteInstanced_Finalize();
		return false;
	}

	const D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, Texcoord), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "INSTANCE_POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 1, offsetof(SpriteInstance, Position), D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 1, offsetof(SpriteInstance, Size), D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_ROTATION", 0, DXGI_FORMAT_R32_FLOAT, 1, offsetof(SpriteInstance, Rotation), D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, offsetof(SpriteInstance, Color), D3D11_INPUT_PER_INSTANCE_DATA, 1 },
	};
	hr = device->CreateInputLayout(
		layout, ARRAYSIZE(layout), vertex_shader_binary.data(), vertex_shader_binary.size(), &g_InputLayout);
	if (FAILED(hr))
	{
		SpriteInstanced_Finalize();
		return false;
	}

	D3D11_SAMPLER_DESC sampler_desc{};
	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
	hr = device->CreateSamplerState(&sampler_desc, &g_SamplerState);
	if (FAILED(hr))
	{
		SpriteInstanced_Finalize();
		return false;
	}

	D3D11_BLEND_DESC blend_desc{};
	blend_desc.RenderTarget[0].BlendEnable = TRUE;
	blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	hr = device->CreateBlendState(&blend_desc, &g_BlendState);
	if (FAILED(hr))
	{
		SpriteInstanced_Finalize();
		return false;
	}

	D3D11_DEPTH_STENCIL_DESC depth_desc{};
	depth_desc.DepthEnable = FALSE;
	depth_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depth_desc.StencilEnable = FALSE;
	hr = device->CreateDepthStencilState(&depth_desc, &g_DepthStencilState);
	if (FAILED(hr))
	{
		SpriteInstanced_Finalize();
		return false;
	}

	D3D11_RASTERIZER_DESC rasterizer_desc{};
	rasterizer_desc.FillMode = D3D11_FILL_SOLID;
	rasterizer_desc.CullMode = D3D11_CULL_NONE;
	rasterizer_desc.DepthClipEnable = TRUE;
	hr = device->CreateRasterizerState(&rasterizer_desc, &g_RasterizerState);
	if (FAILED(hr))
	{
		SpriteInstanced_Finalize();
		return false;
	}

	return true;
}

void SpriteInstanced_SetViewMatrix(const XMMATRIX& view_matrix)
{
	XMStoreFloat4x4(&g_ViewMatrix, view_matrix);
}

void SpriteInstanced_Finalize()
{
	SAFE_RELEASE(g_RasterizerState);
	SAFE_RELEASE(g_DepthStencilState);
	SAFE_RELEASE(g_BlendState);
	SAFE_RELEASE(g_SamplerState);
	SAFE_RELEASE(g_InputLayout);
	SAFE_RELEASE(g_PixelShader);
	SAFE_RELEASE(g_VertexShader);
	SAFE_RELEASE(g_SceneConstantBuffer);
	SAFE_RELEASE(g_InstanceBuffer);
	SAFE_RELEASE(g_VertexBuffer);
}

void SpriteInstanced_Draw(int texture_id, const SpriteInstance* instances, int instance_count)
{
	if (texture_id == TEXTURE_INVALID_ID || !instances || instance_count <= 0)
	{
		return;
	}

	ID3D11DeviceContext* context = Direct3D_GetDeviceContext();
	const XMMATRIX view = XMLoadFloat4x4(&g_ViewMatrix);
	const XMMATRIX projection = XMMatrixOrthographicOffCenterLH(
		0.0f, static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT), 0.0f, 0.0f, 1.0f);
	SceneConstants constants{};
	XMStoreFloat4x4(&constants.ViewProjection, XMMatrixTranspose(view * projection));
	context->UpdateSubresource(g_SceneConstantBuffer, 0, nullptr, &constants, 0, 0);

	context->VSSetShader(g_VertexShader, nullptr, 0);
	context->PSSetShader(g_PixelShader, nullptr, 0);
	context->IASetInputLayout(g_InputLayout);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	context->VSSetConstantBuffers(0, 1, &g_SceneConstantBuffer);
	context->PSSetSamplers(0, 1, &g_SamplerState);
	Texture_SetTexture(texture_id);
	context->OMSetBlendState(g_BlendState, nullptr, 0xffffffff);
	context->OMSetDepthStencilState(g_DepthStencilState, 0);
	context->RSSetState(g_RasterizerState);

	for (int first_instance = 0; first_instance < instance_count; first_instance += INSTANCE_CAPACITY)
	{
		const int draw_count = std::min(INSTANCE_CAPACITY, instance_count - first_instance);
		D3D11_MAPPED_SUBRESOURCE mapped{};
		const HRESULT hr = context->Map(g_InstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		if (FAILED(hr))
		{
			return;
		}

		std::copy_n(instances + first_instance, draw_count, static_cast<SpriteInstance*>(mapped.pData));
		context->Unmap(g_InstanceBuffer, 0);

		ID3D11Buffer* buffers[] = { g_VertexBuffer, g_InstanceBuffer };
		const UINT strides[] = { sizeof(Vertex), sizeof(SpriteInstance) };
		const UINT offsets[] = { 0, 0 };
		context->IASetVertexBuffers(0, ARRAYSIZE(buffers), buffers, strides, offsets);
		context->DrawInstanced(VERTEX_COUNT, draw_count, 0, 0);
	}
}
