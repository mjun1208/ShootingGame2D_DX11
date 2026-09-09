#ifndef WEAPON_AUDIO_H
#define WEAPON_AUDIO_H

enum class BulletType;

namespace WeaponAudio
{
	void Initialize();
	void Finalize();
	void PlayFire(BulletType type);
	void PlayFireballExplosion();
	void PlayMagicBladeSummon();
	void UpdateProjectileEvents();
} // namespace WeaponAudio

#endif // !WEAPON_AUDIO_H
