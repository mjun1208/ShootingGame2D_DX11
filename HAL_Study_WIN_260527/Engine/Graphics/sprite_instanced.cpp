#include "sprite_instanced.h"

#include "config.h"
#include "debug_ostream.h"
#include "direct3d.h"
#include "file_utils.h"
#include "render_state_utils.h"
#include "texture.h"

#include <algorithm>
#include <cstddef>
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
	ID3D11Buffer* g_CutInstanceBuffer = nullptr;
	ID3D11Buffer* g_SceneConstantBuffer = nullptr;
	ID3D11Buffer* g_LightConstantBuffer = nullptr;
	ID3D11VertexShader* g_VertexShader = nullptr;
	ID3D11VertexShader* g_CutVertexShader = nullptr;
	ID3D11PixelShader* g_PixelShader = nullptr;
	ID3D11PixelShader* g_CutPixelShader = nullptr;
	ID3D11PixelShader* g_LightningPixelShader = nullptr;
	ID3D11InputLayout* g_InputLayout = nullptr;
	ID3D11InputLayout* g_CutInputLayout = nullptr;
	ID3D11SamplerState* g_SamplerState = nullptr;
	ID3D11SamplerState* g_WrapUSamplerState = nullptr;
	ID3D11SamplerState* g_LightningSamplerState = nullptr;
	ID3D11BlendState* g_BlendState = nullptr;
	ID3D11BlendState* g_AdditiveBlendState = nullptr;
	ID3D11BlendState* g_LightningBlendState = nullptr;
	ID3D11DepthStencilState* g_DepthStencilState = nullptr;
	ID3D11RasterizerState* g_RasterizerState = nullptr;
	XMFLOAT4X4 g_ViewMatrix{};
} // namespace

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
		{ { 0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f } },
		{ { -0.5f, 0.5f, 0.0f }, { 0.0f, 1.0f } },
		{ { 0.5f, 0.5f, 0.0f }, { 1.0f, 1.0f } },
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
	instance_desc.ByteWidth = sizeof(CutSpriteInstance) * INSTANCE_CAPACITY;
	hr = device->CreateBuffer(&instance_desc, nullptr, &g_CutInstanceBuffer);
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
	constant_desc.ByteWidth = sizeof(SpriteLightConstants);
	hr = device->CreateBuffer(&constant_desc, nullptr, &g_LightConstantBuffer);
	if (FAILED(hr))
	{
		SpriteInstanced_Finalize();
		return false;
	}

	std::vector<unsigned char> vertex_shader_binary;
	std::vector<unsigned char> pixel_shader_binary;
	std::vector<unsigned char> cut_vertex_shader_binary;
	std::vector<unsigned char> cut_pixel_shader_binary;
	std::vector<unsigned char> lightning_pixel_shader_binary;
	if (!ReadBinaryFile("asset/shader/shader_vertex_instanced_2d.cso", vertex_shader_binary) ||
	    !ReadBinaryFile("asset/shader/shader_pixel_instanced_2d.cso", pixel_shader_binary) ||
	    !ReadBinaryFile("asset/shader/shader_vertex_cut_2d.cso", cut_vertex_shader_binary) ||
	    !ReadBinaryFile("asset/shader/shader_pixel_cut_2d.cso", cut_pixel_shader_binary) ||
	    !ReadBinaryFile("asset/shader/shader_pixel_lightning.cso", lightning_pixel_shader_binary))
	{
		hal::dout << "Instanced sprite shader loading failed." << std::endl;
		SpriteInstanced_Finalize();
		return false;
	}

	hr = device->CreateVertexShader(vertex_shader_binary.data(), vertex_shader_binary.size(), nullptr, &g_VertexShader);
	if (FAILED(hr))
	{
		SpriteInstanced_Finalize();
		return false;
	}

	hr = device->CreatePixelShader(pixel_shader_binary.data(), pixel_shader_binary.size(), nullptr, &g_PixelShader);
	if (FAILED(hr))
	{
		SpriteInstanced_Finalize();
		return false;
	}
	hr = device->CreateVertexShader(cut_vertex_shader_binary.data(), cut_vertex_shader_binary.size(), nullptr,
	                                &g_CutVertexShader);
	if (FAILED(hr))
	{
		SpriteInstanced_Finalize();
		return false;
	}
	hr = device->CreatePixelShader(cut_pixel_shader_binary.data(), cut_pixel_shader_binary.size(), nullptr,
	                               &g_CutPixelShader);
	if (FAILED(hr))
	{
		SpriteInstanced_Finalize();
		return false;
	}
	hr = device->CreatePixelShader(lightning_pixel_shader_binary.data(), lightning_pixel_shader_binary.size(), nullptr,
	                               &g_LightningPixelShader);
	if (FAILED(hr))
	{
		SpriteInstanced_Finalize();
		return false;
	}

	const D3D11_INPUT_ELEMENT_DESC cut_layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, Texcoord), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "INSTANCE_POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 1, offsetof(CutSpriteInstance, Position),
		  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 1, offsetof(CutSpriteInstance, Size),
		  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_ROTATION", 0, DXGI_FORMAT_R32_FLOAT, 1, offsetof(CutSpriteInstance, Rotation),
		  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, offsetof(CutSpriteInstance, Color),
		  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_TEXCOORD_OFFSET", 0, DXGI_FORMAT_R32G32_FLOAT, 1, offsetof(CutSpriteInstance, TexcoordOffset),
		  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_TEXCOORD_SCALE", 0, DXGI_FORMAT_R32G32_FLOAT, 1, offsetof(CutSpriteInstance, TexcoordScale),
		  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_CUT_NORMAL", 0, DXGI_FORMAT_R32G32_FLOAT, 1, offsetof(CutSpriteInstance, CutNormal),
		  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_CUT_PARAMETERS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, offsetof(CutSpriteInstance, CutParameters),
		  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_EDGE_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, offsetof(CutSpriteInstance, EdgeColor),
		  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
	};
	hr = device->CreateInputLayout(cut_layout, ARRAYSIZE(cut_layout), cut_vertex_shader_binary.data(),
	                               cut_vertex_shader_binary.size(), &g_CutInputLayout);
	if (FAILED(hr))
	{
		SpriteInstanced_Finalize();
		return false;
	}

	const D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, Texcoord), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "INSTANCE_POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 1, offsetof(SpriteInstance, Position),
		  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 1, offsetof(SpriteInstance, Size),
		  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_ROTATION", 0, DXGI_FORMAT_R32_FLOAT, 1, offsetof(SpriteInstance, Rotation),
		  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, offsetof(SpriteInstance, Color),
		  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_TEXCOORD_OFFSET", 0, DXGI_FORMAT_R32G32_FLOAT, 1, offsetof(SpriteInstance, TexcoordOffset),
		  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_TEXCOORD_SCALE", 0, DXGI_FORMAT_R32G32_FLOAT, 1, offsetof(SpriteInstance, TexcoordScale),
		  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "INSTANCE_COLOR_MASK", 0, DXGI_FORMAT_R32_FLOAT, 1, offsetof(SpriteInstance, ColorMask),
		  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
	};
	hr = device->CreateInputLayout(layout, ARRAYSIZE(layout), vertex_shader_binary.data(), vertex_shader_binary.size(),
	                               &g_InputLayout);
	if (FAILED(hr))
	{
		SpriteInstanced_Finalize();
		return false;
	}

	D3D11_SAMPLER_DESC sampler_desc =
	    hal::CreateSamplerDesc(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);
	hr = device->CreateSamplerState(&sampler_desc, &g_SamplerState);
	if (FAILED(hr))
	{
		SpriteInstanced_Finalize();
		return false;
	}
	D3D11_SAMPLER_DESC wrap_u_sampler_desc = sampler_desc;
	wrap_u_sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	hr = device->CreateSamplerState(&wrap_u_sampler_desc, &g_WrapUSamplerState);
	if (FAILED(hr))
	{
		SpriteInstanced_Finalize();
		return false;
	}
	D3D11_SAMPLER_DESC lightning_sampler_desc = sampler_desc;
	lightning_sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	hr = device->CreateSamplerState(&lightning_sampler_desc, &g_LightningSamplerState);
	if (FAILED(hr))
	{
		SpriteInstanced_Finalize();
		return false;
	}

	D3D11_BLEND_DESC blend_desc = hal::CreateAlphaBlendDesc();
	hr = device->CreateBlendState(&blend_desc, &g_BlendState);
	if (FAILED(hr))
	{
		SpriteInstanced_Finalize();
		return false;
	}
	D3D11_BLEND_DESC additive_blend_desc = hal::CreateAdditiveBlendDesc();
	hr = device->CreateBlendState(&additive_blend_desc, &g_AdditiveBlendState);
	if (FAILED(hr))
	{
		SpriteInstanced_Finalize();
		return false;
	}
	D3D11_BLEND_DESC lightning_blend_desc = hal::CreateOneOneBlendDesc();
	hr = device->CreateBlendState(&lightning_blend_desc, &g_LightningBlendState);
	if (FAILED(hr))
	{
		SpriteInstanced_Finalize();
		return false;
	}

	D3D11_DEPTH_STENCIL_DESC depth_desc = hal::CreateDepthDisabledDesc(D3D11_COMPARISON_NEVER);
	hr = device->CreateDepthStencilState(&depth_desc, &g_DepthStencilState);
	if (FAILED(hr))
	{
		SpriteInstanced_Finalize();
		return false;
	}

	D3D11_RASTERIZER_DESC rasterizer_desc = hal::CreateRasterizerDesc(D3D11_CULL_NONE);
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
	SAFE_RELEASE(g_LightningBlendState);
	SAFE_RELEASE(g_AdditiveBlendState);
	SAFE_RELEASE(g_BlendState);
	SAFE_RELEASE(g_LightningSamplerState);
	SAFE_RELEASE(g_WrapUSamplerState);
	SAFE_RELEASE(g_SamplerState);
	SAFE_RELEASE(g_CutInputLayout);
	SAFE_RELEASE(g_InputLayout);
	SAFE_RELEASE(g_LightningPixelShader);
	SAFE_RELEASE(g_CutPixelShader);
	SAFE_RELEASE(g_PixelShader);
	SAFE_RELEASE(g_CutVertexShader);
	SAFE_RELEASE(g_VertexShader);
	SAFE_RELEASE(g_LightConstantBuffer);
	SAFE_RELEASE(g_SceneConstantBuffer);
	SAFE_RELEASE(g_CutInstanceBuffer);
	SAFE_RELEASE(g_InstanceBuffer);
	SAFE_RELEASE(g_VertexBuffer);
}

