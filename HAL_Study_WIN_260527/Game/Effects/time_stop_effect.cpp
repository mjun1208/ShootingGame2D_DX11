#include "time_stop_effect.h"

#include "direct3d.h"
#include "file_utils.h"
#include "math_utils.h"
#include "render_state_utils.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <vector>

namespace
{
	namespace TimeStopTuning
	{
		constexpr float WAVE_OUT_DURATION = 0.12f;
		constexpr float WAVE_RETURN_DURATION = 0.14f;
		constexpr float STOP_HOLD_DURATION = 1.35f;
		constexpr float RESTORE_DURATION = 0.16f;
	} // namespace TimeStopTuning

} // namespace

namespace
{

	enum class EffectPhase
	{
		Inactive = 0,
		WaveOut = 1,
		WaveReturn = 2,
		Hold = 3,
		Restore = 4,
	};

	struct PixelConstants
	{
		DirectX::XMFLOAT2 CenterUv;
		float Radius;
		float Phase;
		DirectX::XMFLOAT2 Resolution;
		float Time;
		float MaxRadius;
	};

	ID3D11Device* g_Device = nullptr;
	ID3D11DeviceContext* g_Context = nullptr;
	ID3D11RenderTargetView* g_BackBuffer = nullptr;
	ID3D11DepthStencilView* g_DepthStencil = nullptr;
	ID3D11Texture2D* g_SceneTexture = nullptr;
	ID3D11RenderTargetView* g_SceneRenderTarget = nullptr;
	ID3D11ShaderResourceView* g_SceneShaderResource = nullptr;
	ID3D11VertexShader* g_VertexShader = nullptr;
	ID3D11PixelShader* g_PixelShader = nullptr;
	ID3D11Buffer* g_PixelConstantBuffer = nullptr;
	ID3D11SamplerState* g_SamplerState = nullptr;
	ID3D11BlendState* g_BlendState = nullptr;
	ID3D11DepthStencilState* g_DepthStencilState = nullptr;
	ID3D11RasterizerState* g_RasterizerState = nullptr;
	unsigned int g_Width = 1;
	unsigned int g_Height = 1;
	float g_ElapsedTime = 0.0f;
	float g_MaxRadius = 1.0f;
	DirectX::XMFLOAT2 g_CenterUv{ 0.5f, 0.5f };
	EffectPhase g_Phase = EffectPhase::Inactive;

	void ReleaseResources()
	{
		SAFE_RELEASE(g_RasterizerState);
		SAFE_RELEASE(g_DepthStencilState);
		SAFE_RELEASE(g_BlendState);
		SAFE_RELEASE(g_SamplerState);
		SAFE_RELEASE(g_PixelConstantBuffer);
		SAFE_RELEASE(g_PixelShader);
		SAFE_RELEASE(g_VertexShader);
		SAFE_RELEASE(g_SceneShaderResource);
		SAFE_RELEASE(g_SceneRenderTarget);
		SAFE_RELEASE(g_SceneTexture);
		g_DepthStencil = nullptr;
		g_BackBuffer = nullptr;
		g_Context = nullptr;
		g_Device = nullptr;
	}

	void FillAnimatedConstants(PixelConstants& constants)
	{
		constants.Phase = static_cast<float>(g_Phase);
		constants.Radius = 0.0f;
		if (g_Phase == EffectPhase::WaveOut)
		{
			constants.Radius = g_MaxRadius * EaseOutCubic(g_ElapsedTime / TimeStopTuning::WAVE_OUT_DURATION);
		}
		else if (g_Phase == EffectPhase::WaveReturn)
		{
			const float local_time = g_ElapsedTime - TimeStopTuning::WAVE_OUT_DURATION;
			constants.Radius = g_MaxRadius * (1.0f - SmoothStep(local_time / TimeStopTuning::WAVE_RETURN_DURATION));
		}
		else if (g_Phase == EffectPhase::Restore)
		{
			const float local_time = g_ElapsedTime - TimeStopTuning::WAVE_OUT_DURATION -
			                         TimeStopTuning::WAVE_RETURN_DURATION - TimeStopTuning::STOP_HOLD_DURATION;
			constants.Radius = g_MaxRadius * (1.0f - SmoothStep(local_time / TimeStopTuning::RESTORE_DURATION));
		}
	}
} // namespace

