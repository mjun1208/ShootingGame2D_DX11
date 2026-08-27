/*==============================================================================

   Direct3D11用 デバッグテキスト表示 [debug_text.h]
														 Author : Youhei Sato
														 Date   : 2025/06/15
--------------------------------------------------------------------------------

==============================================================================*/
#ifndef DEBUG_TEXT_H
#define DEBUG_TEXT_H

#include <d3d11.h>
#include <unordered_map>
#include <string>
#include <tuple>
#include <list>
#include <wrl/client.h> // Microsoft::WRL::ComPtrを使用するために必要
#include <DirectXMath.h>


namespace hal
{
	class DebugText
	{
	private:
		// 参照インターフェース（外部から借りるものなのでRelease不要）
		ID3D11Device* m_pDevice = nullptr;
		ID3D11DeviceContext* m_pContext = nullptr;

		float m_OffsetX{ 0.0f }; // オフセットX座標
		float m_OffsetY{ 0.0f }; // オフセットY座標
		ULONG m_MaxLine{ 0 }; // 最大行数
		ULONG m_MaxCharactersPerLine{ 0 }; // 1行あたりの最大文字数
		float m_LineSpacing{ 0.0f }; // 行の間隔
		float m_CharacterSpacing{ 0.0f }; // 文字の間隔

		struct Characters { 
			Characters(const DirectX::XMFLOAT4& color) : color(color) {}
			std::u32string characters;
			DirectX::XMFLOAT4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
		};

		struct LineStrings {
			std::list<Characters> strings;
			ULONG characterCount{ 0 };
			ULONG spaceCount{ 0 };
		};

		std::list<LineStrings> m_TextLines; // 表示するテキスト行のリスト
		UINT m_CharacterCount{ 0 }; // 表示文字の総数（タブや改行などを除く）

		// フォントテクスチャ関連
		std::wstring m_FileName; // ファイル名
		ID3D11Resource* m_pTexture = nullptr;
		ID3D11ShaderResourceView* m_pTextureView = nullptr;
		UINT m_TextureWidth{ 0 }; // テクスチャの幅
		UINT m_TextureHeight{ 0 }; // テクスチャの高さ
		UINT m_AtlasColumns{ 16 };
		UINT m_AtlasRows{ 16 };
		std::unordered_map<char32_t, UINT> m_GlyphIndices;

		// テクスチャ管理用のMAP
		static std::unordered_map<std::wstring, std::tuple<ID3D11Resource*, ID3D11ShaderResourceView*>> m_TextureMap;

		Microsoft::WRL::ComPtr<ID3D11Buffer> m_pVertexBuffer; // 頂点バッファ
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_pIndexBuffer; // インデックスバッファ
		UINT m_BufferSourceCharacterCount{ 0 }; // バッファに登録された最大文字数

		static Microsoft::WRL::ComPtr<ID3D11BlendState> m_pBlendState; // ブレンドステート
		static Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_pDepthStencilState; // 奥行きステンシルステート
		static Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_pRasterizerState; // ラスタライザステート

		static Microsoft::WRL::ComPtr<ID3D11VertexShader> m_pVertexShader; // 頂点シェーダー
		static Microsoft::WRL::ComPtr<ID3D11InputLayout> m_pInputLayout; // 入力レイアウト
		static Microsoft::WRL::ComPtr<ID3D11Buffer> m_pVSConstantBuffer; // 頂点シェーダー定数バッファ
		static Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pPixelShader; // ピクセルシェーダー
		static Microsoft::WRL::ComPtr<ID3D11SamplerState> m_pSamplerState; // サンプラーステート

	public:
		DebugText() = delete;
		DebugText(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, const wchar_t* pFontTextureFileName, UINT screenWidth, UINT screenHeight, float offsetX = 0.0f, float offsetY = 0.0f, ULONG maxLine = 0, ULONG maxCharactersPerLine = 0, float lineSpacing = 0.0f, float characterSpacing = 0.0f);
		~DebugText();

		// 文字列の登録（描画可能な文字、改行、タブなどに対応）
		void SetText(const char* pText, DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f });
		static std::size_t CountUtf8Characters(const char* pText);

		void Draw();

		void Clear(); // 登録されたテキストをクリア

	private:
		
		struct Vertex
		{
			DirectX::XMFLOAT3 position; // 座標
			DirectX::XMFLOAT4 color;    // 色
			DirectX::XMFLOAT2 texcoord; // テクスチャ座標
		};

		void createBuffer(ULONG characterCount);
	};
}
#endif // DEBUG_TEXT_H
