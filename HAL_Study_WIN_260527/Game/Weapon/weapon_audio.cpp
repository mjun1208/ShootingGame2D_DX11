#include "weapon_audio.h"

#include "Audio.h"
#include "game_bullet.h"
#include "projectile.h"

#include <array>

namespace
{
	constexpr std::size_t WEAPON_COUNT = static_cast<std::size_t>(BulletType::Count);
	std::array<int, WEAPON_COUNT> g_FireAudioIDs{};
	int g_FireballExplosionAudioID = -1;
	int g_MagicBladeSummonAudioID = -1;
	int g_MagicBladeLaunchAudioID = -1;

	int& GetFireAudioSlot(BulletType type)
	{
		return g_FireAudioIDs[static_cast<std::size_t>(type)];
	}

	void UnloadIfLoaded(int& audio_id)
	{
		if (audio_id < 0)
		{
			return;
		}
		Audio_Unload(audio_id);
		audio_id = -1;
	}
} // namespace

namespace WeaponAudio
{
	void Initialize()
	{
		g_FireAudioIDs.fill(-1);
		GetFireAudioSlot(BulletType::Fireball) = Audio_Load("asset/sound/mixkit-wizard-fire-woosh-1326.wav");
		GetFireAudioSlot(BulletType::Lightning) = Audio_Load("asset/sound/pixabay-plasma-ku-05-233818.wav");
		GetFireAudioSlot(BulletType::Boomerang) = Audio_Load("asset/sound/boomerang-throw.wav");
		GetFireAudioSlot(BulletType::Shotgun) = Audio_Load("asset/sound/pixabay-saiga-12k-gunshot-94496.wav");
		g_FireballExplosionAudioID = Audio_Load("asset/sound/mixkit-short-explosion-1694.wav");
		g_MagicBladeSummonAudioID = Audio_Load("asset/sound/magic-blade-summon-teleport.wav");
		g_MagicBladeLaunchAudioID = Audio_Load("asset/sound/magic-blade-launch.wav");
	}

	void Finalize()
	{
		for (int& audio_id : g_FireAudioIDs)
		{
			UnloadIfLoaded(audio_id);
		}
		UnloadIfLoaded(g_FireballExplosionAudioID);
		UnloadIfLoaded(g_MagicBladeSummonAudioID);
		UnloadIfLoaded(g_MagicBladeLaunchAudioID);
	}

	void PlayFire(BulletType type)
	{
		const int type_index = static_cast<int>(type);
		const int audio_id = type_index >= 0 && type_index < static_cast<int>(g_FireAudioIDs.size())
		                         ? g_FireAudioIDs[static_cast<std::size_t>(type_index)]
		                         : -1;
		if (audio_id >= 0)
		{
			Audio_Play(audio_id);
		}
	}

	void PlayFireballExplosion()
	{
		if (g_FireballExplosionAudioID >= 0)
		{
			Audio_Play(g_FireballExplosionAudioID);
		}
	}

	void PlayMagicBladeSummon()
	{
		if (g_MagicBladeSummonAudioID >= 0)
		{
			Audio_Play(g_MagicBladeSummonAudioID);
		}
	}

	void UpdateProjectileEvents()
	{
		const int launch_event_count = ProjectileSystem_ConsumeMagicBladeLaunchEvents();
		if (launch_event_count > 0 && g_MagicBladeLaunchAudioID >= 0)
		{
			Audio_Play(g_MagicBladeLaunchAudioID);
		}
	}
} // namespace WeaponAudio