bool TimeStopEffect_Initialize(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11RenderTargetView* back_buffer,
                               ID3D11DepthStencilView* depth_stencil, unsigned int width, unsigned int height)
{
	TimeStopEffect_Finalize();
	if (!device || !context || !back_buffer || width == 0 || height == 0)
	{
		return false;
	}
	g_Device = device;
	g_Context = context;
	g_BackBuffer = back_buffer;
	g_DepthStencil = depth_stencil;
	g_Width = width;
	g_Height = height;

	D3D11_TEXTURE2D_DESC texture_desc{};
	texture_desc.Width = width;
	texture_desc.Height = height;
	texture_desc.MipLevels = 1;
	texture_desc.ArraySize = 1;
	texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texture_desc.SampleDesc.Count = 1;
	texture_desc.Usage = D3D11_USAGE_DEFAULT;
	texture_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	if (FAILED(device->CreateTexture2D(&texture_desc, nullptr, &g_SceneTexture)) ||
	    FAILED(device->CreateRenderTargetView(g_SceneTexture, nullptr, &g_SceneRenderTarget)) ||
	    FAILED(device->CreateShaderResourceView(g_SceneTexture, nullptr, &g_SceneShaderResource)))
	{
		ReleaseResources();
		return false;
	}

	std::vector<unsigned char> shader_binary;
	if (!ReadBinaryFile("asset/shader/shader_vertex_time_stop.cso", shader_binary) ||
	    FAILED(device->CreateVertexShader(shader_binary.data(), shader_binary.size(), nullptr, &g_VertexShader)))
	{
		ReleaseResources();
		return false;
	}
	shader_binary.clear();
	if (!ReadBinaryFile("asset/shader/shader_pixel_time_stop.cso", shader_binary) ||
	    FAILED(device->CreatePixelShader(shader_binary.data(), shader_binary.size(), nullptr, &g_PixelShader)))
	{
		ReleaseResources();
		return false;
	}

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(PixelConstants);
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	if (FAILED(device->CreateBuffer(&buffer_desc, nullptr, &g_PixelConstantBuffer)))
	{
		ReleaseResources();
		return false;
	}

	D3D11_SAMPLER_DESC sampler_desc =
	    hal::CreateSamplerDesc(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);
	if (FAILED(device->CreateSamplerState(&sampler_desc, &g_SamplerState)))
	{
		ReleaseResources();
		return false;
	}

	D3D11_BLEND_DESC blend_desc = hal::CreateOpaqueBlendDesc();
	if (FAILED(device->CreateBlendState(&blend_desc, &g_BlendState)))
	{
		ReleaseResources();
		return false;
	}

	D3D11_DEPTH_STENCIL_DESC depth_desc = hal::CreateDepthDisabledDesc(D3D11_COMPARISON_ALWAYS);
	if (FAILED(device->CreateDepthStencilState(&depth_desc, &g_DepthStencilState)))
	{
		ReleaseResources();
		return false;
	}

	D3D11_RASTERIZER_DESC rasterizer_desc = hal::CreateRasterizerDesc(D3D11_CULL_NONE);
	if (FAILED(device->CreateRasterizerState(&rasterizer_desc, &g_RasterizerState)))
	{
		ReleaseResources();
		return false;
	}

	TimeStopEffect_Cancel();
	return true;
}

void TimeStopEffect_Finalize()
{
	ReleaseResources();
	g_Phase = EffectPhase::Inactive;
}

void TimeStopEffect_BeginCapture()
{
	if (!g_Context || !g_SceneRenderTarget)
	{
		return;
	}
	ID3D11ShaderResourceView* null_resource = nullptr;
	g_Context->PSSetShaderResources(0, 1, &null_resource);
	const float clear_color[4] = { 0.2f, 0.4f, 0.8f, 1.0f };
	g_Context->ClearRenderTargetView(g_SceneRenderTarget, clear_color);
	g_Context->OMSetRenderTargets(1, &g_SceneRenderTarget, g_DepthStencil);
}

