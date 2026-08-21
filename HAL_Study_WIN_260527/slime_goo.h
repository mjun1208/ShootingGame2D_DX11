#ifndef SLIME_GOO_H
#define SLIME_GOO_H

#include <DirectXMath.h>

#include <array>
#include <cstddef>
#include <cstdint>

class cSlimeGoo final
{
public:
	static cSlimeGoo& GetInstance();

	bool Initialize();
	void Finalize();
	void Clear();
	void Spawn(
		const DirectX::XMFLOAT2& position,
		const DirectX::XMFLOAT2& impact_direction = { 0.0f, 0.0f },
		float intensity = 1.0f);
	void Update(float delta_time);
	void DrawGround() const;
	void DrawBurst() const;

private:
	struct Puddle
	{
		DirectX::XMFLOAT2 Position{};
		DirectX::XMFLOAT2 Size{};
		float Rotation{ 0.0f };
		float Age{ 0.0f };
		bool IsActive{ false };
	};

	struct Droplet
	{
		DirectX::XMFLOAT2 Position{};
		DirectX::XMFLOAT2 Velocity{};
		float Size{ 0.0f };
		float Rotation{ 0.0f };
		float AngularVelocity{ 0.0f };
		float Age{ 0.0f };
		float Lifetime{ 0.0f };
		bool IsActive{ false };
	};

	struct Pop
	{
		DirectX::XMFLOAT2 Position{};
		float StartSize{ 0.0f };
		float EndSize{ 0.0f };
		float Rotation{ 0.0f };
		float Age{ 0.0f };
		float Lifetime{ 0.0f };
		bool IsActive{ false };
	};

	static constexpr std::size_t PUDDLE_CAPACITY = 500;
	static constexpr std::size_t DROPLET_CAPACITY = 4096;
	static constexpr std::size_t POP_CAPACITY = 512;

	cSlimeGoo() = default;
	~cSlimeGoo() = default;
	cSlimeGoo(const cSlimeGoo&) = delete;
	cSlimeGoo& operator=(const cSlimeGoo&) = delete;

	float NextRandom();
	void SpawnPuddle(const DirectX::XMFLOAT2& position, float intensity);

	std::array<Puddle, PUDDLE_CAPACITY> m_Puddles{};
	std::array<Droplet, DROPLET_CAPACITY> m_Droplets{};
	std::array<Pop, POP_CAPACITY> m_Pops{};
	std::size_t m_PuddleCursor{ 0 };
	std::size_t m_DropletCursor{ 0 };
	std::size_t m_PopCursor{ 0 };
	int m_TextureID{ -1 };
	std::uint32_t m_RandomState{ 0x6D2B79F5u };
	bool m_IsInitialized{ false };
};

#endif // SLIME_GOO_H
