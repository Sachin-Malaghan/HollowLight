// EMBERHOME: all sound is synthesised live - rain, wind, footsteps, traps, the dog, a gentle generated score.
// No audio files. (CLAUDE.md: Audio)
#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "Containers/Queue.h"
#include "Game/HLGame.h"

#include <atomic>

#include "HLAudioSynth.generated.h"

UCLASS()
class UHLAudioSynth : public USynthComponent, public IHLAudioSink
{
	GENERATED_BODY()

public:
	UHLAudioSynth(const FObjectInitializer& ObjectInitializer);

	// IHLAudioSink (game thread)
	virtual void OnSimEvent(const HL::FEvent& Event, float Pan, float Distance) override;
	virtual void OnUiSound(EHLUiSound Sound) override;
	virtual void OnCue(EHLCue Cue, float Near, float Pan) override;
	virtual void OnThunder(float Strength, float Delay) override;
	virtual void SetMix(float Rain, float Wind, bool bMusic, bool bSound, bool bMenu, float Warmth) override;

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

public:
	struct FVoiceSpec
	{
		float Delay = 0;        // seconds before it starts
		float Dur = 0.2f;       // seconds to -60 dB
		float Attack = 0.003f;
		float Amp = 0.1f;
		float Pan = 0;          // -1..1
		float F0 = 0, F1 = 0;   // tone sweep (Hz), 0 = no tone
		float Tone = 0;         // tone mix
		float Noise = 0;        // band-limited noise mix
		float Cut1 = 0.2f;      // noise low-pass coefficient (higher = brighter)
		float Cut2 = 0.02f;     // noise high-pass coefficient
		float Wet = 0.15f;      // reverb send
		float Harm = 0;         // 2nd harmonic amount (bells, piano)
		float FMid = 0;         // > 0: the pitch passes through this at KMid of the way (a bark's yelp, a whistle's bend)
		float KMid = 0.5f;
		float Hold = 0;         // fraction of Dur held at full level before the decay (breath and voice, not struck things)
		float Vib = 0;          // vibrato depth (fraction of pitch)
		float VibHz = 5.5f;
		float Saw = 0;          // upper harmonics 2..5 (a throat, not a bell)
		bool bMusicBus = false; // follows the music setting instead of the sound setting
	};

private:
	void Queue(const FVoiceSpec& Spec);

	struct FVoice
	{
		FVoiceSpec S;
		double T = 0;
		double Phase = 0;
		float Lp1 = 0, Lp2 = 0;
		bool bActive = false;
	};

	struct FCommand
	{
		FVoiceSpec Spec;
	};

	TQueue<FCommand, EQueueMode::Mpsc> Commands;

	// Mix targets written by the game thread, smoothed on the audio thread.
	std::atomic<float> TargetRain{ 1.f };
	std::atomic<float> TargetWind{ 0.f };
	std::atomic<float> TargetMusic{ 1.f };
	std::atomic<float> TargetSfx{ 1.f };
	std::atomic<float> TargetWarmth{ 0.f };

	// Audio-thread state
	static constexpr int32 MaxVoices = 48;
	FVoice Voices[MaxVoices];
	float Rate = 48000.f;
	uint32 Rng = 0x1234567u;
	float Rain = 0, Wind = 0, Music = 0, Sfx = 0, Warmth = 0;
	float RainLpL = 0, RainLpR = 0, RainHpL = 0, RainHpR = 0, RainLow = 0;
	float WindLp = 0, WindLp2 = 0;
	double WindPhase = 0;
	double DronePhase[3] = { 0, 0, 0 };
	double MusicClock = 0, NextBeat = 1.5;
	int32 Beat = 0, MelodyIndex = 2;
	double DripClock = 0;
	TArray<float> CombBuf[4];
	int32 CombPos[4] = { 0, 0, 0, 0 };
	float CombLp[4] = { 0, 0, 0, 0 };
	TArray<float> ApBuf[2];
	int32 ApPos[2] = { 0, 0 };

	float Noise();
	void StartVoice(const FVoiceSpec& Spec);
	void PlayBeat();
	float Reverb(float In);
};
