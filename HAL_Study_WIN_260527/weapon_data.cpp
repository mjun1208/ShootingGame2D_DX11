#include "weapon_data.h"

#include "json_reader.h"

#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

namespace
{
	constexpr std::size_t INVALID_WEAPON_INDEX = WeaponGameData::WEAPON_COUNT;

	std::size_t IdToIndex(std::string_view id)
	{
		if (id == "fireball") return 0;
		if (id == "lightning") return 1;
		if (id == "ricochet") return 2;
		if (id == "bezier_homing") return 3;
		if (id == "orbit_blade") return 4;
		if (id == "boomerang") return 5;
		if (id == "shotgun") return 6;
		if (id == "magic_blade") return 7;
		return INVALID_WEAPON_INDEX;
	}

	std::wstring ToWide(std::string_view text)
	{
		return { text.begin(), text.end() };
	}

	bool TryParseHitBehavior(
		std::string_view value,
		ProjectileHitBehavior& output)
	{
		if (value == "stop") output = ProjectileHitBehavior::Stop;
		else if (value == "area") output = ProjectileHitBehavior::Area;
		else if (value == "chainLightning") output = ProjectileHitBehavior::ChainLightning;
		else if (value == "pierce") output = ProjectileHitBehavior::Pierce;
		else if (value == "persistentPierce") output = ProjectileHitBehavior::PersistentPierce;
		else return false;
		return true;
	}

	bool TryParseMotionBehavior(
		std::string_view value,
		ProjectileMotionBehavior& output)
	{
		if (value == "linear") output = ProjectileMotionBehavior::Linear;
		else if (value == "boomerang") output = ProjectileMotionBehavior::Boomerang;
		else if (value == "orbitOwner") output = ProjectileMotionBehavior::OrbitOwner;
		else if (value == "magicBlade") output = ProjectileMotionBehavior::MagicBlade;
		else return false;
		return true;
	}

	constexpr std::uint64_t FIELD_ID = 1ull << 0;
	constexpr std::uint64_t FIELD_TEXTURE = 1ull << 1;
	constexpr std::uint64_t FIELD_DRAW_SIZE = 1ull << 2;
	constexpr std::uint64_t FIELD_COLLISION_RADIUS = 1ull << 3;
	constexpr std::uint64_t FIELD_PROJECTILE_SPEED = 1ull << 4;
	constexpr std::uint64_t FIELD_DAMAGE = 1ull << 5;
	constexpr std::uint64_t FIELD_FIRE_INTERVAL = 1ull << 6;
	constexpr std::uint64_t FIELD_PROJECTILE_LIFETIME = 1ull << 7;
	constexpr std::uint64_t FIELD_HIT_BEHAVIOR = 1ull << 8;
	constexpr std::uint64_t FIELD_AREA_RADIUS = 1ull << 9;
	constexpr std::uint64_t FIELD_MAX_TARGET_HITS = 1ull << 10;
	constexpr std::uint64_t FIELD_MAX_BOUNCES = 1ull << 11;
	constexpr std::uint64_t FIELD_ALIVE_LIMIT_GROUP = 1ull << 12;
	constexpr std::uint64_t FIELD_MAX_ALIVE = 1ull << 13;
	constexpr std::uint64_t FIELD_VOLLEY_COUNT = 1ull << 14;
	constexpr std::uint64_t FIELD_VOLLEY_SPAWN_RADIUS = 1ull << 15;
	constexpr std::uint64_t FIELD_USES_BEZIER_HOMING = 1ull << 16;
	constexpr std::uint64_t FIELD_BEZIER_CURVE_STRENGTH = 1ull << 17;
	constexpr std::uint64_t FIELD_TRAIL_EMIT_INTERVAL = 1ull << 18;
	constexpr std::uint64_t FIELD_TRAIL_WIDTH = 1ull << 19;
	constexpr std::uint64_t FIELD_TRAIL_LENGTH = 1ull << 20;
	constexpr std::uint64_t FIELD_TRAIL_OFFSET = 1ull << 21;
	constexpr std::uint64_t FIELD_TRAIL_LIFETIME = 1ull << 22;
	constexpr std::uint64_t FIELD_TRAIL_START_SCALE = 1ull << 23;
	constexpr std::uint64_t FIELD_TRAIL_END_SCALE = 1ull << 24;
	constexpr std::uint64_t FIELD_TRAIL_COLOR = 1ull << 25;
	constexpr std::uint64_t FIELD_VOLLEY_ARC_DEGREES = 1ull << 26;
	constexpr std::uint64_t FIELD_MOTION = 1ull << 27;
	constexpr std::uint64_t FIELD_BOOMERANG_RETURN_TIME = 1ull << 28;
	constexpr std::uint64_t FIELD_ORBIT_RADIUS = 1ull << 29;
	constexpr std::uint64_t FIELD_ORBIT_ANGULAR_SPEED = 1ull << 30;
	constexpr std::uint64_t FIELD_SPIN_SPEED = 1ull << 31;
	constexpr std::uint64_t FIELD_REPEAT_HIT_INTERVAL = 1ull << 32;
	constexpr std::uint64_t FIELD_USES_TRAIL = 1ull << 33;
	constexpr std::uint64_t FIELD_MAGIC_BLADE_SUMMON_TIME = 1ull << 34;
	constexpr std::uint64_t FIELD_MAGIC_BLADE_READY_DELAY = 1ull << 35;
	constexpr std::uint64_t FIELD_MAGIC_BLADE_SIDE_OFFSET = 1ull << 36;
	constexpr std::uint64_t REQUIRED_FIELDS = (1ull << 26) - 1ull;

