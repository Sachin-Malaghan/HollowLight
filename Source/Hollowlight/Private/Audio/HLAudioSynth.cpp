// EMBERHOME: live sound synthesis. (CLAUDE.md: Audio)
#include "Audio/HLAudioSynth.h"

using namespace HL;

namespace
{
	// Pentatonic scales for the sparse score: minor in the rain, major as the dawn comes.
	const float MinorScale[] = { 220.00f, 261.63f, 293.66f, 329.63f, 392.00f, 440.00f, 523.25f, 587.33f, 659.25f };
	const float MajorScale[] = { 220.00f, 246.94f, 277.18f, 329.63f, 369.99f, 440.00f, 493.88f, 554.37f, 659.25f };

	const int32 CombDelay48k[4] = { 1687, 1601, 2053, 2251 };
	const int32 ApDelay48k[2] = { 556, 441 };
}

UHLAudioSynth::UHLAudioSynth(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 2;
	bAutoActivate = true;
	bIsUISound = true;          // keeps playing regardless of world pause
	bAllowSpatialization = false;
}

bool UHLAudioSynth::Init(int32& SampleRate)
{
	NumChannels = 2;
	Rate = (float)SampleRate;
	for (int32 I = 0; I < 4; ++I)
	{
		CombBuf[I].SetNumZeroed(FMath::Max(64, (int32)(CombDelay48k[I] * Rate / 48000.f)));
		CombPos[I] = 0;
	}
	for (int32 I = 0; I < 2; ++I)
	{
		ApBuf[I].SetNumZeroed(FMath::Max(32, (int32)(ApDelay48k[I] * Rate / 48000.f)));
		ApPos[I] = 0;
	}
	return true;
}

// -------------------------------------------------------------------------------------------------
// Game thread

void UHLAudioSynth::Queue(const FVoiceSpec& Spec)
{
	Commands.Enqueue(FCommand{ Spec });
}

void UHLAudioSynth::SetMix(float InRain, float InWind, bool bMusic, bool bSound, bool bMenu, float InWarmth)
{
	TargetRain = bSound ? FMath::Clamp(InRain, 0.f, 2.f) : 0.f;
	TargetWind = bSound ? FMath::Clamp(InWind, 0.f, 2.f) : 0.f;
	TargetMusic = bMusic ? 1.f : 0.f;
	TargetSfx = bSound ? (bMenu ? 0.45f : 1.f) : 0.f;
	TargetWarmth = FMath::Clamp(InWarmth, 0.f, 1.f);
}

void UHLAudioSynth::OnUiSound(EHLUiSound Sound)
{
	FVoiceSpec V;
	V.Tone = 1;
	V.Wet = 0.2f;
	switch (Sound)
	{
	case EHLUiSound::Move: V.F0 = V.F1 = 1480; V.Dur = 0.05f; V.Amp = 0.018f; break;
	case EHLUiSound::Select: V.F0 = 880; V.F1 = 1320; V.Dur = 0.22f; V.Amp = 0.04f; V.Harm = 0.3f; break;
	case EHLUiSound::Back: V.F0 = 740; V.F1 = 520; V.Dur = 0.16f; V.Amp = 0.03f; break;
	}
	Queue(V);
}

void UHLAudioSynth::OnThunder(float Strength, float Delay)
{
	FVoiceSpec V;
	V.Delay = Delay;
	V.Noise = 1;
	V.Cut1 = 0.012f;
	V.Cut2 = 0.0015f;
	V.Attack = 0.25f;
	V.Dur = 3.6f;
	V.Amp = 1.3f * Strength;
	V.Wet = 0.3f;
	Queue(V);
	FVoiceSpec Crack = V;
	Crack.Delay = Delay;
	Crack.Cut1 = 0.08f;
	Crack.Cut2 = 0.01f;
	Crack.Attack = 0.01f;
	Crack.Dur = 0.6f;
	Crack.Amp = 0.35f * Strength;
	Queue(Crack);
}

