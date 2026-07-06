#ifndef TEXTURE_H
#define TEXTURE_H
#include <d3d11.h>
#include <DirectXMath.h>

// 蛻晄悄蛹悶→邨ゆｺ・・逅・
void Texture_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Texture_Finalize();

// 繝・け繧ｹ繝√Ε縺ｮ隱ｭ縺ｿ霎ｼ縺ｿ
// 謌ｻ繧雁､・壹ユ繧ｯ繧ｹ繝√ΕID・亥､ｱ謨励＠縺溷ｴ蜷医・ TEXTURE_INVALID_ID 繧定ｿ斐☆・・
int Texture_Load(const wchar_t* pFileName, bool bMipMap = true);
constexpr int TEXTURE_INVALID_ID = -1;

// 繝・け繧ｹ繝√Ε縺ｮ隗｣謾ｾ
void Texture_Release(int texture_id);
void Texture_Release(const int* pTextureIDs, int count);
void Texture_AllRelease();

// 謠冗判譎ゅ・繝・け繧ｹ繝√Ε險ｭ螳・
void Texture_SetTexture(int texture_id);
void Texture_SetTextureSlot(int texture_id, unsigned int slot);

// 繝・け繧ｹ繝√Ε諠・ｱ縺ｮ蜿門ｾ・
DirectX::XMUINT2 Texture_GetSize(int texture_id);
unsigned int Texture_GetWidth(int texture_id);
unsigned int Texture_GetHeight(int texture_id);

#endif // TEXTURE_H