	class WeaponJsonParser
	{
	public:
		WeaponJsonParser(
			std::string_view source,
			std::array<WeaponData, WeaponGameData::WEAPON_COUNT>& weapons)
			: m_Reader(source), m_Weapons(weapons)
		{
		}

		bool Parse()
		{
			if (!m_Reader.Consume('{') || m_Reader.Consume('}')) return false;

			bool has_weapons = false;
			while (true)
			{
				std::string key;
				if (!m_Reader.ParseString(key) || !m_Reader.Consume(':')) return false;
				if (key == "weapons")
				{
					if (has_weapons || !ParseWeaponArray()) return false;
					has_weapons = true;
				}
				else if (!m_Reader.SkipValue())
				{
					return false;
				}

				if (m_Reader.Consume('}')) break;
				if (!m_Reader.Consume(',')) return false;
			}

			if (!has_weapons || !m_Reader.IsAtEnd()) return false;
			for (bool was_parsed : m_ParsedWeapons)
			{
				if (!was_parsed) return false;
			}
			return true;
		}

	private:
		bool ParseFloat2(float& first, float& second)
		{
			return m_Reader.Consume('[') &&
				m_Reader.ParseFloat(first) && m_Reader.Consume(',') &&
				m_Reader.ParseFloat(second) && m_Reader.Consume(']');
		}

		bool ParseFloat4(DirectX::XMFLOAT4& output)
		{
			return m_Reader.Consume('[') &&
				m_Reader.ParseFloat(output.x) && m_Reader.Consume(',') &&
				m_Reader.ParseFloat(output.y) && m_Reader.Consume(',') &&
				m_Reader.ParseFloat(output.z) && m_Reader.Consume(',') &&
				m_Reader.ParseFloat(output.w) && m_Reader.Consume(']');
		}

		bool ParseWeaponArray()
		{
			if (!m_Reader.Consume('[') || m_Reader.Consume(']')) return false;
			while (true)
			{
				if (!ParseWeapon()) return false;
				if (m_Reader.Consume(']')) return true;
				if (!m_Reader.Consume(',')) return false;
			}
		}

		bool ParseWeapon()
		{
			if (!m_Reader.Consume('{') || m_Reader.Consume('}')) return false;

			WeaponData parsed{};
			std::uint64_t parsed_fields = 0;
			while (true)
			{
				std::string key;
				if (!m_Reader.ParseString(key) || !m_Reader.Consume(':')) return false;
				if (!ParseField(key, parsed, parsed_fields)) return false;

				if (m_Reader.Consume('}')) break;
				if (!m_Reader.Consume(',')) return false;
			}

			if ((parsed_fields & REQUIRED_FIELDS) != REQUIRED_FIELDS ||
				!Validate(parsed)) return false;
			const std::size_t index = IdToIndex(parsed.Id);
			if (index == INVALID_WEAPON_INDEX || m_ParsedWeapons[index]) return false;

			m_ParsedWeapons[index] = true;
			m_Weapons[index] = std::move(parsed);
			return true;
		}