void UHLAudioSynth::OnSimEvent(const HL::FEvent& E, float Pan, float Distance)
{
	const float Near = FMath::Clamp(1.f - Distance / 900.f, 0.12f, 1.f);
	const float S = (float)E.Strength;
	FVoiceSpec V;
	V.Pan = Pan * 0.6f;
	switch (E.Type)
	{
	case EEvent::Footstep:
		V.Noise = 1; V.Cut1 = 0.16f; V.Cut2 = 0.018f; V.Dur = 0.075f; V.Amp = 0.11f * S; V.Wet = 0.05f;
		Queue(V);
		V.Cut1 = 0.55f; V.Cut2 = 0.25f; V.Dur = 0.03f; V.Amp = 0.03f * S;   // wet grass tick
		Queue(V);
		break;
	case EEvent::Jump:
		V.Noise = 0.8f; V.Tone = 0.15f; V.F0 = 300; V.F1 = 520; V.Cut1 = 0.10f; V.Cut2 = 0.03f; V.Attack = 0.03f; V.Dur = 0.18f; V.Amp = 0.07f;
		Queue(V);
		break;
	case EEvent::Land:
		V.Tone = 1; V.F0 = 120; V.F1 = 45; V.Dur = 0.16f; V.Amp = 0.12f + 0.12f * S;
		Queue(V);
		V.Tone = 0; V.Noise = 1; V.Cut1 = 0.12f; V.Cut2 = 0.02f; V.Dur = 0.1f; V.Amp = 0.08f + 0.05f * S;
		Queue(V);
		break;
	case EEvent::PushStart:
		V.Noise = 1; V.Cut1 = 0.05f; V.Cut2 = 0.008f; V.Attack = 0.06f; V.Dur = 0.5f; V.Amp = 0.09f;
		Queue(V);
		break;
	case EEvent::TrapSnap:
	case EEvent::TrapSnapCrate:
		V.Amp = Near;
		{
			FVoiceSpec Click = V; Click.Noise = 1; Click.Cut1 = 0.9f; Click.Cut2 = 0.25f; Click.Dur = 0.035f; Click.Amp = 0.4f * Near; Queue(Click);
			FVoiceSpec Ring = V; Ring.Tone = 1; Ring.F0 = 2240; Ring.F1 = 2190; Ring.Harm = 0.4f; Ring.Dur = 0.32f; Ring.Amp = 0.05f * Near; Ring.Wet = 0.3f; Queue(Ring);
			FVoiceSpec Thud = V; Thud.Tone = 1; Thud.F0 = 95; Thud.F1 = 50; Thud.Dur = 0.14f; Thud.Amp = 0.25f * Near; Queue(Thud);
		}
		break;
	case EEvent::Death:
		{
			FVoiceSpec Out = V; Out.Tone = 1; Out.F0 = 520; Out.F1 = 140; Out.Attack = 0.02f; Out.Dur = 1.1f; Out.Amp = 0.06f; Out.Wet = 0.5f; Out.bMusicBus = false; Queue(Out);
			FVoiceSpec Hiss = V; Hiss.Noise = 1; Hiss.Cut1 = 0.45f; Hiss.Cut2 = 0.12f; Hiss.Dur = 0.5f; Hiss.Amp = 0.05f; Queue(Hiss);
			FVoiceSpec Boom = V; Boom.Tone = 1; Boom.F0 = 62; Boom.F1 = 34; Boom.Dur = 1.4f; Boom.Amp = 0.22f; Boom.Wet = 0.4f; Queue(Boom);
		}
		break;
	case EEvent::Respawn:
		for (int32 I = 0; I < 3; ++I)
		{
			FVoiceSpec N = V; N.Delay = 0.12f * I; N.Tone = 1; N.F0 = N.F1 = MinorScale[3 + I * 2]; N.Harm = 0.25f; N.Dur = 1.2f; N.Amp = 0.035f; N.Wet = 0.55f; Queue(N);
		}
		break;
	case EEvent::Checkpoint:
		{
			FVoiceSpec Bell = V; Bell.Tone = 1; Bell.F0 = Bell.F1 = 880; Bell.Harm = 0.5f; Bell.Dur = 2.2f; Bell.Amp = 0.05f; Bell.Wet = 0.6f; Queue(Bell);
			FVoiceSpec Hi = Bell; Hi.Delay = 0.09f; Hi.F0 = Hi.F1 = 1318.5f; Hi.Dur = 1.6f; Hi.Amp = 0.025f; Queue(Hi);
		}
		break;
	case EEvent::Goal:
		{
			const float Chord[] = { 220.f, 277.18f, 329.63f, 440.f, 554.37f, 659.25f };
			for (int32 I = 0; I < 6; ++I)
			{
				FVoiceSpec N; N.Delay = 0.16f * I; N.Tone = 1; N.F0 = N.F1 = Chord[I]; N.Harm = 0.2f; N.Attack = 0.9f; N.Dur = 6.0f; N.Amp = 0.045f; N.Wet = 0.7f; N.Pan = (I - 2.5f) * 0.12f; N.bMusicBus = true;
				Queue(N);
			}
		}
		break;
	case EEvent::CrateLand:
		V.Tone = 1; V.F0 = 85; V.F1 = 52; V.Dur = 0.2f; V.Amp = (0.12f + 0.15f * S) * Near; Queue(V);
		V.Tone = 0; V.Noise = 1; V.Cut1 = 0.09f; V.Cut2 = 0.015f; V.Dur = 0.12f; V.Amp = 0.1f * Near; Queue(V);
		break;
	case EEvent::CrateSplash:
	case EEvent::Splash:
		V.Noise = 1; V.Cut1 = 0.35f; V.Cut2 = 0.03f; V.Attack = 0.004f; V.Dur = 0.55f; V.Amp = 0.2f * Near; V.Wet = 0.35f; Queue(V);
		V.Noise = 0; V.Tone = 1; V.F0 = 320; V.F1 = 120; V.Dur = 0.25f; V.Amp = 0.04f * Near; Queue(V);
		break;
	case EEvent::CrateReset:
		V.Noise = 1; V.Cut1 = 0.1f; V.Cut2 = 0.05f; V.Attack = 0.2f; V.Dur = 0.6f; V.Amp = 0.05f * Near; Queue(V);
		break;
	case EEvent::CrumbleCreak:
		V.Tone = 0.6f; V.F0 = 150; V.F1 = 105; V.Harm = 0.8f; V.Noise = 0.4f; V.Cut1 = 0.05f; V.Cut2 = 0.01f; V.Dur = 0.45f; V.Amp = 0.07f; Queue(V);
		break;
	case EEvent::CrumbleFall:
		V.Noise = 1; V.Cut1 = 0.7f; V.Cut2 = 0.12f; V.Dur = 0.09f; V.Amp = 0.25f * Near; Queue(V);
		V.Cut1 = 0.08f; V.Cut2 = 0.01f; V.Delay = 0.05f; V.Dur = 0.35f; V.Amp = 0.12f * Near; Queue(V);
		break;
	case EEvent::LogSwoosh:
		V.Noise = 1; V.Cut1 = 0.07f; V.Cut2 = 0.025f; V.Attack = 0.16f; V.Dur = 0.5f; V.Amp = 0.13f * Near * Near; V.Wet = 0.2f; Queue(V);
		break;
	default:
		break;
	}
}

