#include "game_damage_text.h"

#include "math_utils.h"
#include "sprite_instanced.h"
#include "texture.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace
{

	namespace DamageTextTuning::Motion
	{
		constexpr float Lifetime = 0.85f;
	} // namespace DamageTextTuning::Motion

	namespace DamageTextTuning::Pool
	{
		constexpr int Max = 64;
	} // namespace DamageTextTuning::Pool

} // namespace

namespace
{

	struct DamageTextEntry
	{
		DirectX::XMFLOAT2 OriginPosition{};
		DirectX::XMFLOAT2 Position{};
		DirectX::XMFLOAT2 Velocity{};
		DirectX::XMFLOAT4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		std::string Text;
		float Age{ 0.0f };
		int StackLevel{ 0 };
	};

	int g_FontTextureID = TEXTURE_INVALID_ID;
	std::vector<DamageTextEntry> g_Entries;
	unsigned int g_SpawnSerial = 0;

	std::string FormatDamage(float damage)
	{
		char buffer[24]{};
		std::snprintf(buffer, sizeof(buffer), "%.0f", std::round(damage));
		return buffer;
	}

	std::string FormatHealing(float healing)
	{
		char buffer[24]{};
		std::snprintf(buffer, sizeof(buffer), "+%.0f", std::round(healing));
		return buffer;
	}

	void AppendGlyph(std::vector<SpriteInstance>& instances, char character, float x, float y, float width,
	                 float height, const DirectX::XMFLOAT4& color, const DirectX::XMUINT2& texture_size)
	{
		static constexpr int GlyphHeightPixels = 32;
		static constexpr int GlyphWidthPixels = 20;
		static constexpr int AtlasColumns = 16;

		if (character < ' ' || character > '~')
		{
			character = '?';
		}

		const int glyph_index = character - ' ';
		const int texture_x = (glyph_index % AtlasColumns) * GlyphWidthPixels;
		const int texture_y = (glyph_index / AtlasColumns) * GlyphHeightPixels;
		instances.push_back({
		    { x, y },
		    { width, height },
		    0.0f,
		    color,
		    {
		        static_cast<float>(texture_x) / static_cast<float>(texture_size.x),
		        static_cast<float>(texture_y) / static_cast<float>(texture_size.y),
		    },
		    {
		        static_cast<float>(GlyphWidthPixels) / static_cast<float>(texture_size.x),
		        static_cast<float>(GlyphHeightPixels) / static_cast<float>(texture_size.y),
		    },
		});
	}

	void AppendEntry(const DamageTextEntry& entry, std::vector<SpriteInstance>& outline_instances,
	                 std::vector<SpriteInstance>& fill_instances, const DirectX::XMUINT2& texture_size)
	{
		static constexpr float Size = 2.0f;
		static constexpr DirectX::XMFLOAT2 Offsets[] = {
			{ -Size, -Size }, { 0.0f, -Size }, { Size, -Size }, { -Size, 0.0f },
			{ Size, 0.0f },   { -Size, Size }, { 0.0f, Size },  { Size, Size },
		};
		static constexpr float FadeStart = 0.52f;
		static constexpr float GlyphAdvance = 19.0f;
		static constexpr float GlyphHeight = 32.0f;
		static constexpr float GlyphWidth = 20.0f;

		const float life_amount = Saturate(entry.Age / DamageTextTuning::Motion::Lifetime);
		const float pop_amount = SmoothStep(entry.Age / 0.12f);
		const float scale = (0.68f + 0.32f * pop_amount) * (1.0f - life_amount * 0.08f);
		const float fade_amount =
		    SmoothStep((entry.Age - FadeStart) / (DamageTextTuning::Motion::Lifetime - FadeStart));
		const float alpha = 1.0f - fade_amount;
		const float glyph_width = GlyphWidth * scale;
		const float glyph_height = GlyphHeight * scale;
		const float advance = GlyphAdvance * scale;
		const float text_width =
		    entry.Text.empty() ? 0.0f : glyph_width + advance * static_cast<float>(entry.Text.size() - 1);
		const float first_x = entry.Position.x - text_width * 0.5f + glyph_width * 0.5f;

		for (std::size_t i = 0; i < entry.Text.size(); ++i)
		{
			const float x = first_x + static_cast<float>(i) * advance;
			for (const DirectX::XMFLOAT2& offset : Offsets)
			{
				AppendGlyph(outline_instances, entry.Text[i], x + offset.x, entry.Position.y + offset.y, glyph_width,
				            glyph_height, { 0.0f, 0.0f, 0.0f, alpha }, texture_size);
			}
			AppendGlyph(fill_instances, entry.Text[i], x, entry.Position.y, glyph_width, glyph_height,
			            {
			                entry.Color.x,
			                entry.Color.y,
			                entry.Color.z,
			                entry.Color.w * alpha,
			            },
			            texture_size);
		}
	}

