#ifndef TRAIL_H
#define TRAIL_H

#include <DirectXMath.h>

constexpr int TRAIL_INVALID_ID = -1;
constexpr int TRAIL_MAX = 32768;

struct cTrailDesc
{
	DirectX::XMFLOAT2 Position{ 0.0f, 0.0f };
	float Width{ 16.0f };
	float Height{ 16.0f };
	float StartScale{ 1.0f };
	float EndScale{ 0.25f };
	float Rotation{ 0.0f };
	float LifeTime{ 0.2f };
	int TextureID{ -1 };
	DirectX::XMFLOAT4 Color{ 1.0f, 1.0f, 1.0f, 0.5f };
};

struct cTrailParticle
{
	bool IsActive{ false };
	DirectX::XMFLOAT2 Position{ 0.0f, 0.0f };
	float Width{ 16.0f };
	float Height{ 16.0f };
	float StartScale{ 1.0f };
	float EndScale{ 0.25f };
	float Rotation{ 0.0f };
	float Age{ 0.0f };
	float LifeTime{ 0.2f };
	int TextureID{ -1 };
	DirectX::XMFLOAT4 Color{ 1.0f, 1.0f, 1.0f, 0.5f };
};

void TrailSystem_Initialize();
void TrailSystem_Finalize();
void TrailSystem_Clear();
int TrailSystem_Emit(const cTrailDesc& desc);
void TrailSystem_Update(float delta_time);
void TrailSystem_Draw();
void TrailSystem_Deactivate(int trail_id);
bool TrailSystem_IsActive(int trail_id);
int TrailSystem_GetActiveCount();
int TrailSystem_GetCapacity();

#endif // !TRAIL_H