// -------------------------------------------------------------------------------------------------
// Audio thread

float UHLAudioSynth::Noise()
{
	Rng ^= Rng << 13;
	Rng ^= Rng >> 17;
	Rng ^= Rng << 5;
	return (Rng & 0xffffff) / float(0x800000) - 1.f;
}

void UHLAudioSynth::StartVoice(const FVoiceSpec& Spec)
{
	int32 Slot = -1;
	double Oldest = -1;
	for (int32 I = 0; I < MaxVoices; ++I)
	{
		if (!Voices[I].bActive) { Slot = I; break; }
		if (Voices[I].T > Oldest) { Oldest = Voices[I].T; Slot = I; }
	}
	FVoice& V = Voices[Slot];
	V = FVoice();
	V.S = Spec;
	V.T = -Spec.Delay;
	V.bActive = true;
}

void UHLAudioSynth::PlayNote()
{
	const float* Scale = Warmth > 0.5f ? MajorScale : MinorScale;
	const int32 Index = (int32)(((Rng >> 8) % 9));
	FVoiceSpec N;
	N.Tone = 1;
	N.F0 = N.F1 = Scale[Index] * ((Rng & 0x100) ? 1.f : 2.f);
	N.Harm = 0.35f;
	N.Attack = 0.006f;
	N.Dur = 3.2f;
	N.Amp = 0.032f;
	N.Pan = Noise() * 0.4f;
	N.Wet = 0.75f;
	N.bMusicBus = true;
	StartVoice(N);
	if (((Rng >> 3) & 3) == 0)
	{
		// sometimes a soft answering note
		N.Delay = 0.45f;
		N.F0 = N.F1 = Scale[FMath::Max(0, Index - 2)];
		N.Amp = 0.022f;
		StartVoice(N);
	}
}

