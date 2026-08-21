#include "game_damage_text.h"

#include "sprite_instanced.h"
#include "texture.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
	constexpr int DAMAGE_TEXT_MAX = 64;
	constexpr float DAMAGE_TEXT_LIFETIME = 0.85f;
	constexpr float DAMAGE_TEXT_FADE_START = 0.52f;
	constexpr float DAMAGE_TEXT_RISE_SPEED = 76.0f;
	constexpr float DAMAGE_TEXT_GLYPH_WIDTH = 20.0f;
	constexpr float DAMAGE_TEXT_GLYPH_HEIGHT = 32.0f;
	constexpr float DAMAGE_TEXT_GLYPH_ADVANCE = 19.0f;
	constexpr int FONT_ATLAS_COLUMNS = 16;
	constexpr int FONT_GLYPH_WIDTH_PIXELS = 20;
	constexpr int FONT_GLYPH_HEIGHT_PIXELS = 32;

	struct DamageTextEntry
	{
		DirectX::XMFLOAT2 Position{};
		DirectX::XMFLOAT2 Velocity{};
		std::string Text;
		float Age{ 0.0f };
	};

	int g_FontTextureID = TEXTURE_INVALID_ID;
	std::vector<DamageTextEntry> g_Entries;
	unsigned int g_SpawnSerial = 0;

	float SmoothStep(float amount)
	{
		amount = std::clamp(amount, 0.0f, 1.0f);
		return amount * amount * (3.0f - 2.0f * amount);
	}

	std::string FormatDamage(float damage)
	{
		char buffer[24]{};
		const float rounded = std::round(damage);
		if (std::fabs(damage - rounded) < 0.01f)
		{
			std::snprintf(buffer, sizeof(buffer), "%.0f", rounded);
		}
		else
		{
			std::snprintf(buffer, sizeof(buffer), "%.1f", damage);
		}
		return buffer;
	}

	void AppendGlyph(
		std::vector<SpriteInstance>& instances,
		char character,
		float x,
		float y,
		float width,
		float height,
		const DirectX::XMFLOAT4& color,
		const DirectX::XMUINT2& texture_size)
	{
		if (character < ' ' || character > '~')
		{
			character = '?';
		}

		const int glyph_index = character - ' ';
		const int texture_x =
			(glyph_index % FONT_ATLAS_COLUMNS) * FONT_GLYPH_WIDTH_PIXELS;
		const int texture_y =
			(glyph_index / FONT_ATLAS_COLUMNS) * FONT_GLYPH_HEIGHT_PIXELS;
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
				static_cast<float>(FONT_GLYPH_WIDTH_PIXELS) /
					static_cast<float>(texture_size.x),
				static_cast<float>(FONT_GLYPH_HEIGHT_PIXELS) /
					static_cast<float>(texture_size.y),
			},
		});
	}

	void AppendEntry(
		const DamageTextEntry& entry,
		std::vector<SpriteInstance>& outline_instances,
		std::vector<SpriteInstance>& fill_instances,
		const DirectX::XMUINT2& texture_size)
	{
		constexpr float OUTLINE_SIZE = 2.0f;
		constexpr DirectX::XMFLOAT2 OUTLINE_OFFSETS[] = {
			{ -OUTLINE_SIZE, -OUTLINE_SIZE },
			{ 0.0f, -OUTLINE_SIZE },
			{ OUTLINE_SIZE, -OUTLINE_SIZE },
			{ -OUTLINE_SIZE, 0.0f },
			{ OUTLINE_SIZE, 0.0f },
			{ -OUTLINE_SIZE, OUTLINE_SIZE },
			{ 0.0f, OUTLINE_SIZE },
			{ OUTLINE_SIZE, OUTLINE_SIZE },
		};

		const float life_amount = std::clamp(entry.Age / DAMAGE_TEXT_LIFETIME, 0.0f, 1.0f);
		const float pop_amount = SmoothStep(entry.Age / 0.12f);
		const float scale = (0.68f + 0.32f * pop_amount) * (1.0f - life_amount * 0.08f);
		const float fade_amount = SmoothStep(
			(entry.Age - DAMAGE_TEXT_FADE_START) /
			(DAMAGE_TEXT_LIFETIME - DAMAGE_TEXT_FADE_START));
		const float alpha = 1.0f - fade_amount;
		const float glyph_width = DAMAGE_TEXT_GLYPH_WIDTH * scale;
		const float glyph_height = DAMAGE_TEXT_GLYPH_HEIGHT * scale;
		const float advance = DAMAGE_TEXT_GLYPH_ADVANCE * scale;
		const float text_width = entry.Text.empty() ? 0.0f :
			glyph_width + advance * static_cast<float>(entry.Text.size() - 1);
		const float first_x =
			entry.Position.x - text_width * 0.5f + glyph_width * 0.5f;

		for (std::size_t i = 0; i < entry.Text.size(); ++i)
		{
			const float x = first_x + static_cast<float>(i) * advance;
			for (const DirectX::XMFLOAT2& offset : OUTLINE_OFFSETS)
			{
				AppendGlyph(
					outline_instances,
					entry.Text[i],
					x + offset.x,
					entry.Position.y + offset.y,
					glyph_width,
					glyph_height,
					{ 0.0f, 0.0f, 0.0f, alpha },
					texture_size);
			}
			AppendGlyph(
				fill_instances,
				entry.Text[i],
				x,
				entry.Position.y,
				glyph_width,
				glyph_height,
				{ 1.0f, 1.0f, 1.0f, alpha },
				texture_size);
		}
	}
}