		bool ParseField(
			std::string_view key,
			WeaponData& data,
			std::uint64_t& parsed_fields)
		{
			if (key == "id")
			{
				return MarkField(parsed_fields, FIELD_ID) &&
					m_Reader.ParseString(data.Id);
			}
			if (key == "texture")
			{
				if (!MarkField(parsed_fields, FIELD_TEXTURE)) return false;
				std::string texture_path;
				if (!m_Reader.ParseString(texture_path)) return false;
				data.TexturePath = ToWide(texture_path);
				return true;
			}
			if (key == "drawSize")
			{
				return MarkField(parsed_fields, FIELD_DRAW_SIZE) &&
					ParseFloat2(data.Width, data.Height);
			}
			if (key == "collisionRadius")
				return ParseFloatField(parsed_fields, FIELD_COLLISION_RADIUS, data.CollisionRadius);
			if (key == "projectileSpeed")
				return ParseFloatField(parsed_fields, FIELD_PROJECTILE_SPEED, data.ProjectileSpeed);
			if (key == "damage")
				return ParseFloatField(parsed_fields, FIELD_DAMAGE, data.Damage);
			if (key == "fireInterval")
				return ParseFloatField(parsed_fields, FIELD_FIRE_INTERVAL, data.FireInterval);
			if (key == "projectileLifetime")
				return ParseFloatField(parsed_fields, FIELD_PROJECTILE_LIFETIME, data.ProjectileLifeTime);
			if (key == "hitBehavior")
			{
				if (!MarkField(parsed_fields, FIELD_HIT_BEHAVIOR)) return false;
				std::string hit_behavior;
				return m_Reader.ParseString(hit_behavior) &&
					TryParseHitBehavior(hit_behavior, data.HitBehavior);
			}
			if (key == "areaRadius")
				return ParseFloatField(parsed_fields, FIELD_AREA_RADIUS, data.AreaRadius);
			if (key == "maxTargetHits")
				return ParseIntField(parsed_fields, FIELD_MAX_TARGET_HITS, data.MaxTargetHits);
			if (key == "maxBounces")
				return ParseIntField(parsed_fields, FIELD_MAX_BOUNCES, data.MaxBounces);
			if (key == "aliveLimitGroup")
				return ParseIntField(parsed_fields, FIELD_ALIVE_LIMIT_GROUP, data.AliveLimitGroup);
			if (key == "maxAlive")
				return ParseIntField(parsed_fields, FIELD_MAX_ALIVE, data.MaxAlive);
			if (key == "volleyCount")
				return ParseIntField(parsed_fields, FIELD_VOLLEY_COUNT, data.VolleyCount);
			if (key == "volleySpawnRadius")
				return ParseFloatField(parsed_fields, FIELD_VOLLEY_SPAWN_RADIUS, data.VolleySpawnRadius);
			if (key == "volleyArcDegrees")
				return ParseFloatField(parsed_fields, FIELD_VOLLEY_ARC_DEGREES, data.VolleyArcDegrees);
			if (key == "usesBezierHoming")
			{
				return MarkField(parsed_fields, FIELD_USES_BEZIER_HOMING) &&
					m_Reader.ParseBool(data.UsesBezierHoming);
			}
			if (key == "bezierCurveStrength")
				return ParseFloatField(parsed_fields, FIELD_BEZIER_CURVE_STRENGTH, data.BezierCurveStrength);
			if (key == "motion")
			{
				if (!MarkField(parsed_fields, FIELD_MOTION)) return false;
				std::string motion;
				return m_Reader.ParseString(motion) &&
					TryParseMotionBehavior(motion, data.MotionBehavior);
			}
			if (key == "boomerangReturnTime")
				return ParseFloatField(parsed_fields, FIELD_BOOMERANG_RETURN_TIME, data.BoomerangReturnTime);
			if (key == "orbitRadius")
				return ParseFloatField(parsed_fields, FIELD_ORBIT_RADIUS, data.OrbitRadius);
			if (key == "orbitAngularSpeed")
				return ParseFloatField(parsed_fields, FIELD_ORBIT_ANGULAR_SPEED, data.OrbitAngularSpeed);
			if (key == "spinSpeed")
				return ParseFloatField(parsed_fields, FIELD_SPIN_SPEED, data.SpinSpeed);
			if (key == "repeatHitInterval")
				return ParseFloatField(parsed_fields, FIELD_REPEAT_HIT_INTERVAL, data.RepeatHitInterval);
			if (key == "magicBladeSummonTime")
				return ParseFloatField(parsed_fields, FIELD_MAGIC_BLADE_SUMMON_TIME, data.MagicBladeSummonTime);
			if (key == "magicBladeReadyDelay")
				return ParseFloatField(parsed_fields, FIELD_MAGIC_BLADE_READY_DELAY, data.MagicBladeReadyDelay);
			if (key == "magicBladeSideOffset")
				return ParseFloatField(parsed_fields, FIELD_MAGIC_BLADE_SIDE_OFFSET, data.MagicBladeSideOffset);
			if (key == "usesTrail")
			{
				return MarkField(parsed_fields, FIELD_USES_TRAIL) &&
					m_Reader.ParseBool(data.UsesTrail);
			}
			if (key == "trailEmitInterval")
				return ParseFloatField(parsed_fields, FIELD_TRAIL_EMIT_INTERVAL, data.TrailEmitInterval);
			if (key == "trailWidth")
				return ParseFloatField(parsed_fields, FIELD_TRAIL_WIDTH, data.TrailWidth);
			if (key == "trailLength")
				return ParseFloatField(parsed_fields, FIELD_TRAIL_LENGTH, data.TrailLength);
			if (key == "trailOffset")
				return ParseFloatField(parsed_fields, FIELD_TRAIL_OFFSET, data.TrailOffset);
			if (key == "trailLifetime")
				return ParseFloatField(parsed_fields, FIELD_TRAIL_LIFETIME, data.TrailLifeTime);
			if (key == "trailStartScale")
				return ParseFloatField(parsed_fields, FIELD_TRAIL_START_SCALE, data.TrailStartScale);
			if (key == "trailEndScale")
				return ParseFloatField(parsed_fields, FIELD_TRAIL_END_SCALE, data.TrailEndScale);
			if (key == "trailColor")
			{
				return MarkField(parsed_fields, FIELD_TRAIL_COLOR) && ParseFloat4(data.TrailColor);
			}
			return m_Reader.SkipValue();
		}

