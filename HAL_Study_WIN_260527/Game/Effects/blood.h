#ifndef BLOOD_H
#define BLOOD_H

#include <DirectXMath.h>

namespace Blood
{
	bool Initialize();
	void Finalize();
	void Clear();
	void Spawn(const DirectX::XMFLOAT2& position);
	void Update(float delta_time);
	void Draw();
} // namespace Blood

#endif // BLOOD_H
