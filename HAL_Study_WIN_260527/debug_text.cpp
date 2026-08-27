/*==============================================================================

   Direct3D11用 デバッグテキスト?示 [debug_text.cpp]
														 Author : Youhei Sato
														 Date   : 2025/06/15
--------------------------------------------------------------------------------

==============================================================================*/
#include "debug_text.h"
#include "WICTextureLoader11.h"
using namespace DirectX;
#include <D3Dcompiler.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
using namespace Microsoft::WRL;

#pragma comment(lib, "d3dcompiler.lib")

namespace
{
	char32_t DecodeNextUtf8(const char*& text)
	{
		const auto* bytes = reinterpret_cast<const unsigned char*>(text);
		if (bytes[0] < 0x80)
		{
			++text;
			return bytes[0];
		}
		int continuation_count = 0;
		char32_t codepoint = 0;
		if ((bytes[0] & 0xE0) == 0xC0)
		{
			continuation_count = 1;
			codepoint = bytes[0] & 0x1F;
		}
		else if ((bytes[0] & 0xF0) == 0xE0)
		{
			continuation_count = 2;
			codepoint = bytes[0] & 0x0F;
		}
		else if ((bytes[0] & 0xF8) == 0xF0)
		{
			continuation_count = 3;
			codepoint = bytes[0] & 0x07;
		}
		else
		{
			++text;
			return U'?';
		}
		for (int index = 1; index <= continuation_count; ++index)
		{
			if ((bytes[index] & 0xC0) != 0x80)
			{
				++text;
				return U'?';
			}
			codepoint = (codepoint << 6) | (bytes[index] & 0x3F);
		}
		text += continuation_count + 1;
		return codepoint <= 0x10FFFF ? codepoint : U'?';
	}
}


namespace hal
{
	std::unordered_map<std::wstring, std::tuple<ID3D11Resource*, ID3D11ShaderResourceView*>> DebugText::m_TextureMap;
	ComPtr<ID3D11BlendState> DebugText::m_pBlendState;
	ComPtr<ID3D11DepthStencilState> DebugText::m_pDepthStencilState;
	ComPtr<ID3D11RasterizerState> DebugText::m_pRasterizerState;

	ComPtr<ID3D11VertexShader> DebugText::m_pVertexShader;
	ComPtr<ID3D11InputLayout> DebugText::m_pInputLayout;
	ComPtr<ID3D11Buffer> DebugText::m_pVSConstantBuffer;
	ComPtr<ID3D11PixelShader> DebugText::m_pPixelShader;
	ComPtr<ID3D11SamplerState> DebugText::m_pSamplerState;

