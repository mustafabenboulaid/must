// AAA City Weapons - chapter 1 sounds made in code (no assets to import): typewriter tick, objective chime, pickup,
// heartbeat, chapter-complete chord, and the opening music (a low drone with a bendir-like frame drum).
// Each sound is synthesised once (mono 16-bit, 32 kHz) and played through a fresh USoundWaveProcedural.
#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundWaveProcedural.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"

namespace AAAMissionAudio
{
	constexpr int32 Rate = 32000;

	enum class ESnd : uint8 { Tick, Chime, Pickup, Heart, Complete, Music, Num };

	inline float Env(float T, float Attack, float Decay) { return T < Attack ? T / FMath::Max(Attack, 1e-4f) : FMath::Exp(-(T - Attack) / FMath::Max(Decay, 1e-4f)); }

	inline void Bell(TArray<float>& Buf, float At, float Freq, float Amp, float Decay)
	{
		static const float Ratio[4] = { 1.f, 2.76f, 5.40f, 8.93f };
		static const float Gain[4] = { 1.f, 0.42f, 0.18f, 0.07f };
		const int32 S0 = FMath::RoundToInt(At * Rate), N = FMath::RoundToInt(Decay * 6.f * Rate);
		for (int32 I = 0; I < N && S0 + I < Buf.Num(); ++I)
		{
			const float T = (float)I / Rate;
			float V = 0.f;
			for (int32 K = 0; K < 4; ++K) V += Gain[K] * FMath::Sin(2.f * UE_PI * Freq * Ratio[K] * T) * FMath::Exp(-T * (1.f + K * 1.4f) / Decay);
			Buf[S0 + I] += Amp * V * FMath::Min(1.f, T / 0.004f);
		}
	}

	inline void Thump(TArray<float>& Buf, float At, float Freq, float Amp, float Decay, float Noise = 0.f, uint32 Seed = 7)
	{
		FRandomStream R(Seed);
		const int32 S0 = FMath::RoundToInt(At * Rate), N = FMath::RoundToInt(Decay * 6.f * Rate);
		float Ph = 0.f, Lp = 0.f;
		for (int32 I = 0; I < N && S0 + I < Buf.Num(); ++I)
		{
			const float T = (float)I / Rate;
			const float F = Freq * (1.f + 0.6f * FMath::Exp(-T / 0.02f));   // pitch drops (skin)
			Ph += 2.f * UE_PI * F / Rate;
			Lp += (R.FRandRange(-1.f, 1.f) - Lp) * 0.35f;                   // softened noise (jingles / slap)
			Buf[S0 + I] += Amp * (FMath::Sin(Ph) * FMath::Exp(-T / Decay) + Noise * Lp * FMath::Exp(-T / (Decay * 0.35f))) * FMath::Min(1.f, T / 0.002f);
		}
	}