namespace GameDamageText
{
void Initialize()
{
	Finalize();
	g_FontTextureID = Texture_Load(
		L"asset/font/fixedsys/FixedsysExcelsior_ascii_320x512.png", false);
	g_Entries.reserve(DAMAGE_TEXT_MAX);
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
	if (damage <= 0.0f || g_FontTextureID == TEXTURE_INVALID_ID)
	{
		return;
	}

	if (g_Entries.size() >= DAMAGE_TEXT_MAX)
	{
		g_Entries.erase(g_Entries.begin());
	}

	const int drift_step = static_cast<int>(g_SpawnSerial++ % 7u) - 3;
	DamageTextEntry entry{};
	entry.Position = { world_position.x, world_position.y - 54.0f };
	entry.Velocity = {
		static_cast<float>(drift_step) * 7.0f,
		-DAMAGE_TEXT_RISE_SPEED,
	};
	entry.Text = FormatDamage(damage);
	g_Entries.push_back(std::move(entry));
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

	g_Entries.erase(
		std::remove_if(
			g_Entries.begin(),
			g_Entries.end(),
			[](const DamageTextEntry& entry)
			{
				return entry.Age >= DAMAGE_TEXT_LIFETIME;
			}),
		g_Entries.end());
}

void Draw()
{
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
	constexpr std::size_t FILL_INSTANCE_RESERVE = DAMAGE_TEXT_MAX * 6;
	constexpr std::size_t OUTLINE_INSTANCE_RESERVE =
		FILL_INSTANCE_RESERVE * 8;
	if (outline_instances.capacity() < OUTLINE_INSTANCE_RESERVE)
	{
		outline_instances.reserve(OUTLINE_INSTANCE_RESERVE);
		fill_instances.reserve(FILL_INSTANCE_RESERVE);
	}
	outline_instances.clear();
	fill_instances.clear();

	for (const DamageTextEntry& entry : g_Entries)
	{
		AppendEntry(
			entry,
			outline_instances,
			fill_instances,
			texture_size);
	}

	if (!outline_instances.empty())
	{
		SpriteInstanced_Draw(
			g_FontTextureID,
			outline_instances.data(),
			static_cast<int>(outline_instances.size()));
	}
	if (!fill_instances.empty())
	{
		SpriteInstanced_Draw(
			g_FontTextureID,
			fill_instances.data(),
			static_cast<int>(fill_instances.size()));
	}
}
}