	void SpawnEntry(std::string text, const DirectX::XMFLOAT2& world_position, const DirectX::XMFLOAT4& color)
	{
		static constexpr float Spacing = 30.0f;
		static constexpr float Distance = 24.0f;
		static constexpr float Window = 0.05f;
		static constexpr float RiseSpeed = 76.0f;

		if (g_FontTextureID == TEXTURE_INVALID_ID)
		{
			return;
		}

		if (g_Entries.size() >= DamageTextTuning::Pool::Max)
		{
			g_Entries.erase(g_Entries.begin());
		}

		const int drift_step = static_cast<int>(g_SpawnSerial++ % 7u) - 3;
		int stack_level = 0;
		const float stack_distance_sq = Distance * Distance;
		for (const DamageTextEntry& existing : g_Entries)
		{
			if (existing.Age > Window)
			{
				continue;
			}
			const float distance_x = existing.OriginPosition.x - world_position.x;
			const float distance_y = existing.OriginPosition.y - world_position.y;
			if (distance_x * distance_x + distance_y * distance_y <= stack_distance_sq)
			{
				stack_level = std::max(stack_level, existing.StackLevel + 1);
			}
		}

		DamageTextEntry entry{};
		entry.OriginPosition = world_position;
		entry.Color = color;
		entry.Position = {
			world_position.x,
			world_position.y - 54.0f - static_cast<float>(stack_level) * Spacing,
		};
		entry.Velocity = {
			static_cast<float>(drift_step) * 7.0f,
			-RiseSpeed,
		};
		entry.Text = std::move(text);
		entry.StackLevel = stack_level;
		g_Entries.push_back(std::move(entry));
	}
} // namespace

namespace GameDamageText
{
	void Initialize()
	{
		Finalize();
		g_FontTextureID = Texture_Load(L"asset/font/fixedsys/FixedsysExcelsior_ascii_320x512.png", false);
		g_Entries.reserve(DamageTextTuning::Pool::Max);
	}

	void Finalize()
	{
		g_Entries.clear();
		g_SpawnSerial = 0;
		Texture_Release(g_FontTextureID);
		g_FontTextureID = TEXTURE_INVALID_ID;
	}

	void Clear()
	{
		g_Entries.clear();
	}

	void Spawn(float damage, const DirectX::XMFLOAT2& world_position)
	{
		Spawn(damage, world_position, { 1.0f, 1.0f, 1.0f, 1.0f });
	}

	void Spawn(float damage, const DirectX::XMFLOAT2& world_position, const DirectX::XMFLOAT4& color)
	{
		if (damage <= 0.0f)
		{
			return;
		}
		SpawnEntry(FormatDamage(damage), world_position, color);
	}

	void SpawnHealing(float healing, const DirectX::XMFLOAT2& world_position)
	{
		if (healing <= 0.0f)
		{
			return;
		}
		SpawnEntry(FormatHealing(healing), world_position, { 0.20f, 1.0f, 0.32f, 1.0f });
	}

	void Update(float delta_time)
	{
		delta_time = std::max(delta_time, 0.0f);
		for (DamageTextEntry& entry : g_Entries)
		{
			entry.Age += delta_time;
			entry.Position.x += entry.Velocity.x * delta_time;
			entry.Position.y += entry.Velocity.y * delta_time;
			entry.Velocity.x *= std::max(0.0f, 1.0f - delta_time * 3.5f);
		}
		std::erase_if(g_Entries,
		              [](const DamageTextEntry& entry)
		              {
			              return entry.Age >= DamageTextTuning::Motion::Lifetime;
		              });
	}

	void Draw()
	{
		static constexpr std::size_t FillInstanceReserve = DamageTextTuning::Pool::Max * 6;
		static constexpr std::size_t OutlineInstanceReserve = FillInstanceReserve * 8;
		if (g_FontTextureID == TEXTURE_INVALID_ID)
		{
			return;
		}
		const DirectX::XMUINT2 texture_size = Texture_GetSize(g_FontTextureID);
		if (texture_size.x == 0 || texture_size.y == 0)
		{
			return;
		}
		static std::vector<SpriteInstance> outline_instances;
		static std::vector<SpriteInstance> fill_instances;
		if (outline_instances.capacity() < OutlineInstanceReserve)
		{
			outline_instances.reserve(OutlineInstanceReserve);
			fill_instances.reserve(FillInstanceReserve);
		}
		outline_instances.clear();
		fill_instances.clear();
		for (const DamageTextEntry& entry : g_Entries)
		{
			AppendEntry(entry, outline_instances, fill_instances, texture_size);
		}
		if (!outline_instances.empty())
		{
			SpriteInstanced_DrawUnlit(g_FontTextureID, outline_instances.data(),
			                          static_cast<int>(outline_instances.size()));
		}
		if (!fill_instances.empty())
		{
			SpriteInstanced_DrawUnlit(g_FontTextureID, fill_instances.data(), static_cast<int>(fill_instances.size()));
		}
	}
} // namespace GameDamageText
