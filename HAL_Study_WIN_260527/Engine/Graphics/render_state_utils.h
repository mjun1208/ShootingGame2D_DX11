#ifndef RENDER_STATE_UTILS_H
#define RENDER_STATE_UTILS_H

#include <d3d11.h>

namespace hal
{
	inline D3D11_SAMPLER_DESC CreateSamplerDesc(D3D11_FILTER filter, D3D11_TEXTURE_ADDRESS_MODE address_u,
	                                            D3D11_TEXTURE_ADDRESS_MODE address_v,
	                                            D3D11_TEXTURE_ADDRESS_MODE address_w,
	                                            D3D11_COMPARISON_FUNC comparison = D3D11_COMPARISON_NEVER,
	                                            UINT max_anisotropy = 0)
	{
		D3D11_SAMPLER_DESC desc{};
		desc.Filter = filter;
		desc.AddressU = address_u;
		desc.AddressV = address_v;
		desc.AddressW = address_w;
		desc.MaxAnisotropy = max_anisotropy;
		desc.ComparisonFunc = comparison;
		desc.MaxLOD = D3D11_FLOAT32_MAX;
		return desc;
	}

	inline D3D11_SAMPLER_DESC CreateSamplerDesc(D3D11_FILTER filter, D3D11_TEXTURE_ADDRESS_MODE address,
	                                            D3D11_COMPARISON_FUNC comparison = D3D11_COMPARISON_NEVER,
	                                            UINT max_anisotropy = 0)
	{
		return CreateSamplerDesc(filter, address, address, address, comparison, max_anisotropy);
	}

	inline D3D11_BLEND_DESC CreateBlendDesc(D3D11_BLEND source, D3D11_BLEND destination,
	                                        D3D11_BLEND source_alpha = D3D11_BLEND_ONE,
	                                        D3D11_BLEND destination_alpha = D3D11_BLEND_ZERO)
	{
		D3D11_BLEND_DESC desc{};
		auto& target = desc.RenderTarget[0];
		target.BlendEnable = TRUE;
		target.SrcBlend = source;
		target.DestBlend = destination;
		target.BlendOp = D3D11_BLEND_OP_ADD;
		target.SrcBlendAlpha = source_alpha;
		target.DestBlendAlpha = destination_alpha;
		target.BlendOpAlpha = D3D11_BLEND_OP_ADD;
		target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		return desc;
	}

	inline D3D11_BLEND_DESC CreateAlphaBlendDesc()
	{
		return CreateBlendDesc(D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA);
	}

	inline D3D11_BLEND_DESC CreateAdditiveBlendDesc()
	{
		return CreateBlendDesc(D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_ONE);
	}

	inline D3D11_BLEND_DESC CreateOneOneBlendDesc()
	{
		return CreateBlendDesc(D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_ONE);
	}

	inline D3D11_BLEND_DESC CreateOpaqueBlendDesc()
	{
		D3D11_BLEND_DESC desc{};
		desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		return desc;
	}

	inline D3D11_DEPTH_STENCIL_DESC CreateDepthDisabledDesc(D3D11_COMPARISON_FUNC comparison)
	{
		D3D11_DEPTH_STENCIL_DESC desc{};
		desc.DepthEnable = FALSE;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		desc.DepthFunc = comparison;
		desc.StencilEnable = FALSE;
		return desc;
	}

	inline D3D11_RASTERIZER_DESC CreateRasterizerDesc(D3D11_CULL_MODE cull_mode)
	{
		D3D11_RASTERIZER_DESC desc{};
		desc.FillMode = D3D11_FILL_SOLID;
		desc.CullMode = cull_mode;
		desc.DepthClipEnable = TRUE;
		return desc;
	}
} // namespace hal

#endif // RENDER_STATE_UTILS_H