		bool ParseFloatField(
			std::uint64_t& parsed_fields,
			std::uint64_t field,
			float& output)
		{
			return MarkField(parsed_fields, field) && m_Reader.ParseFloat(output);
		}

		bool ParseIntField(
			std::uint64_t& parsed_fields,
			std::uint64_t field,
			int& output)
		{
			return MarkField(parsed_fields, field) && m_Reader.ParseInt(output);
		}

		bool MarkField(std::uint64_t& parsed_fields, std::uint64_t field) const
		{
			if ((parsed_fields & field) != 0) return false;
			parsed_fields |= field;
			return true;
		}

		bool Validate(const WeaponData& data) const
		{
			return !data.Id.empty() &&
				!data.TexturePath.empty() &&
				data.Width > 0.0f &&
				data.Height > 0.0f &&
				data.CollisionRadius > 0.0f &&
				data.ProjectileSpeed >= 0.0f &&
				data.Damage >= 0.0f &&
				data.FireInterval >= 0.0f &&
				data.ProjectileLifeTime >= 0.0f &&
				data.AreaRadius >= 0.0f &&
				data.MaxTargetHits >= 1 &&
				data.MaxTargetHits <= PROJECTILE_HIT_HISTORY_MAX &&
				data.MaxBounces >= 0 &&
				data.AliveLimitGroup >= PROJECTILE_INVALID_ID &&
				data.MaxAlive >= 0 &&
				data.VolleyCount >= 1 &&
				data.VolleySpawnRadius >= 0.0f &&
				data.VolleyArcDegrees > 0.0f &&
				data.VolleyArcDegrees <= 360.0f &&
				data.BezierCurveStrength >= 0.0f &&
				data.BoomerangReturnTime >= 0.0f &&
				data.OrbitRadius >= 0.0f &&
				data.OrbitAngularSpeed >= 0.0f &&
				data.RepeatHitInterval >= 0.0f &&
				data.MagicBladeSummonTime >= 0.0f &&
				data.MagicBladeReadyDelay >= 0.0f &&
				data.MagicBladeSideOffset >= 0.0f &&
				(data.MotionBehavior != ProjectileMotionBehavior::Boomerang ||
					data.BoomerangReturnTime > 0.0f) &&
				(data.MotionBehavior != ProjectileMotionBehavior::OrbitOwner ||
					(data.OrbitRadius > 0.0f && data.OrbitAngularSpeed > 0.0f)) &&
				(data.MotionBehavior != ProjectileMotionBehavior::MagicBlade ||
					(data.MagicBladeSummonTime > 0.0f &&
						data.MagicBladeSideOffset > 0.0f)) &&
				data.TrailEmitInterval > 0.0f &&
				data.TrailWidth >= 0.0f &&
				data.TrailLength >= 0.0f &&
				data.TrailLifeTime > 0.0f &&
				data.TrailStartScale >= 0.0f &&
				data.TrailEndScale >= 0.0f &&
				IsColorComponent(data.TrailColor.x) &&
				IsColorComponent(data.TrailColor.y) &&
				IsColorComponent(data.TrailColor.z) &&
				IsColorComponent(data.TrailColor.w);
		}

		bool IsColorComponent(float value) const
		{
			return value >= 0.0f && value <= 1.0f;
		}

		Json::Reader m_Reader;
		std::array<WeaponData, WeaponGameData::WEAPON_COUNT>& m_Weapons;
		std::array<bool, WeaponGameData::WEAPON_COUNT> m_ParsedWeapons{};
	};
}

bool WeaponGameData::Load(const char* file_path)
{
	std::string source;
	if (!Json::LoadTextFile(file_path, source)) return false;

	std::array<WeaponData, WEAPON_COUNT> parsed_weapons{};
	WeaponJsonParser parser(source, parsed_weapons);
	if (!parser.Parse()) return false;

	m_Weapons = std::move(parsed_weapons);
	return true;
}

const WeaponData& WeaponGameData::Get(std::size_t weapon_index) const
{
	return m_Weapons[weapon_index < WEAPON_COUNT ? weapon_index : 0u];
}