	inline TArray<int16> Make(ESnd S)
	{
		TArray<float> B;
		switch (S)
		{
		case ESnd::Tick:   // a soft typewriter key
		{
			B.SetNumZeroed(Rate * 6 / 100);
			FRandomStream R(3);
			for (int32 I = 0; I < B.Num(); ++I) { const float T = (float)I / Rate; B[I] = (0.5f * R.FRandRange(-1.f, 1.f) + 0.5f * FMath::Sin(2.f * UE_PI * 2900.f * T)) * FMath::Exp(-T / 0.006f); }
			Thump(B, 0.f, 180.f, 0.25f, 0.012f);
			break;
		}
		case ESnd::Chime:   // new objective / objective done: two bell notes (E5, B5)
			B.SetNumZeroed(Rate * 2);
			Bell(B, 0.f, 659.3f, 0.32f, 0.35f);
			Bell(B, 0.13f, 987.8f, 0.28f, 0.45f);
			break;
		case ESnd::Pickup:   // a rifle / crate taken: a wooden-metal clack and a small bell
			B.SetNumZeroed(Rate * 1);
			Thump(B, 0.f, 140.f, 0.55f, 0.05f, 0.8f, 11);
			Thump(B, 0.045f, 220.f, 0.35f, 0.04f, 0.6f, 12);
			Bell(B, 0.09f, 1318.5f, 0.12f, 0.2f);
			break;
		case ESnd::Heart:   // lub-dub
			B.SetNumZeroed(Rate * 7 / 10);
			Thump(B, 0.f, 52.f, 0.9f, 0.07f, 0.05f);
			Thump(B, 0.27f, 46.f, 0.65f, 0.08f, 0.05f);
			break;
		case ESnd::Complete:   // chapter complete: a slow minor arpeggio resolving to the major (A)
		{
			B.SetNumZeroed(Rate * 6);
			const float Notes[6] = { 220.f, 261.6f, 329.6f, 440.f, 554.4f, 659.3f };
			for (int32 K = 0; K < 6; ++K) Bell(B, K * 0.32f, Notes[K], 0.2f, 0.9f);
			Thump(B, 0.f, 70.f, 0.5f, 0.3f, 0.3f);
			break;
		}
		case ESnd::Music:   // ~32 s: a low drone (A, E) breathing slowly, a soft pad a fifth up, a bendir pulse (dum . . tak dum . .)
		{
			const int32 N = Rate * 32;
			B.SetNumZeroed(N);
			for (int32 I = 0; I < N; ++I)
			{
				const float T = (float)I / Rate;
				const float Breath = 0.6f + 0.4f * FMath::Sin(2.f * UE_PI * T / 9.f);
				const float Fade = FMath::Min(1.f, T / 4.f) * FMath::Min(1.f, (32.f - T) / 3.f);
				float V = 0.18f * FMath::Sin(2.f * UE_PI * 110.f * T) + 0.08f * FMath::Sin(2.f * UE_PI * 220.3f * T + 0.4f)
					+ 0.11f * FMath::Sin(2.f * UE_PI * 164.8f * T) * Breath + 0.05f * FMath::Sin(2.f * UE_PI * 329.9f * T) * (1.f - Breath)
					+ 0.03f * FMath::Sin(2.f * UE_PI * 440.f * T + FMath::Sin(2.f * UE_PI * 0.3f * T));
				B[I] = V * Fade;
			}
			for (float T = 6.f; T < 29.f; T += 2.4f)
			{
				Thump(B, T, 68.f, 0.55f, 0.22f, 0.15f, (uint32)(T * 10));
				Thump(B, T + 0.9f, 190.f, 0.22f, 0.05f, 0.9f, (uint32)(T * 10 + 1));
				Thump(B, T + 1.2f, 70.f, 0.35f, 0.18f, 0.1f, (uint32)(T * 10 + 2));
			}
			break;
		}
		default: break;
		}
		float Peak = 0.001f;
		for (const float V : B) Peak = FMath::Max(Peak, FMath::Abs(V));
		const float K = 0.89f / FMath::Max(Peak, 0.89f);   // only scaled down when it would clip
		TArray<int16> Out;
		Out.SetNumUninitialized(B.Num());
		for (int32 I = 0; I < B.Num(); ++I) Out[I] = (int16)FMath::Clamp(FMath::RoundToInt(B[I] * K * 32767.f), -32768, 32767);
		return Out;
	}

	inline const TArray<int16>& Get(ESnd S)
	{
		static TArray<int16> Cache[(int32)ESnd::Num];
		TArray<int16>& C = Cache[(int32)S];
		if (C.Num() == 0) C = Make(S);
		return C;
	}

	/** play (2D) and return the component (to fade the music out); Vol 0..1 */
	inline UAudioComponent* Play(UWorld* W, ESnd S, float Vol = 1.f, float Pitch = 1.f)
	{
		if (!W || W->GetNetMode() == NM_DedicatedServer) return nullptr;
		const TArray<int16>& Pcm = Get(S);
		if (Pcm.Num() == 0) return nullptr;
		USoundWaveProcedural* Wv = NewObject<USoundWaveProcedural>(GetTransientPackage());
		Wv->SetSampleRate(Rate);
		Wv->NumChannels = 1;
		Wv->Duration = (float)Pcm.Num() / Rate;
		Wv->SoundGroup = SOUNDGROUP_Default;
		Wv->bLooping = false;
		Wv->QueueAudio(reinterpret_cast<const uint8*>(Pcm.GetData()), Pcm.Num() * sizeof(int16));
		return UGameplayStatics::SpawnSound2D(W, Wv, Vol, Pitch, 0.f, nullptr, false, true);
	}
}