static void SpriteInstanced_DrawWithBlend(int texture_id, const SpriteInstance* instances, int instance_count,
                                          ID3D11BlendState* blend_state, ID3D11PixelShader* pixel_shader,
                                          ID3D11SamplerState* sampler_state, bool lighting_enabled)
{
	if (texture_id == TEXTURE_INVALID_ID || !instances || instance_count <= 0)
	{
		return;
	}

	ID3D11DeviceContext* context = Direct3D_GetDeviceContext();
	const XMMATRIX view = XMLoadFloat4x4(&g_ViewMatrix);
	const XMMATRIX projection = XMMatrixOrthographicOffCenterLH(0.0f, static_cast<float>(SCREEN_WIDTH),
	                                                            static_cast<float>(SCREEN_HEIGHT), 0.0f, 0.0f, 1.0f);
	SceneConstants constants{};
	XMStoreFloat4x4(&constants.ViewProjection, XMMatrixTranspose(view * projection));
	context->UpdateSubresource(g_SceneConstantBuffer, 0, nullptr, &constants, 0, 0);
	const SpriteLightConstants light_constants = SpriteLighting_BuildConstants(g_ViewMatrix, lighting_enabled, true);
	context->UpdateSubresource(g_LightConstantBuffer, 0, nullptr, &light_constants, 0, 0);

	context->VSSetShader(g_VertexShader, nullptr, 0);
	context->PSSetShader(pixel_shader, nullptr, 0);
	context->IASetInputLayout(g_InputLayout);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	context->VSSetConstantBuffers(0, 1, &g_SceneConstantBuffer);
	context->PSSetConstantBuffers(0, 1, &g_LightConstantBuffer);
	context->PSSetSamplers(0, 1, &sampler_state);
	Texture_SetTexture(texture_id);
	context->OMSetBlendState(blend_state, nullptr, 0xffffffff);
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

void SpriteInstanced_Draw(int texture_id, const SpriteInstance* instances, int instance_count)
{
	SpriteInstanced_DrawWithBlend(texture_id, instances, instance_count, g_BlendState, g_PixelShader, g_SamplerState,
	                              true);
}

void SpriteInstanced_DrawUnlit(int texture_id, const SpriteInstance* instances, int instance_count)
{
	SpriteInstanced_DrawWithBlend(texture_id, instances, instance_count, g_BlendState, g_PixelShader, g_SamplerState,
	                              false);
}

void SpriteInstanced_DrawOutlinedUnlit(int texture_id, const SpriteInstance* instances, int instance_count,
                                       const XMFLOAT4& outline_color, float outline_thickness)
{
	if (texture_id == TEXTURE_INVALID_ID || !instances || instance_count <= 0 || outline_thickness <= 0.0f)
	{
		SpriteInstanced_DrawUnlit(texture_id, instances, instance_count);
		return;
	}

	constexpr XMFLOAT2 OUTLINE_DIRECTIONS[] = {
		{ -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f }, { -1.0f, 0.0f },
		{ 1.0f, 0.0f },   { -1.0f, 1.0f }, { 0.0f, 1.0f },  { 1.0f, 1.0f },
	};
	static std::vector<SpriteInstance> outline_instances;
	outline_instances.clear();
	outline_instances.reserve(static_cast<size_t>(instance_count) * ARRAYSIZE(OUTLINE_DIRECTIONS));

	for (const XMFLOAT2& direction : OUTLINE_DIRECTIONS)
	{
		for (int i = 0; i < instance_count; ++i)
		{
			SpriteInstance outline = instances[i];
			outline.Position.x += direction.x * outline_thickness;
			outline.Position.y += direction.y * outline_thickness;
			outline.Color = outline_color;
			outline.ColorMask = 1.0f;
			outline_instances.push_back(outline);
		}
	}

	SpriteInstanced_DrawUnlit(texture_id, outline_instances.data(), static_cast<int>(outline_instances.size()));
	SpriteInstanced_DrawUnlit(texture_id, instances, instance_count);
}

void SpriteInstanced_DrawWrapUUnlit(int texture_id, const SpriteInstance* instances, int instance_count)
{
	SpriteInstanced_DrawWithBlend(texture_id, instances, instance_count, g_BlendState, g_PixelShader,
	                              g_WrapUSamplerState, false);
}

void SpriteInstanced_DrawAdditive(int texture_id, const SpriteInstance* instances, int instance_count)
{
	SpriteInstanced_DrawWithBlend(texture_id, instances, instance_count, g_AdditiveBlendState, g_PixelShader,
	                              g_SamplerState, true);
}

void SpriteInstanced_DrawAdditiveUnlit(int texture_id, const SpriteInstance* instances, int instance_count)
{
	SpriteInstanced_DrawWithBlend(texture_id, instances, instance_count, g_AdditiveBlendState, g_PixelShader,
	                              g_SamplerState, false);
}

void SpriteInstanced_DrawLightning(int texture_id, const SpriteInstance* instances, int instance_count)
{
	SpriteInstanced_DrawWithBlend(texture_id, instances, instance_count, g_LightningBlendState, g_LightningPixelShader,
	                              g_LightningSamplerState, true);
}

void SpriteInstanced_DrawCut(int texture_id, const CutSpriteInstance* instances, int instance_count)
{
	if (texture_id == TEXTURE_INVALID_ID || !instances || instance_count <= 0 || !g_CutVertexShader ||
	    !g_CutPixelShader || !g_CutInputLayout || !g_CutInstanceBuffer)
	{
		return;
	}

	ID3D11DeviceContext* context = Direct3D_GetDeviceContext();
	const XMMATRIX view = XMLoadFloat4x4(&g_ViewMatrix);
	const XMMATRIX projection = XMMatrixOrthographicOffCenterLH(0.0f, static_cast<float>(SCREEN_WIDTH),
	                                                            static_cast<float>(SCREEN_HEIGHT), 0.0f, 0.0f, 1.0f);
	SceneConstants constants{};
	XMStoreFloat4x4(&constants.ViewProjection, XMMatrixTranspose(view * projection));
	context->UpdateSubresource(g_SceneConstantBuffer, 0, nullptr, &constants, 0, 0);
	const SpriteLightConstants light_constants = SpriteLighting_BuildConstants(g_ViewMatrix, true, true);
	context->UpdateSubresource(g_LightConstantBuffer, 0, nullptr, &light_constants, 0, 0);

	context->VSSetShader(g_CutVertexShader, nullptr, 0);
	context->PSSetShader(g_CutPixelShader, nullptr, 0);
	context->IASetInputLayout(g_CutInputLayout);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	context->VSSetConstantBuffers(0, 1, &g_SceneConstantBuffer);
	context->PSSetConstantBuffers(0, 1, &g_LightConstantBuffer);
	context->PSSetSamplers(0, 1, &g_SamplerState);
	Texture_SetTexture(texture_id);
	context->OMSetBlendState(g_BlendState, nullptr, 0xffffffff);
	context->OMSetDepthStencilState(g_DepthStencilState, 0);
	context->RSSetState(g_RasterizerState);

	for (int first_instance = 0; first_instance < instance_count; first_instance += INSTANCE_CAPACITY)
	{
		const int draw_count = std::min(INSTANCE_CAPACITY, instance_count - first_instance);
		D3D11_MAPPED_SUBRESOURCE mapped{};
		const HRESULT hr = context->Map(g_CutInstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		if (FAILED(hr))
		{
			return;
		}

		std::copy_n(instances + first_instance, draw_count, static_cast<CutSpriteInstance*>(mapped.pData));
		context->Unmap(g_CutInstanceBuffer, 0);

		ID3D11Buffer* buffers[] = { g_VertexBuffer, g_CutInstanceBuffer };
		const UINT strides[] = { sizeof(Vertex), sizeof(CutSpriteInstance) };
		const UINT offsets[] = { 0, 0 };
		context->IASetVertexBuffers(0, ARRAYSIZE(buffers), buffers, strides, offsets);
		context->DrawInstanced(VERTEX_COUNT, draw_count, 0, 0);
	}
}