void TimeStopEffect_EndCapture()
{
	if (!g_Context || !g_SceneShaderResource)
	{
		return;
	}
	g_Context->OMSetRenderTargets(1, &g_BackBuffer, nullptr);

	PixelConstants constants{};
	constants.CenterUv = g_CenterUv;
	constants.Resolution = { static_cast<float>(g_Width), static_cast<float>(g_Height) };
	constants.Time = g_ElapsedTime;
	constants.MaxRadius = g_MaxRadius;
	FillAnimatedConstants(constants);
	g_Context->UpdateSubresource(g_PixelConstantBuffer, 0, nullptr, &constants, 0, 0);

	ID3D11Buffer* null_buffer = nullptr;
	UINT zero = 0;
	g_Context->IASetVertexBuffers(0, 1, &null_buffer, &zero, &zero);
	g_Context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	g_Context->IASetInputLayout(nullptr);
	g_Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	g_Context->VSSetShader(g_VertexShader, nullptr, 0);
	g_Context->PSSetShader(g_PixelShader, nullptr, 0);
	g_Context->PSSetConstantBuffers(0, 1, &g_PixelConstantBuffer);
	g_Context->PSSetShaderResources(0, 1, &g_SceneShaderResource);
	g_Context->PSSetSamplers(0, 1, &g_SamplerState);
	g_Context->OMSetBlendState(g_BlendState, nullptr, 0xffffffff);
	g_Context->OMSetDepthStencilState(g_DepthStencilState, 0);
	g_Context->RSSetState(g_RasterizerState);
	g_Context->Draw(3, 0);

	ID3D11ShaderResourceView* null_resource = nullptr;
	g_Context->PSSetShaderResources(0, 1, &null_resource);
}

void TimeStopEffect_Trigger(const DirectX::XMFLOAT2& screen_position)
{
	if (g_Phase != EffectPhase::Inactive)
	{
		return;
	}
	g_CenterUv.x = Saturate(screen_position.x / static_cast<float>(g_Width));
	g_CenterUv.y = Saturate(screen_position.y / static_cast<float>(g_Height));

	const float aspect = static_cast<float>(g_Width) / static_cast<float>(g_Height);
	g_MaxRadius = 0.0f;
	const std::initializer_list<DirectX::XMFLOAT2> corners = {
		{ 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f }
	};
	for (const DirectX::XMFLOAT2& corner : corners)
	{
		const float dx = (corner.x - g_CenterUv.x) * aspect;
		const float dy = corner.y - g_CenterUv.y;
		g_MaxRadius = std::max(g_MaxRadius, Length({ dx, dy }));
	}
	g_MaxRadius += 0.06f;
	g_ElapsedTime = 0.0f;
	g_Phase = EffectPhase::WaveOut;
}

void TimeStopEffect_Update(float delta_time)
{
	delta_time = std::max(delta_time, 0.0f);
	static constexpr float EFFECT_DURATION = TimeStopTuning::WAVE_OUT_DURATION + TimeStopTuning::WAVE_RETURN_DURATION +
	                                         TimeStopTuning::STOP_HOLD_DURATION + TimeStopTuning::RESTORE_DURATION;

	if (g_Phase == EffectPhase::Inactive)
	{
		return;
	}
	g_ElapsedTime += delta_time;
	if (g_ElapsedTime < TimeStopTuning::WAVE_OUT_DURATION)
	{
		g_Phase = EffectPhase::WaveOut;
	}
	else if (g_ElapsedTime < TimeStopTuning::WAVE_OUT_DURATION + TimeStopTuning::WAVE_RETURN_DURATION)
	{
		g_Phase = EffectPhase::WaveReturn;
	}
	else if (g_ElapsedTime < TimeStopTuning::WAVE_OUT_DURATION + TimeStopTuning::WAVE_RETURN_DURATION +
	                             TimeStopTuning::STOP_HOLD_DURATION)
	{
		g_Phase = EffectPhase::Hold;
	}
	else if (g_ElapsedTime < EFFECT_DURATION)
	{
		g_Phase = EffectPhase::Restore;
	}
	else
	{
		TimeStopEffect_Cancel();
	}
}

void TimeStopEffect_Cancel()
{
	g_ElapsedTime = 0.0f;
	g_Phase = EffectPhase::Inactive;
}

bool TimeStopEffect_IsActive()
{
	return g_Phase != EffectPhase::Inactive;
}

bool TimeStopEffect_IsRestoring()
{
	return g_Phase == EffectPhase::Restore;
}