float UHLAudioSynth::Reverb(float In)
{
	float Sum = 0;
	for (int32 I = 0; I < 4; ++I)
	{
		TArray<float>& B = CombBuf[I];
		const float Out = B[CombPos[I]];
		CombLp[I] += 0.35f * (Out - CombLp[I]);
		B[CombPos[I]] = In + CombLp[I] * 0.84f;
		CombPos[I] = (CombPos[I] + 1) % B.Num();
		Sum += Out;
	}
	Sum *= 0.25f;
	for (int32 I = 0; I < 2; ++I)
	{
		TArray<float>& B = ApBuf[I];
		const float Buf = B[ApPos[I]];
		const float Out = -Sum + Buf;
		B[ApPos[I]] = Sum + Buf * 0.5f;
		ApPos[I] = (ApPos[I] + 1) % B.Num();
		Sum = Out;
	}
	return Sum;
}

int32 UHLAudioSynth::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	FCommand Cmd;
	while (Commands.Dequeue(Cmd)) { StartVoice(Cmd.Spec); }

	const int32 Frames = NumSamples / 2;
	const float Dt = 1.f / Rate;
	const float Smooth = 1.f - FMath::Exp(-Dt * 3.f);
	const float TRain = TargetRain.load(), TWind = TargetWind.load(), TMusic = TargetMusic.load(), TSfx = TargetSfx.load(), TWarm = TargetWarmth.load();

	for (int32 F = 0; F < Frames; ++F)
	{
		Rain += (TRain - Rain) * Smooth;
		Wind += (TWind - Wind) * Smooth;
		Music += (TMusic - Music) * Smooth;
		Sfx += (TSfx - Sfx) * Smooth;
		Warmth += (TWarm - Warmth) * Smooth;

		float L = 0, R = 0, Wet = 0;

		// Rain bed: two decorrelated noise streams, a soft low roar plus hiss.
		if (Rain > 0.001f)
		{
			const float NL = Noise(), NR = Noise();
			RainLpL += 0.22f * (NL - RainLpL);
			RainLpR += 0.22f * (NR - RainLpR);
			RainHpL += 0.02f * (RainLpL - RainHpL);
			RainHpR += 0.02f * (RainLpR - RainHpR);
			RainLow += 0.004f * ((NL + NR) * 0.5f - RainLow);
			const float G = 0.075f * FMath::Min(Rain, 1.6f) * (1.f - 0.5f * Warmth);
			L += ((RainLpL - RainHpL) + RainLow * 2.5f) * G;
			R += ((RainLpR - RainHpR) + RainLow * 2.5f) * G;

			// Drips on leaves.
			DripClock -= Dt;
			if (DripClock <= 0)
			{
				DripClock = (0.02 + ((Rng >> 9) & 1023) / 1023.0 * 0.12) / FMath::Max(0.2f, Rain);
				FVoiceSpec D;
				D.Noise = 1; D.Cut1 = 0.55f; D.Cut2 = 0.22f; D.Dur = 0.014f; D.Amp = 0.035f + 0.03f * FMath::Abs(Noise()); D.Pan = Noise();
				D.Wet = 0.1f;
				StartVoice(D);
			}
		}

		// Wind: slow, low gusts.
		if (Wind > 0.001f)
		{
			WindPhase += Dt * 0.13;
			const float Gust = 0.55f + 0.45f * FMath::Sin((float)(WindPhase * 2.0 * PI)) * FMath::Sin((float)(WindPhase * 0.37 * 2.0 * PI + 1.0));
			WindLp += 0.006f * (Noise() - WindLp);
			WindLp2 += 0.02f * (WindLp - WindLp2);
			const float W = WindLp2 * 1.6f * Wind * Gust;
			L += W;
			R += W * 0.85f;
		}

		// Score: a low drone and sparse notes.
		if (Music > 0.001f)
		{
			const float Freq[3] = { 55.f, 82.41f, Warmth > 0.5f ? 138.59f : 130.81f };
			float Drone = 0;
			for (int32 I = 0; I < 3; ++I)
			{
				DronePhase[I] += Freq[I] * Dt;
				if (DronePhase[I] > 1.0) { DronePhase[I] -= 1.0; }
				const float Swell = 0.6f + 0.4f * FMath::Sin((float)(MusicClock * (0.05 + I * 0.031) * 2.0 * PI + I));
				Drone += FMath::Sin((float)(DronePhase[I] * 2.0 * PI)) * Swell * (I == 2 ? 0.25f : 0.5f);
			}
			Drone *= 0.03f * Music;
			L += Drone;
			R += Drone;
			Wet += Drone * 0.4f;
			MusicClock += Dt;
			if (MusicClock >= NextNote)
			{
				NextNote = MusicClock + 2.4 + ((Rng >> 7) & 1023) / 1023.0 * 3.4;
				PlayNote();
			}
		}

		// Voices
		for (int32 I = 0; I < MaxVoices; ++I)
		{
			FVoice& V = Voices[I];
			if (!V.bActive) { continue; }
			V.T += Dt;
			if (V.T < 0) { continue; }
			const FVoiceSpec& S = V.S;
			if (V.T > S.Dur + S.Attack) { V.bActive = false; continue; }
			const float T = (float)V.T;
			const float Env = T < S.Attack ? T / S.Attack : FMath::Exp(-(T - S.Attack) * 6.9f / S.Dur);
			float Sample = 0;
			if (S.Tone > 0)
			{
				const float K = FMath::Clamp(T / S.Dur, 0.f, 1.f);
				const float Hz = S.F0 * FMath::Pow(FMath::Max(1.f, S.F1) / FMath::Max(1.f, S.F0), K);
				V.Phase += Hz * Dt;
				if (V.Phase > 1.0) { V.Phase -= 1.0; }
				const float P = (float)(V.Phase * 2.0 * PI);
				Sample += S.Tone * (FMath::Sin(P) + S.Harm * FMath::Sin(P * 2.f) * FMath::Exp(-T * 3.f));
			}
			if (S.Noise > 0)
			{
				V.Lp1 += S.Cut1 * (Noise() - V.Lp1);
				V.Lp2 += S.Cut2 * (V.Lp1 - V.Lp2);
				Sample += S.Noise * (V.Lp1 - V.Lp2) * 2.f;
			}
			Sample *= Env * S.Amp * (S.bMusicBus ? Music : Sfx);
			const float PanR = 0.5f + 0.5f * S.Pan;
			L += Sample * FMath::Sqrt(1.f - PanR);
			R += Sample * FMath::Sqrt(PanR);
			Wet += Sample * S.Wet;
		}

		const float Verb = Reverb(Wet);
		L += Verb * 0.55f;
		R += Verb * 0.55f;
		// gentle soft clip
		OutAudio[F * 2 + 0] = FMath::Clamp(L / (1.f + FMath::Abs(L) * 0.5f), -1.f, 1.f);
		OutAudio[F * 2 + 1] = FMath::Clamp(R / (1.f + FMath::Abs(R) * 0.5f), -1.f, 1.f);
	}
	return NumSamples;
}