	DebugText::DebugText(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, const wchar_t* pFontTextureFileName, UINT screenWidth, UINT screenHeight, float offsetX, float offsetY, ULONG maxLine, ULONG maxCharactersPerLine, float lineSpacing, float characterSpacing)
		: m_pDevice(pDevice), m_pContext(pContext), m_FileName(pFontTextureFileName), m_OffsetX(offsetX), m_OffsetY(offsetY), m_MaxLine(maxLine), m_MaxCharactersPerLine(maxCharactersPerLine), m_LineSpacing(lineSpacing), m_CharacterSpacing(characterSpacing)
	{
		auto it = m_TextureMap.find(pFontTextureFileName);

		if (it != m_TextureMap.end()) {
			m_pTexture = std::get<0>(it->second);
			m_pTextureView = std::get<1>(it->second);
			m_pTexture->AddRef();
			m_pTextureView->AddRef();
		}
		else {
			if (FAILED(CreateWICTextureFromFile(pDevice, pFontTextureFileName, &m_pTexture, &m_pTextureView))) {
				MessageBoxW(nullptr, L"フォントテクス?ャの読み込みに失敗しました", pFontTextureFileName, MB_OK | MB_ICONERROR);
				return;
			}

			m_TextureMap[pFontTextureFileName] = std::make_tuple(m_pTexture, m_pTextureView);
		}

		D3D11_TEXTURE2D_DESC texture2d_desc;
		((ID3D11Texture2D*)m_pTexture)->GetDesc(&texture2d_desc);
		m_TextureWidth = texture2d_desc.Width;
		m_TextureHeight = texture2d_desc.Height;

		for (char32_t codepoint = U' '; codepoint <= U'~'; ++codepoint)
		{
			m_GlyphIndices[codepoint] = static_cast<UINT>(codepoint - U' ');
		}
		std::ifstream glyph_map(std::filesystem::path(m_FileName + L".glyphs"));
		if (glyph_map)
		{
			UINT columns = 0;
			UINT rows = 0;
			if (glyph_map >> columns >> rows && columns > 0 && rows > 0)
			{
				m_AtlasColumns = columns;
				m_AtlasRows = rows;
				m_GlyphIndices.clear();
				UINT glyph_index = 0;
				std::uint32_t codepoint = 0;
				while (glyph_map >> std::hex >> codepoint)
				{
					m_GlyphIndices[static_cast<char32_t>(codepoint)] = glyph_index++;
				}
			}
		}

		if (!m_LineSpacing) {
			m_LineSpacing = m_TextureHeight / static_cast<float>(m_AtlasRows);
		}

		if (!m_CharacterSpacing) {
			m_CharacterSpacing = m_TextureWidth / static_cast<float>(m_AtlasColumns);
		}

		m_TextLines.emplace_back();

		if (!m_pBlendState) {
			// ブレンドステ?トの作成
			D3D11_BLEND_DESC blend_desc{};
			blend_desc.RenderTarget[0].BlendEnable = TRUE;
			blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
			blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
			blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

			m_pDevice->CreateBlendState(&blend_desc, m_pBlendState.GetAddressOf());
		}

		if (!m_pDepthStencilState) {
			// 奥行きステンシルステ?トの作成
			D3D11_DEPTH_STENCIL_DESC depth_stencil_desc{};
			depth_stencil_desc.DepthFunc = D3D11_COMPARISON_LESS;
			depth_stencil_desc.StencilEnable = FALSE;
			depth_stencil_desc.DepthEnable = FALSE; // Zバッフ?無効
			depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;

			m_pDevice->CreateDepthStencilState(&depth_stencil_desc, m_pDepthStencilState.GetAddressOf());
		}

		if (!m_pRasterizerState) {
			// ラス?ライザステ?トの作成
			D3D11_RASTERIZER_DESC rasterizer_desc{};
			rasterizer_desc.FillMode = D3D11_FILL_SOLID;
			rasterizer_desc.CullMode = D3D11_CULL_BACK;
			rasterizer_desc.DepthClipEnable = TRUE; // Zクリッピング有効

			m_pDevice->CreateRasterizerState(&rasterizer_desc, m_pRasterizerState.GetAddressOf());
		}

		if (!m_pVertexShader) {

			// 頂?シェ???の作成
			static const char* vs_text = R"(
				float4x4 mtx;

				struct VS_IN
				{
					float4 posL  : POSITION0;
					float4 color : COLOR0;
					float2 uv    : TEXCOORD0;
				};

				struct VS_OUT
				{
					float4 posH  : SV_POSITION;
					float4 color : COLOR0;
					float2 uv    : TEXCOORD0;
				};

				VS_OUT main(VS_IN vsin)
				{
					VS_OUT vsout;
    
					vsout.posH = mul(vsin.posL, mtx);
					vsout.color = vsin.color;
					vsout.uv = vsin.uv;
    
					return vsout;
				}
			)";

			ComPtr<ID3DBlob> pVSBlob;
			D3DCompile(vs_text, strlen(vs_text), nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, pVSBlob.GetAddressOf(), nullptr);
			m_pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, m_pVertexShader.GetAddressOf());

			// 頂?シェ???入力レイアウトの定?
			D3D11_INPUT_ELEMENT_DESC layout[] = {
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 }
			};
			UINT numElements = ARRAYSIZE(layout);

			// 入力レイアウトの作成
			m_pDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), m_pInputLayout.GetAddressOf());
		}

		if (!m_pVSConstantBuffer) {
			// 定数バッフ?の作成
			D3D11_BUFFER_DESC constant_buffer_desc = {};
			constant_buffer_desc.ByteWidth = sizeof(XMFLOAT4X4);
			constant_buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

			m_pDevice->CreateBuffer(&constant_buffer_desc, nullptr, m_pVSConstantBuffer.GetAddressOf());

			// 定数バッフ?の初期化（平行投影行列を設定）
			XMFLOAT4X4 mtx;
			XMStoreFloat4x4(&mtx, XMMatrixTranspose(XMMatrixOrthographicOffCenterLH(0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 0.0f, 1.0f)));
			m_pContext->UpdateSubresource(m_pVSConstantBuffer.Get(), 0, nullptr, &mtx, 0, 0);
		}

		if (!m_pPixelShader) {
			// ピクセルシェ???の作成
			static const char* ps_text = R"(
				struct PS_INPUT
				{
					float4 posH  : SV_POSITION;
					float4 color : COLOR0;
					float2 uv    : TEXCOORD0;
				};
				Texture2D fontTexture : register(t0);
				SamplerState fontSampler : register(s0);
				float4 main(PS_INPUT psin) : SV_TARGET
				{
					float4 color = fontTexture.Sample(fontSampler, psin.uv);
					return color * psin.color; // テクス?ャのアルフ?成分と頂?カラ?を?け合わせる
				}
			)";
			ComPtr<ID3DBlob> pPSBlob;
			D3DCompile(ps_text, strlen(ps_text), nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, pPSBlob.GetAddressOf(), nullptr);
			m_pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, m_pPixelShader.GetAddressOf());
		}

		if( !m_pSamplerState) {
			// サンプラ?ステ?トの作成
			D3D11_SAMPLER_DESC sampler_desc = {};
			sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
			sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
			sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
			sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
			sampler_desc.MinLOD = 0.0f;
			sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
			m_pDevice->CreateSamplerState(&sampler_desc, m_pSamplerState.GetAddressOf());
		}
	}

	DebugText::~DebugText()
	{
		// テクス?ャとテクス?ャビュ?の解放
		if (m_pTextureView) {
			m_pTexture->Release();
			if(!m_pTextureView->Release()) {
				auto it = m_TextureMap.find(m_FileName);
				if (it != m_TextureMap.end()) {
					m_TextureMap.erase(it);
				}
			}
		}
	}

	void DebugText::SetText(const char* pText, XMFLOAT4 color)
	{
		if (!pText)
		{
			return;
		}
		m_TextLines.back().strings.emplace_back(color);
		const char* cursor = pText;
		while (*cursor)
		{
			char32_t codepoint = DecodeNextUtf8(cursor);
			if (codepoint == U'\n')
			{
				m_TextLines.emplace_back();
				m_TextLines.back().strings.emplace_back(color);
				continue;
			}
			if (codepoint == U'\r')
			{
				continue;
			}
			if (codepoint == U'\t')
			{
				do
				{
					if (m_MaxCharactersPerLine &&
						m_TextLines.back().characterCount >= m_MaxCharactersPerLine)
					{
						m_TextLines.emplace_back();
						m_TextLines.back().strings.emplace_back(color);
						break;
					}
					m_TextLines.back().strings.back().characters += U' ';
					++m_TextLines.back().characterCount;
					++m_TextLines.back().spaceCount;
				} while (m_TextLines.back().characterCount % 4 != 0);
				continue;
			}

			if (m_MaxCharactersPerLine &&
				m_TextLines.back().characterCount >= m_MaxCharactersPerLine)
			{
				m_TextLines.emplace_back();
				m_TextLines.back().strings.emplace_back(color);
			}
			if (m_GlyphIndices.find(codepoint) == m_GlyphIndices.end())
			{
				codepoint = U'?';
			}
			m_TextLines.back().strings.back().characters += codepoint;
			if (codepoint == U' ')
			{
				++m_TextLines.back().spaceCount;
			}
			else
			{
				++m_CharacterCount;
			}
			++m_TextLines.back().characterCount;
		}

		int last_line_count = m_TextLines.back().characterCount ? 0 : -1;

		while(m_MaxLine && m_TextLines.size() + last_line_count > m_MaxLine) {
			ULONG remove_character_count = m_TextLines.front().characterCount - m_TextLines.front().spaceCount;
			m_CharacterCount -= remove_character_count; // 最大行数を超えたら古い行を削除
			m_TextLines.pop_front();
		}
	}

	std::size_t DebugText::CountUtf8Characters(const char* pText)
	{
		if (!pText)
		{
			return 0;
		}
		std::size_t count = 0;
		const char* cursor = pText;
		while (*cursor)
		{
			DecodeNextUtf8(cursor);
			++count;
		}
		return count;
	}

	void DebugText::Draw()
	{
		if( !m_CharacterCount){
			return; // ?画文字がない場合は何もしない
		}

		if (!m_pVertexBuffer || m_CharacterCount > m_BufferSourceCharacterCount) {
			createBuffer(m_CharacterCount);
		}

		// 頂?バッフ?とインデックスバッフ?のロック
		D3D11_MAPPED_SUBRESOURCE msr;
		m_pContext->Map(m_pVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

		// 頂?バッフ?の書き込み?イン?を取得
		Vertex* v = (Vertex*)msr.pData;

		m_pContext->Map(m_pIndexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

		// インデックスバッフ?の書き込み?イン?を取得
		WORD* indices = (WORD*)msr.pData;

		// 頂?情報の?築
		UINT lineCount = 0;
		WORD characterCount = 0;
		const float characterWidth =
			m_TextureWidth / static_cast<float>(m_AtlasColumns);
		const float characterHeight =
			m_TextureHeight / static_cast<float>(m_AtlasRows);

		for (const auto& strings : m_TextLines) {

			UINT columnCount = 0;

			for (const auto& string : strings.strings) {

				for (const auto& code : string.characters) {

					const auto glyph = m_GlyphIndices.find(code);

					if (code != U' ' && glyph != m_GlyphIndices.end()) {
						const UINT index = glyph->second;
						float u0 = (index % m_AtlasColumns) /
							static_cast<float>(m_AtlasColumns);
						float v0 = (index / m_AtlasColumns) /
							static_cast<float>(m_AtlasRows);
						float u1 = (index % m_AtlasColumns + 1) /
							static_cast<float>(m_AtlasColumns);
						float v1 = (index / m_AtlasColumns + 1) /
							static_cast<float>(m_AtlasRows);
						float x = m_OffsetX + columnCount * m_CharacterSpacing;
						float y = m_OffsetY + lineCount * m_LineSpacing;

						v[0].position = { x, y, 1.0f };
						v[0].color = string.color;
						v[0].texcoord = { u0, v0 };

						v[1].position = { x + characterWidth, y, 1.0f };
						v[1].color = string.color;
						v[1].texcoord = { u1, v0 };

						v[2].position = { x, y + characterHeight, 1.0f };
						v[2].color = string.color;
						v[2].texcoord = { u0, v1 };

						v[3].position = { x + characterWidth, y + characterHeight, 1.0f };
						v[3].color = string.color;
						v[3].texcoord = { u1, v1 };

						v += 4;

						indices[0] = characterCount * 4 + 0;
						indices[1] = characterCount * 4 + 1;
						indices[2] = characterCount * 4 + 2;
						indices[3] = characterCount * 4 + 2;
						indices[4] = characterCount * 4 + 1;
						indices[5] = characterCount * 4 + 3;
						indices += 6;

						characterCount++;
					}

					columnCount++;
				}
			}

			lineCount++;
		}

		// 頂?バッフ?とインデックスバッフ?のアンロック
		m_pContext->Unmap(m_pVertexBuffer.Get(), 0);
		m_pContext->Unmap(m_pIndexBuffer.Get(), 0);

		// 頂?バッフ?をパイプラインにセット
		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		m_pContext->IASetVertexBuffers(0, 1, m_pVertexBuffer.GetAddressOf(), &stride, &offset);

		// インデックスバッフ?をパイプラインにセット
		m_pContext->IASetIndexBuffer(m_pIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
		
		// 頂?シェ???をパイプラインにセット
		m_pContext->VSSetShader(m_pVertexShader.Get(), nullptr, 0);

		// 定数バッフ?をパイプラインにセット
		m_pContext->VSSetConstantBuffers(0, 1, m_pVSConstantBuffer.GetAddressOf());

		// 入力レイアウトをパイプラインにセット
		m_pContext->IASetInputLayout(m_pInputLayout.Get());

		// ピクセルシェ???、テクス?ャ、サンプラ?ステ?トをセット
		m_pContext->PSSetShader(m_pPixelShader.Get(), nullptr, 0);
		m_pContext->PSSetShaderResources(0, 1, &m_pTextureView);
		m_pContext->PSSetSamplers(0, 1, m_pSamplerState.GetAddressOf());

		ComPtr<ID3D11BlendState> pPreviousBlendState; // 以前のブレンドステ?ト
		float previous_blend_factor[4];
		UINT previous_sample_mask;
		m_pContext->OMGetBlendState(pPreviousBlendState.GetAddressOf(), previous_blend_factor, &previous_sample_mask);

		// アルフ?ブレンド用のステ?トに変更
		float blend_factor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		m_pContext->OMSetBlendState(m_pBlendState.Get(), blend_factor, 0xffffffff);

		ComPtr<ID3D11DepthStencilState> pPreviousDepthStencilState; // 以前のデプスステンシルステ?ト
		UINT previous_stencil_ref = 0;
		m_pContext->OMGetDepthStencilState(pPreviousDepthStencilState.GetAddressOf(), &previous_stencil_ref);

		// ?度バッフ?を無効化（Zバッフ?への書き込みとテストを行わない）
		m_pContext->OMSetDepthStencilState(m_pDepthStencilState.Get(), 0);

		// ラス?ライザステ?トを設定
		ComPtr<ID3D11RasterizerState> pPreviousRasterizerState; // 以前のラス?ライザステ?ト
		m_pContext->RSGetState(pPreviousRasterizerState.GetAddressOf());
		m_pContext->RSSetState(m_pRasterizerState.Get());

		// プリ?ティブト?ロジ?を設定
		m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// インデックス?画を実行
		m_pContext->DrawIndexed(m_CharacterCount * 6, 0, 0);

		// ?画後、設定を元に戻す
		m_pContext->OMSetBlendState(pPreviousBlendState.Get(), previous_blend_factor, 0xffffffff);
		m_pContext->OMSetDepthStencilState(pPreviousDepthStencilState.Get(), previous_stencil_ref);
		m_pContext->RSSetState(pPreviousRasterizerState.Get());
	}

	void DebugText::Clear()
	{
		m_TextLines.clear();
		m_TextLines.emplace_back(); // 空の行を追加して?備
		m_CharacterCount = 0;
	}

	void DebugText::createBuffer(ULONG characterCount)
	{
		// 頂?バッフ?の作成
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = sizeof(Vertex) * characterCount * 4;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		m_pDevice->CreateBuffer(&bd, NULL, m_pVertexBuffer.ReleaseAndGetAddressOf());

		// インデックスバッフ?の作成
		bd.ByteWidth = sizeof(WORD) * characterCount * 6;
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;

		m_pDevice->CreateBuffer(&bd, NULL, m_pIndexBuffer.ReleaseAndGetAddressOf());

		m_BufferSourceCharacterCount = characterCount;
	}

}
