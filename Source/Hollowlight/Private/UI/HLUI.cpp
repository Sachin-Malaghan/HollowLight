// EMBERHOME: menus, HUD overlay, touch controls and the procedural serif title. (CLAUDE.md: UI)
#include "UI/HLUI.h"

#include "Core/HLLevel.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/HLGame.h"
#include "Game/HLSaveGame.h"
#include "Render/HLDraw.h"
#include "Rendering/SlateRenderer.h"
#include "CanvasItem.h"

using namespace HL;

namespace
{
	const FLinearColor Ink = HLColor(0xebe7dc);
	const FLinearColor Dim = HLColor(0x9d9a92);
	const FLinearColor Faint = HLColor(0x6d6b66);
	const FLinearColor Ember = HLColor(0xffc46e);

	FLinearColor WithAlpha(FLinearColor C, double A)
	{
		C.A *= (float)FMath::Clamp(A, 0.0, 1.0);
		return C;
	}

	FString Roman(int32 N)
	{
		static const int32 Val[] = { 50, 40, 10, 9, 5, 4, 1 };
		static const TCHAR* Sym[] = { TEXT("L"), TEXT("XL"), TEXT("X"), TEXT("IX"), TEXT("V"), TEXT("IV"), TEXT("I") };
		FString Out;
		for (int32 I = 0; I < 7 && N > 0; ++I)
		{
			while (N >= Val[I]) { Out += Sym[I]; N -= Val[I]; }
		}
		return Out;
	}

	FString FormatTime(double Seconds)
	{
		const int32 M = FMath::FloorToInt(Seconds / 60.0);
		const double S = Seconds - M * 60.0;
		return FString::Printf(TEXT("%d:%04.1f"), M, S);
	}

	// ---------------------------------------------------------------------------------------------
	// Text through the Slate font cache (crisp at any size).

	FSlateFontInfo FontInfo(UFont* Font, double Px, int32 Spacing, bool bBold)
	{
		FSlateFontInfo Info(Font, FMath::Max(1, FMath::RoundToInt(Px * 0.75)), bBold ? FName(TEXT("Bold")) : FName(TEXT("Light")));
		Info.LetterSpacing = Spacing;
		return Info;
	}

	FVector2D Measure(const FString& S, const FSlateFontInfo& Info, double Px)
	{
		if (FSlateApplication::IsInitialized())
		{
			const TSharedRef<FSlateFontMeasure> M = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			return M->Measure(S, Info);
		}
		return FVector2D(S.Len() * Px * 0.55, Px * 1.2);
	}

	struct FText2
	{
		FHLDraw& D;
		UCanvas* Canvas;
		UFont* Font;

		// Align: 0 left, 0.5 centre, 1 right. Y is the vertical centre of the line.
		FVector2D Draw(const FString& S, double X, double Y, double Px, const FLinearColor& Color, double Align = 0.5, int32 Spacing = 0, bool bBold = false)
		{
			if (!Font || S.IsEmpty() || Color.A <= 0.001f) { return FVector2D::ZeroVector; }
			D.Flush();
			const FSlateFontInfo Info = FontInfo(Font, Px, Spacing, bBold);
			const FVector2D Size = Measure(S, Info, Px);
			FCanvasTextItem Item(FVector2D(X - Size.X * Align, Y - Size.Y * 0.5), FText::FromString(S), Info, Color);
			Item.BlendMode = SE_BLEND_Translucent;
			// A soft shadow keeps text legible over bright mist.
			Item.EnableShadow(FLinearColor(0, 0, 0, 0.6f * Color.A), FVector2D(1, 1) * FMath::Max(1.0, Px * 0.05));
			Canvas->DrawItem(Item);
			return Size;
		}

		FVector2D Size(const FString& S, double Px, int32 Spacing = 0, bool bBold = false) const
		{
			return Measure(S, FontInfo(Font, Px, Spacing, bBold), Px);
		}
	};

	// ---------------------------------------------------------------------------------------------
	// Serif glyphs, drawn in pixels. Coordinates in letter heights: x right, y up from the baseline.

	double GlyphAdvance(TCHAR C)
	{
		switch (C)
		{
		case 'H': return 0.66;
		case 'O': return 0.78;
		case 'L': return 0.56;
		case 'W': return 1.02;
		case 'I': return 0.22;
		case 'G': return 0.76;
		case 'T': return 0.68;
		case 'V': return 0.66;
		case 'X': return 0.66;
		case 'E': return 0.58;
		case 'M': return 0.86;
		case 'B': return 0.62;
		case 'R': return 0.64;
		default: return 0.40;
		}
	}

	struct FGlyphPen
	{
		FHLDraw& D;
		double OX, BY, H;
		FLinearColor C;
		double Thick() const { return 0.088 * H; }
		double Thin() const { return 0.030 * H; }
		FVector2D P(double X, double Y) const { return FVector2D(OX + X * H, BY - Y * H); }
		void Bar(double X0, double Y0, double X1, double Y1) const
		{
			const FVector2D A = P(X0, Y0), B = P(X1, Y1);
			D.Rect(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y), FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y), C);
		}
		void Stroke(double X0, double Y0, double X1, double Y1, double W) const
		{
			const FVector2D A = P(X0, Y0), B = P(X1, Y1);
			D.Line(A.X, A.Y, B.X, B.Y, W, C);
		}
		void Stem(double XC) const { Bar(XC - 0.044, 0, XC + 0.044, 1); }
		void Serif(double XC, double Y, double Half = 0.1) const
		{
			const double T = 0.03;
			Bar(XC - Half, Y > 0.5 ? Y - T : Y, XC + Half, Y > 0.5 ? Y : Y + T);
			// bracket the serif into the stem
			const double Dir = Y > 0.5 ? -1 : 1;
			const FVector2D A = P(XC - 0.075, Y + Dir * T), B = P(XC + 0.075, Y + Dir * T), K = P(XC, Y + Dir * (T + 0.07));
			D.Tri(A.X, A.Y, B.X, B.Y, K.X, K.Y, C);
		}
		// Ring with vertical stress between angles (degrees, 0 = right, counter-clockwise, y up).
		void Bowl(double CX, double CY, double RX, double RY, double A0, double A1) const
		{
			const int N = 40;
			for (int I = 0; I < N; ++I)
			{
				const double T0 = FMath::DegreesToRadians(FMath::Lerp(A0, A1, double(I) / N));
				const double T1 = FMath::DegreesToRadians(FMath::Lerp(A0, A1, double(I + 1) / N));
				auto Outer = [&](double T) { return P(CX + FMath::Cos(T) * RX, CY + FMath::Sin(T) * RY); };
				auto Inner = [&](double T) { return P(CX + 0.012 + FMath::Cos(T) * (RX - 0.092), CY + FMath::Sin(T) * (RY - 0.034)); };
				D.Quad(Outer(T0), Outer(T1), Inner(T1), Inner(T0), C);
			}
		}
	};

	void DrawGlyph(FHLDraw& D, TCHAR Ch, double OX, double BY, double H, const FLinearColor& Col)
	{
		const FGlyphPen G{ D, OX, BY, H, Col };
		switch (Ch)
		{
		case 'H':
			G.Stem(0.10); G.Stem(0.56);
			G.Bar(0.10, 0.50, 0.56, 0.53);
			G.Serif(0.10, 1); G.Serif(0.10, 0); G.Serif(0.56, 1); G.Serif(0.56, 0);
			break;
		case 'O':
			G.Bowl(0.39, 0.5, 0.37, 0.515, 0, 360);
			break;
		case 'L':
			G.Stem(0.10);
			G.Bar(0.10, 0, 0.52, 0.032);
			G.Bar(0.49, 0, 0.52, 0.16);
			G.Serif(0.10, 1); G.Bar(0.0, 0, 0.12, 0.03);
			break;
		case 'W':
			G.Stroke(0.05, 1, 0.28, 0.0, G.Thick());
			G.Stroke(0.28, 0.0, 0.51, 0.93, G.Thin() * 1.2);
			G.Stroke(0.51, 0.93, 0.74, 0.0, G.Thick());
			G.Stroke(0.74, 0.0, 0.97, 1, G.Thin() * 1.2);
			G.Bar(-0.04, 0.97, 0.16, 1.0); G.Bar(0.43, 0.97, 0.59, 1.0); G.Bar(0.88, 0.97, 1.05, 1.0);
			break;
		case 'I':
			G.Stem(0.11); G.Serif(0.11, 1); G.Serif(0.11, 0);
			break;
		case 'G':
			G.Bowl(0.39, 0.5, 0.36, 0.515, 18, 338);
			G.Bar(0.44, 0.40, 0.72, 0.43);
			G.Bar(0.64, 0.06, 0.72, 0.43);
			G.Bar(0.66, 0.70, 0.69, 0.86);
			break;
		case 'T':
			G.Bar(0.0, 0.97, 0.68, 1.0);
			G.Bar(0.0, 0.84, 0.03, 1.0); G.Bar(0.65, 0.84, 0.68, 1.0);
			G.Stem(0.34); G.Serif(0.34, 0);
			break;
		case 'V':
			G.Stroke(0.03, 1, 0.33, 0.0, G.Thick());
			G.Stroke(0.33, 0.0, 0.63, 1, G.Thin() * 1.2);
			G.Bar(-0.06, 0.97, 0.14, 1.0); G.Bar(0.54, 0.97, 0.72, 1.0);
			break;
		case 'X':
			G.Stroke(0.06, 1, 0.60, 0.0, G.Thick());
			G.Stroke(0.60, 1, 0.06, 0.0, G.Thin() * 1.2);
			G.Bar(-0.04, 0.97, 0.18, 1.0); G.Bar(0.50, 0.97, 0.70, 1.0);
			G.Bar(-0.04, 0.0, 0.18, 0.03); G.Bar(0.48, 0.0, 0.70, 0.03);
			break;
		case 'E':
			G.Stem(0.10);
			G.Bar(0.10, 0.97, 0.52, 1.0); G.Bar(0.49, 0.84, 0.52, 1.0);     // top arm + beak serif
			G.Bar(0.10, 0.50, 0.44, 0.53); G.Bar(0.41, 0.43, 0.44, 0.60);   // middle arm
			G.Bar(0.10, 0.0, 0.54, 0.03); G.Bar(0.51, 0.0, 0.54, 0.17);     // bottom arm + beak serif
			G.Bar(0.0, 0.97, 0.12, 1.0); G.Bar(0.0, 0.0, 0.12, 0.03);
			break;
		case 'M':
			G.Stroke(0.08, 0.0, 0.08, 1.0, G.Thin() * 1.3);                // thin left stem
			G.Stroke(0.08, 1.0, 0.43, 0.02, G.Thick());                    // thick diagonal down
			G.Stroke(0.43, 0.02, 0.76, 1.0, G.Thin() * 1.3);               // thin diagonal up
			G.Stem(0.76);                                                 // thick right stem
			G.Bar(-0.02, 0.0, 0.18, 0.03); G.Bar(0.66, 0.0, 0.86, 0.03);
			G.Bar(-0.02, 0.97, 0.10, 1.0); G.Serif(0.76, 1);
			break;
		case 'B':
			G.Stem(0.10);
			G.Bar(0.10, 0.97, 0.36, 1.0); G.Bar(0.10, 0.49, 0.37, 0.52); G.Bar(0.10, 0.0, 0.37, 0.03);
			G.Bowl(0.36, 0.755, 0.20, 0.245, -90, 90);
			G.Bowl(0.37, 0.255, 0.24, 0.255, -90, 90);
			G.Bar(0.0, 0.97, 0.12, 1.0); G.Bar(0.0, 0.0, 0.12, 0.03);
			break;
		case 'R':
			G.Stem(0.10);
			G.Bar(0.10, 0.97, 0.36, 1.0); G.Bar(0.10, 0.475, 0.36, 0.505);
			G.Bowl(0.36, 0.7375, 0.21, 0.2625, -90, 90);
			G.Stroke(0.33, 0.49, 0.58, 0.015, G.Thick());                  // leg
			G.Bar(0.0, 0.97, 0.12, 1.0); G.Bar(0.0, 0.0, 0.20, 0.03); G.Bar(0.50, 0.0, 0.66, 0.03);
			break;
		default:
			break;
		}
	}

	// ---------------------------------------------------------------------------------------------

	struct FScreenCtx
	{
		FHLDraw& D;
		UCanvas* Canvas;
		FHLGame& Game;
		const FHLUiContext& Ctx;
		FText2 Text;
		double W, H;
		double U;   // one "unit": screen height / 400

		// A text button; registers its box with the game for pointer and keyboard input.
		void Button(const FString& Label, double CX, double CY, double Px, EHLAction Action, int32 Param = 0, bool bEnabled = true, double Alpha = 1.0)
		{
			const FVector2D S = Text.Size(Label, Px, 180);
			const double PadX = Px * 0.9, PadY = FMath::Max(Px * 0.45, (0.11 * H - S.Y) * 0.5);
			FHLButton B;
			B.Box = FBox2D(FVector2D(CX - S.X * 0.5 - PadX, CY - S.Y * 0.5 - PadY), FVector2D(CX + S.X * 0.5 + PadX, CY + S.Y * 0.5 + PadY));
			B.Action = Action;
			B.Param = Param;
			B.bEnabled = bEnabled;
			const int32 Index = Game.Buttons.Add(B);
			const bool bFocus = Index == Game.Focus && bEnabled;
			const FLinearColor Col = WithAlpha(bEnabled ? (bFocus ? Ink : Dim) : Faint, Alpha);
			Text.Draw(Label, CX, CY, Px, Col, 0.5, 180);
			if (bFocus)
			{
				const double Y = CY + S.Y * 0.5 + Px * 0.12;
				D.SetBlend(SE_BLEND_Translucent);
				D.SetTransform(1, 0, 0);
				D.Rect(CX - S.X * 0.5, Y, CX + S.X * 0.5, Y + FMath::Max(1.0, U * 0.6), WithAlpha(Ember, 0.8 * Alpha));
				D.SetBlend(SE_BLEND_Additive);
				D.Glow(CX - S.X * 0.5 - Px * 0.7, CY, Px * 0.55, FLinearColor(0.6f * (float)Alpha, 0.35f * (float)Alpha, 0.1f * (float)Alpha, 1), FLinearColor(0, 0, 0, 1), 12);
				D.SetBlend(SE_BLEND_Translucent);
			}
		}

		void Darken(double Alpha)
		{
			D.SetBlend(SE_BLEND_Translucent);
			D.SetTransform(1, 0, 0);
			D.ScreenRect(0, 0, W, H, FLinearColor(0, 0, 0, (float)Alpha));
		}

		double SafeL() const { return Ctx.Safe.X; }
		double SafeT() const { return Ctx.Safe.Y; }
		double SafeR() const { return Ctx.Safe.Z; }
		double SafeB() const { return Ctx.Safe.W; }
	};

	FString ControlsHint(const FHLUiContext& Ctx)
	{
		return Ctx.bShowTouch ? FString(TEXT("hold  ◀  ▶  to run   ·   JUMP   ·   SLIDE   ·   or swipe up / down"))
		                      : FString(TEXT("← →  or  A D  to run   ·   SPACE  jump   ·   S  ↓  slide   ·   E  whistle / take / use   ·   ESC  pause"));
	}

	// The round button is named for what it would do right now.
	const TCHAR* ActLabel(const HL::FSim& Sim)
	{
		switch (Sim.ActKind())
		{
		case HL::EActKind::Take: return TEXT("TAKE");
		case HL::EActKind::Use: return TEXT("USE");
		case HL::EActKind::Lever: return TEXT("PULL");
		default: return TEXT("WHISTLE");
		}
	}

	// Whistling is for the dog: no dog, nothing in reach, no button.
	bool ShowAct(const HL::FSim& Sim)
	{
		return Sim.Dog.Active || Sim.ActKind() != HL::EActKind::Whistle;
	}

	void DrawTitle(FScreenCtx& S)
	{
		FHLGame& G = S.Game;
		const double In = SmoothStep(0.0, 1.4, G.ScreenTime);
		S.Darken(0.30 * In);
		// soft darker band behind the title for legibility
		S.D.SetTransform(1, 0, 0);
		S.D.RectV(0, S.H * 0.16, S.W, S.H * 0.42, FLinearColor(0, 0, 0, 0), FLinearColor(0, 0, 0, (float)(0.22 * In)));
		S.D.RectV(0, S.H * 0.42, S.W, S.H * 0.62, FLinearColor(0, 0, 0, (float)(0.22 * In)), FLinearColor(0, 0, 0, 0));

		const double TitleH = S.H * 0.085;
		FHLUI::DrawSerifWord(S.D, TEXT("EMBERHOME"), S.W * 0.5, S.H * 0.36, TitleH, TitleH * 0.42, WithAlpha(Ink, In));
		S.Text.Draw(TEXT("bring the light home"), S.W * 0.5, S.H * 0.36 + TitleH * 0.75, S.H * 0.026, WithAlpha(Dim, In), 0.5, 420);

		const double Px = S.H * 0.036;
		const bool bStarted = G.Save->LastLevel > 0 || G.Save->UnlockedLevels > 1 || G.Save->LastCheckpoint >= 0;
		const double Y0 = S.H * 0.60;
		S.Button(bStarted ? TEXT("CONTINUE") : TEXT("PLAY"), S.W * 0.5, Y0, Px * 1.15, EHLAction::Play, 0, true, In);
		S.Button(TEXT("LEVELS"), S.W * 0.5, Y0 + S.H * 0.095, Px, EHLAction::Levels, 0, true, In);
		S.Button(TEXT("SETTINGS"), S.W * 0.5, Y0 + S.H * 0.18, Px, EHLAction::Settings, 0, true, In);
		if (FHLGame::PlatformHasQuitButton())
		{
			S.Button(TEXT("QUIT"), S.W * 0.5, Y0 + S.H * 0.265, Px, EHLAction::Quit, 0, true, In);
		}

		S.Text.Draw(ControlsHint(S.Ctx), S.W * 0.5, S.H - S.SafeB() - S.H * 0.05, S.H * 0.022, WithAlpha(Dim, In * 0.9), 0.5, 120);
		if (bStarted)
		{
			const int32 Cairns = (int32)GetLevels()[G.Save->LastLevel].Checkpoints.size();
			const FString At = G.Save->LastCheckpoint >= 0 ? FString::Printf(TEXT("cairn %d of %d"), G.Save->LastCheckpoint + 1, Cairns) : FString(TEXT("from the start"));
			const FString Where = FString::Printf(TEXT("%s  ·  %s  ·  %s  ·  %d of %d lights home"), *Roman(G.Save->LastLevel + 1),
				UTF8_TO_TCHAR(GetLevels()[G.Save->LastLevel].Name.c_str()), *At, G.NumCleared(), G.NumLevels());
			S.Text.Draw(Where, S.W * 0.5, Y0 + S.H * 0.045, S.H * 0.02, WithAlpha(Faint, In), 0.5, 200);
		}
	}

	void DrawBack(FScreenCtx& S, double Alpha)
	{
		S.Button(TEXT("‹  BACK"), S.SafeL() + S.H * 0.13, S.SafeT() + S.H * 0.08, S.H * 0.028, EHLAction::Back, 0, true, Alpha);
	}

	// A small silhouette of the place, behind the numeral of an unlocked chapter.
	void DrawLevelIcon(FScreenCtx& S, int32 Index, const FBox2D& Bx, double In)
	{
		const double X0 = Bx.Min.X + 2, X1 = Bx.Max.X - 2, Y0 = Bx.Min.Y + 2, Y1 = Bx.Min.Y + (Bx.Max.Y - Bx.Min.Y) * 0.60;
		const double W = X1 - X0, H = Y1 - Y0;
		S.D.SetTransform(1, 0, 0);
		S.D.SetBlend(SE_BLEND_Translucent);
		S.D.RectV(X0, Y0, X1, Y1, FLinearColor(1, 1, 1, (float)(0.13 * In)), FLinearColor(1, 1, 1, (float)(0.04 * In)));   // a patch of grey sky
		const FLinearColor K(0, 0, 0, (float)(0.8 * In)), G(1, 1, 1, (float)(0.22 * In));
		auto R = [&](double U0, double V0, double U1, double V1, const FLinearColor& C) { S.D.Rect(X0 + U0 * W, Y0 + V0 * H, X0 + U1 * W, Y0 + V1 * H, C); };
		auto T = [&](double U0, double V0, double U1, double V1, double U2, double V2, const FLinearColor& C)
		{
			S.D.Tri(X0 + U0 * W, Y0 + V0 * H, X0 + U1 * W, Y0 + V1 * H, X0 + U2 * W, Y0 + V2 * H, C);
		};
		const double Gd = 0.84;
		if (Index != 4) { R(0, Gd, 1, 1, K); }
		switch (Index)
		{
		case 0:   // the wood: trunks and a hanging log
			R(0.10, 0, 0.19, Gd, K); R(0.40, 0, 0.45, Gd, K); R(0.70, 0, 0.82, Gd, K); R(0.91, 0, 0.95, Gd, K);
			R(0.50, 0.52, 0.66, 0.62, K); R(0.535, 0, 0.545, 0.52, K); R(0.615, 0, 0.625, 0.52, K);
			break;
		case 1:   // a hill and a dog
			T(0, Gd, 0.66, Gd, 0.32, 0.5, K);
			R(0.70, 0.66, 0.86, 0.75, K); R(0.84, 0.59, 0.91, 0.68, K); T(0.84, 0.59, 0.87, 0.59, 0.85, 0.52, K);
			T(0.70, 0.66, 0.70, 0.71, 0.63, 0.58, K); R(0.71, 0.75, 0.735, Gd, K); R(0.83, 0.75, 0.855, Gd, K);
			break;
		case 2:   // jaws in the grass
			for (int32 I = 0; I < 8; ++I) { T(0.14 + I * 0.09, Gd, 0.22 + I * 0.09, Gd, 0.18 + I * 0.09, Gd - 0.2, K); }
			R(0.02, 0, 0.09, Gd, K);
			break;
		case 3:   // the ridge
			R(0.46, 0.38, 1, Gd, K); T(0.08, Gd, 0.46, Gd, 0.46, 0.38, K);
			R(0.80, 0, 0.86, 0.38, K);
			break;
		case 4:   // still water and a crate
			R(0, Gd, 0.28, 1, K); R(0.74, Gd, 1, 1, K); R(0.28, 0.93, 0.74, 1, K);
			R(0.28, 0.915, 0.74, 0.93, G); R(0.44, 0.72, 0.58, 0.93, K);
			break;
		case 5:   // racking
			R(0.15, 0.2, 0.18, Gd, K); R(0.82, 0.2, 0.85, Gd, K); R(0.15, 0.32, 0.85, 0.36, K); R(0.15, 0.57, 0.85, 0.61, K);
			R(0.24, 0.19, 0.37, 0.32, K); R(0.55, 0.42, 0.72, 0.57, K); R(0.30, 0.68, 0.45, Gd, K);
			R(0.598, 0, 0.606, 0.1, K); T(0.55, 0.16, 0.655, 0.16, 0.602, 0.09, K);
			break;
		case 6:   // a wagon on the sidings
			R(0.16, 0.38, 0.76, 0.70, K);
			S.D.Circle(X0 + 0.28 * W, Y0 + 0.76 * H, 0.085 * H, K, 14); S.D.Circle(X0 + 0.64 * W, Y0 + 0.76 * H, 0.085 * H, K, 14);
			R(0.89, 0.1, 0.91, Gd, K); R(0.85, 0.16, 0.95, 0.19, K);
			break;
		case 7:   // the station canopy
			R(0, 0.2, 1, 0.28, K);
			for (int32 I = 0; I < 10; ++I) { T(I * 0.1, 0.28, (I + 1) * 0.1, 0.28, (I + 0.5) * 0.1, 0.36, K); }
			R(0.2, 0.28, 0.23, Gd, K); R(0.77, 0.28, 0.8, Gd, K);
			S.D.Circle(X0 + 0.5 * W, Y0 + 0.56 * H, 0.09 * H, G, 16);
			break;
		case 8:   // the mountain
			T(0, Gd, 0.62, Gd, 0.3, 0.1, K); T(0.4, Gd, 1, Gd, 0.72, 0.28, K);
			break;
		case 9:   // the storm
			R(0.08, 0, 0.18, Gd, K); R(0.78, 0, 0.86, Gd, K);
			T(0.50, 0.04, 0.58, 0.04, 0.46, 0.42, FLinearColor(1, 1, 1, (float)(0.5 * In)));
			T(0.53, 0.36, 0.45, 0.36, 0.5, 0.8, FLinearColor(1, 1, 1, (float)(0.5 * In)));
			break;
		case 10:  // home: a house and the lamp post
			R(0.52, 0.5, 0.86, Gd, K); T(0.47, 0.5, 0.91, 0.5, 0.69, 0.24, K);
			R(0.2, 0.3, 0.22, Gd, K); R(0.2, 0.3, 0.3, 0.325, K);
			S.D.SetBlend(SE_BLEND_Additive);
			S.D.Glow(X0 + 0.3 * W, Y0 + 0.38 * H, 0.16 * H, FLinearColor(0.8f * (float)In, 0.5f * (float)In, 0.16f * (float)In, 1), FLinearColor(0, 0, 0, 1), 14);
			S.D.SetBlend(SE_BLEND_Translucent);
			break;
		case 11:  // the quarry: a rock face and a rope from a pulley
			R(0.5, 0.25, 1, Gd, K); R(0.40, 0.12, 0.415, Gd - 0.02, K); R(0.40, 0.10, 0.56, 0.13, K);
			S.D.Circle(X0 + 0.41 * W, Y0 + 0.16 * H, 0.05 * H, K, 12);
			break;
		case 12:  // the rope bridge
			R(0, 0.55, 0.2, 1, K); R(0.8, 0.55, 1, 1, K); R(0.18, 0.3, 0.2, 0.6, K); R(0.8, 0.3, 0.82, 0.6, K);
			for (int32 I = 0; I < 6; ++I) { if (I != 3) { R(0.22 + I * 0.1, 0.56, 0.3 + I * 0.1, 0.6, K); } }
			R(0.2, 0.40, 0.8, 0.415, K);
			break;
		case 13:  // the sawmill: a blade in the floor, a hammer above
			S.D.Circle(X0 + 0.32 * W, Y0 + Gd * H, 0.16 * H, K, 16);
			for (int32 I = 0; I < 7; ++I)
			{
				const double A = PI + I * (PI / 6.0);
				T(0.32 + FMath::Cos(A - 0.12) * 0.16 * H / W, Gd + FMath::Sin(A - 0.12) * 0.16, 0.32 + FMath::Cos(A + 0.12) * 0.16 * H / W, Gd + FMath::Sin(A + 0.12) * 0.16,
				  0.32 + FMath::Cos(A) * 0.22 * H / W, Gd + FMath::Sin(A) * 0.22, K);
			}
			R(0.66, 0, 0.70, 0.3, K); R(0.58, 0.3, 0.78, 0.52, K);
			break;
		case 14:  // the cistern: water coming up the steps
			R(0, 0.5, 0.3, Gd, K); R(0.3, 0.62, 0.6, Gd, K); R(0.6, 0.74, 1, Gd, K);
			R(0.3, 0.60, 1, 0.615, G); R(0.6, 0.66, 1, 0.672, G);
			break;
		case 15:  // the foundry: fire from a vent
		case 18:  // the burning mill
			if (Index == 18) { R(0.52, 0.34, 0.9, Gd, K); T(0.47, 0.34, 0.95, 0.34, 0.71, 0.12, K); }
			else { R(0.1, 0.1, 0.9, 0.16, K); R(0.14, 0.16, 0.17, Gd, K); R(0.83, 0.16, 0.86, Gd, K); }
			S.D.SetBlend(SE_BLEND_Additive);
			{
				const FLinearColor Fl(0.9f * (float)In, 0.5f * (float)In, 0.14f * (float)In, 1);
				const double FX = Index == 18 ? 0.28 : 0.5;
				T(FX - 0.09, Gd, FX + 0.09, Gd, FX + 0.01, 0.3, Fl); T(FX - 0.16, Gd, FX - 0.02, Gd, FX - 0.1, 0.5, Fl); T(FX + 0.03, Gd, FX + 0.16, Gd, FX + 0.11, 0.46, Fl);
				S.D.Glow(X0 + FX * W, Y0 + 0.6 * H, 0.3 * H, FLinearColor(0.35f * (float)In, 0.2f * (float)In, 0.06f * (float)In, 1), FLinearColor(0, 0, 0, 1), 14);
			}
			S.D.SetBlend(SE_BLEND_Translucent);
			break;
		case 16:  // wolf wood
		case 19:  // the pack
			if (Index == 16) { R(0.06, 0, 0.14, Gd, K); R(0.88, 0, 0.94, Gd, K); } else { T(0.5, Gd, 1, Gd, 0.8, 0.2, K); }
			for (int32 Wn = 0; Wn < (Index == 19 ? 2 : 1); ++Wn)
			{
				const double O = Wn * 0.26 - (Index == 19 ? 0.16 : 0.0);
				R(0.34 + O, 0.60, 0.56 + O, 0.70, K); R(0.54 + O, 0.50, 0.64 + O, 0.62, K); T(0.62 + O, 0.54, 0.62 + O, 0.60, 0.72 + O, 0.60, K);
				T(0.55 + O, 0.50, 0.59 + O, 0.50, 0.56 + O, 0.42, K); T(0.34 + O, 0.60, 0.34 + O, 0.65, 0.24 + O, 0.70, K);
				R(0.36 + O, 0.70, 0.385 + O, Gd, K); R(0.52 + O, 0.70, 0.545 + O, Gd, K);
				S.D.Circle(X0 + (0.60 + O) * W, Y0 + 0.545 * H, 0.012 * H, FLinearColor(1, 1, 1, (float)(0.9 * In)), 6);
			}
			break;
		case 17:  // the pump house: a shaft, a rope, water at the bottom
			R(0.2, 0, 0.26, Gd, K); R(0.74, 0, 0.8, Gd, K); R(0.26, 0.4, 0.5, 0.44, K); R(0.5, 0.2, 0.74, 0.24, K);
			R(0.47, 0, 0.485, 0.4, K); R(0.26, 0.7, 0.74, Gd, K); R(0.26, 0.69, 0.74, 0.705, G);
			break;
		case 20:  // first light: a low sun behind another village
			S.D.SetBlend(SE_BLEND_Additive);
			S.D.Glow(X0 + 0.5 * W, Y0 + Gd * H, 0.42 * H, FLinearColor(0.5f * (float)In, 0.32f * (float)In, 0.12f * (float)In, 1), FLinearColor(0, 0, 0, 1), 18);
			S.D.SetBlend(SE_BLEND_Translucent);
			R(0.16, 0.56, 0.4, Gd, K); T(0.12, 0.56, 0.44, 0.56, 0.28, 0.36, K);
			R(0.6, 0.62, 0.84, Gd, K); T(0.56, 0.62, 0.88, 0.62, 0.72, 0.44, K);
			R(0, Gd, 1, 1, K);
			break;
		default:
			break;
		}
	}

	FString FormatClock(double Seconds)
	{
		const int32 Ms = FMath::FloorToInt(Seconds * 1000.0 + 0.5);
		return FString::Printf(TEXT("%d:%02d.%03d"), Ms / 60000, (Ms / 1000) % 60, Ms % 1000);
	}

	void DrawLevelSelect(FScreenCtx& S)
	{
		FHLGame& G = S.Game;
		const double In = SmoothStep(0.0, 0.35, G.ScreenTime);
		S.Darken(0.62 * In);
		const int32 Page = FMath::Clamp(G.LevelPage, 0, G.NumChapters() - 1);
		static const TCHAR* ChapterNames[] = { TEXT("CHAPTER I   ·   THE WAY HOME"), TEXT("CHAPTER II   ·   THE LONG DARK") };
		S.Text.Draw(Page < 2 ? ChapterNames[Page] : TEXT("LEVELS"), S.W * 0.5, S.H * 0.13, S.H * 0.036, WithAlpha(Ink, In), 0.5, 400);

		const int32 N = G.NumLevels();
		FString Tally = FString::Printf(TEXT("%d of %d lights brought home   ·   %d of %d embers"), G.NumCleared(), N, G.EmbersFound(), G.EmbersTotal());
		if (G.TotalBestTime() > 0) { Tally += FString::Printf(TEXT("   ·   all levels %s"), *FormatClock(G.TotalBestTime())); }
		S.Text.Draw(Tally, S.W * 0.5, S.H * 0.19, S.H * 0.021, WithAlpha(Dim, In), 0.5, 120);
		const int32 Cols = N > 10 ? 6 : 5;
		const double GridW = FMath::Min(S.W - S.SafeL() - S.SafeR() - S.H * 0.2, S.H * 1.75);
		const double CellW = GridW / Cols, CellH = S.H * 0.27;
		const double X0 = S.W * 0.5 - GridW * 0.5, Y0 = S.H * 0.25;
		const int32 First = Page * FHLGame::kChapterSize, Last = FMath::Min(N, First + FHLGame::kChapterSize);
		for (int32 I = First; I < Last; ++I)
		{
			const int32 R = (I - First) / Cols, C = (I - First) % Cols;
			const double CX = X0 + CellW * (C + 0.5), CY = Y0 + CellH * (R + 0.5);
			const bool bOpen = I < G.Save->UnlockedLevels;
			FHLButton B;
			B.Box = FBox2D(FVector2D(CX - CellW * 0.45, CY - CellH * 0.44), FVector2D(CX + CellW * 0.45, CY + CellH * 0.44));
			B.Action = EHLAction::SelectLevel;
			B.Param = I;
			B.bEnabled = bOpen;
			const int32 Index = G.Buttons.Add(B);
			const bool bFocus = Index == G.Focus && bOpen;

			S.D.SetBlend(SE_BLEND_Translucent);
			S.D.SetTransform(1, 0, 0);
			const FLinearColor Edge = WithAlpha(bFocus ? Ember : (bOpen ? Dim : Faint), In * (bFocus ? 0.9 : 0.35));
			const double T = FMath::Max(1.0, S.U * 0.5);
			const FBox2D& Bx = B.Box;
			S.D.Rect(Bx.Min.X, Bx.Min.Y, Bx.Max.X, Bx.Min.Y + T, Edge);
			S.D.Rect(Bx.Min.X, Bx.Max.Y - T, Bx.Max.X, Bx.Max.Y, Edge);
			S.D.Rect(Bx.Min.X, Bx.Min.Y, Bx.Min.X + T, Bx.Max.Y, Edge);
			S.D.Rect(Bx.Max.X - T, Bx.Min.Y, Bx.Max.X, Bx.Max.Y, Edge);
			if (bFocus) { S.D.Rect(Bx.Min.X, Bx.Min.Y, Bx.Max.X, Bx.Max.Y, WithAlpha(Ember, 0.06 * In)); }
			if (bOpen)
			{
				DrawLevelIcon(S, I, Bx, In);
				// the embers of this chapter, top left
				const int32 Count = (int32)GetLevels()[I].Embers.size();
				S.D.SetBlend(SE_BLEND_Translucent);
				for (int32 K = 0; K < Count; ++K)
				{
					const bool bGot = G.Save->EmberMask.IsValidIndex(I) && ((G.Save->EmberMask[I] >> K) & 1);
					const double EX = Bx.Min.X + CellW * 0.08 + K * S.H * 0.017, EY = Bx.Min.Y + CellH * 0.1, ER = S.H * 0.006;
					S.D.Quad(FVector2D(EX, EY - ER), FVector2D(EX + ER * 0.75, EY), FVector2D(EX, EY + ER), FVector2D(EX - ER * 0.75, EY), bGot ? WithAlpha(Ember, In) : WithAlpha(Faint, In * 0.7));
				}
			}

			const FLinearColor NumCol = WithAlpha(bOpen ? Ink : Faint, In);
			FHLUI::DrawSerifWord(S.D, Roman(I + 1), CX, CY - CellH * 0.02, CellH * 0.24, CellH * 0.03, NumCol);
			const FString Name = UTF8_TO_TCHAR(GetLevels()[I].Name.c_str());
			S.Text.Draw(bOpen ? Name : FString(TEXT("· · ·")), CX, CY + CellH * 0.16, S.H * 0.021, WithAlpha(bOpen ? Dim : Faint, In), 0.5, 60);
			const float Best = G.Save->BestTimes.IsValidIndex(I) ? G.Save->BestTimes[I] : 0.f;
			if (bOpen && Best > 0)
			{
				S.Text.Draw(FString::Printf(TEXT("best %s"), *FormatClock(Best)), CX, CY + CellH * 0.28, S.H * 0.018, WithAlpha(Faint, In), 0.5, 80);
			}
			if (bOpen)
			{
				// One pip for each checkpoint of the level: lit as far as the child has got.
				const int32 Total = (int32)GetLevels()[I].Checkpoints.size(), Lit = G.ReachedCheckpoints(I);
				const double Gap = FMath::Min(CellW * 0.085, S.H * 0.02), PY = CY + CellH * 0.385, PipR = S.H * 0.0042;
				S.D.SetTransform(1, 0, 0);
				for (int32 K = 0; K < Total; ++K)
				{
					const double PX = CX + (K - (Total - 1) * 0.5) * Gap;
					S.D.SetBlend(SE_BLEND_Translucent);
					S.D.Circle(PX, PY, PipR, K < Lit ? WithAlpha(Ember, In) : WithAlpha(Faint, In * 0.6), 10);
				}
			}
			if (bOpen && Best > 0)
			{
				// a tiny lit wick on finished levels
				S.D.SetBlend(SE_BLEND_Additive);
				S.D.Glow(Bx.Max.X - CellW * 0.08, Bx.Min.Y + CellH * 0.1, S.H * 0.012, FLinearColor(0.9f, 0.55f, 0.18f, 1), FLinearColor(0, 0, 0, 1), 10);
				S.D.SetBlend(SE_BLEND_Translucent);
			}
		}
		// Turn the page to the other chapter.
		if (G.NumChapters() > 1)
		{
			const double BY = Y0 + CellH * 2.0 + S.H * 0.075;
			S.Button(TEXT("‹  CHAPTER I"), S.W * 0.5 - S.H * 0.3, BY, S.H * 0.026, EHLAction::PageLevels, -1, Page > 0, In * (Page > 0 ? 1.0 : 0.3));
			S.Button(TEXT("CHAPTER II  ›"), S.W * 0.5 + S.H * 0.3, BY, S.H * 0.026, EHLAction::PageLevels, 1, Page < G.NumChapters() - 1, In * (Page < G.NumChapters() - 1 ? 1.0 : 0.3));
		}
		DrawBack(S, In);
	}

	void DrawSettings(FScreenCtx& S)
	{
		FHLGame& G = S.Game;
		const double In = SmoothStep(0.0, 0.35, G.ScreenTime);
		S.Darken(0.64 * In);
		S.Text.Draw(TEXT("SETTINGS"), S.W * 0.5, S.H * 0.14, S.H * 0.042, WithAlpha(Ink, In), 0.5, 500);
		const UHLSaveGame* Sv = G.Save;
		auto OnOff = [](bool b) { return b ? TEXT("ON") : TEXT("OFF"); };
		const TCHAR* Touch = Sv->TouchMode == EHLTouchMode::Auto ? TEXT("AUTO") : (Sv->TouchMode == EHLTouchMode::On ? TEXT("ON") : TEXT("OFF"));
		const double Px = S.H * 0.028, Step = S.H * 0.075, Y = S.H * 0.245;
		S.Button(FString::Printf(TEXT("MUSIC   %s"), OnOff(Sv->bMusic)), S.W * 0.5, Y, Px, EHLAction::ToggleMusic, 0, true, In);
		S.Button(FString::Printf(TEXT("SOUND   %s"), OnOff(Sv->bSound)), S.W * 0.5, Y + Step, Px, EHLAction::ToggleSound, 0, true, In);
		S.Button(FString::Printf(TEXT("TOUCH CONTROLS   %s"), Touch), S.W * 0.5, Y + Step * 2, Px, EHLAction::CycleTouch, 0, true, In);
		S.Button(FString::Printf(TEXT("FILM GRAIN   %s"), OnOff(Sv->bFilmGrain)), S.W * 0.5, Y + Step * 3, Px, EHLAction::ToggleGrain, 0, true, In);
		S.Button(FString::Printf(TEXT("REDUCE FLASHING   %s"), OnOff(Sv->bReduceFlashing)), S.W * 0.5, Y + Step * 4, Px, EHLAction::ToggleFlashing, 0, true, In);
		S.Button(FString::Printf(TEXT("TIMER   %s"), OnOff(Sv->bTimer)), S.W * 0.5, Y + Step * 5, Px, EHLAction::ToggleTimer, 0, true, In);
		double Row = 6.0;
		if (S.Ctx.bShowTouch || S.Ctx.bTouchDevice)
		{
			S.Button(TEXT("CALIBRATE TOUCH"), S.W * 0.5, Y + Step * Row, Px, EHLAction::Calibrate, 0, true, In);
			Row += 1.0;
		}
		S.Button(TEXT("PRIVACY POLICY"), S.W * 0.5, Y + Step * Row, Px, EHLAction::PrivacyPolicy, 0, true, In);
		S.Button(TEXT("CREDITS"), S.W * 0.5, Y + Step * (Row + 1.0), Px, EHLAction::Credits, 0, true, In);
		DrawBack(S, In);
	}

	void DrawCredits(FScreenCtx& S)
	{
		FHLGame& G = S.Game;
		const double In = SmoothStep(0.0, 0.35, G.ScreenTime);
		S.Darken(0.7 * In);
		FHLUI::DrawSerifWord(S.D, TEXT("EMBERHOME"), S.W * 0.5, S.H * 0.25, S.H * 0.06, S.H * 0.025, WithAlpha(Ink, In));
		const TCHAR* Lines[] = {
			TEXT("a game by Brainrot Interactive Studios"),
			TEXT(""),
			TEXT("every tree, raindrop and sound is generated as you play"),
			TEXT("no images, no recordings"),
			TEXT(""),
			TEXT("made with Unreal Engine"),
		};
		double Y = S.H * 0.38;
		for (const TCHAR* L : Lines)
		{
			S.Text.Draw(L, S.W * 0.5, Y, S.H * 0.026, WithAlpha(Dim, In), 0.5, 120);
			Y += S.H * 0.055;
		}
		DrawBack(S, In);
	}

	// "Touch the light": two taps on known spots measure how this screen reports touches.
	void DrawCalibrate(FScreenCtx& S)
	{
		FHLGame& G = S.Game;
		const double In = SmoothStep(0.0, 0.4, G.ScreenTime);
		const float A = (float)In;
		S.Darken(0.82);
		const FVector2D T = FHLGame::CalibrateTarget(G.CalibrateStep);
		const double X = T.X * S.W, Y = T.Y * S.H;
		const float Pulse = (float)(0.8 + 0.2 * FMath::Sin(G.RealTime * 4.0)) * A;
		S.D.SetTransform(1, 0, 0);
		S.D.SetBlend(SE_BLEND_Additive);
		S.D.Glow(X, Y, S.H * 0.16, FLinearColor(0.75f * Pulse, 0.45f * Pulse, 0.14f * Pulse, 1), FLinearColor(0, 0, 0, 1), 32);
		S.D.Glow(X, Y, S.H * 0.035, FLinearColor(1.f * A, 0.78f * A, 0.42f * A, 1), FLinearColor(0, 0, 0, 1), 20);
		S.D.SetBlend(SE_BLEND_Translucent);
		// a thin cross so the exact centre is clear
		const double C = S.H * 0.05, W = FMath::Max(1.0, S.U * 0.4);
		S.D.Rect(X - C, Y - W, X + C, Y + W, WithAlpha(Ink, 0.5 * In));
		S.D.Rect(X - W, Y - C, X + W, Y + C, WithAlpha(Ink, 0.5 * In));
		S.Text.Draw(TEXT("Touch the light"), S.W * 0.5, S.H * 0.14, S.H * 0.05, WithAlpha(Ink, In), 0.5, 120);
		S.Text.Draw(G.CalibrateStep == 0 ? TEXT("tap the centre of the glow  ·  1 of 2") : TEXT("and once more  ·  2 of 2"),
			S.W * 0.5, S.H * 0.22, S.H * 0.024, WithAlpha(Dim, In), 0.5, 160);
		S.Button(TEXT("SKIP"), S.W * 0.5, S.H - S.SafeB() - S.H * 0.09, S.H * 0.026, EHLAction::SkipCalibrate, 0, true, In * 0.8);
	}

	void DrawPaused(FScreenCtx& S)
	{
		FHLGame& G = S.Game;
		const double In = SmoothStep(0.0, 0.25, G.ScreenTime);
		S.Darken(0.62 * In);
		S.Text.Draw(TEXT("PAUSED"), S.W * 0.5, S.H * 0.17, S.H * 0.045, WithAlpha(Ink, In), 0.5, 600);
		S.Text.Draw(FString::Printf(TEXT("%s  ·  %s"), *Roman(G.LevelIndex + 1), UTF8_TO_TCHAR(G.CurrentLevel().Name.c_str())),
			S.W * 0.5, S.H * 0.24, S.H * 0.024, WithAlpha(Dim, In), 0.5, 200);
		const double Px = S.H * 0.032, Step = S.H * 0.09, Y = S.H * 0.36;
		S.Button(TEXT("RESUME"), S.W * 0.5, Y, Px, EHLAction::Resume, 0, true, In);
		S.Button(TEXT("RESTART FROM CHECKPOINT"), S.W * 0.5, Y + Step, Px, EHLAction::RestartCheckpoint, 0, true, In);
		S.Button(TEXT("RESTART LEVEL"), S.W * 0.5, Y + Step * 2, Px, EHLAction::RestartLevel, 0, true, In);
		S.Button(TEXT("LEVELS"), S.W * 0.5, Y + Step * 3, Px, EHLAction::Levels, 0, true, In);
		S.Button(TEXT("SETTINGS"), S.W * 0.5, Y + Step * 4, Px, EHLAction::Settings, 0, true, In);
		S.Button(TEXT("QUIT TO TITLE"), S.W * 0.5, Y + Step * 5, Px, EHLAction::QuitToTitle, 0, true, In);
	}

	void DrawResults(FScreenCtx& S, bool bEnding)
	{
		FHLGame& G = S.Game;
		const double In = SmoothStep(0.0, 1.0, G.ScreenTime);
		S.Darken(0.5 * In);
		const double Y = S.H * 0.30;
		S.Text.Draw(bEnding ? TEXT("Every light is home.") : TEXT("The light is home."), S.W * 0.5, Y, S.H * 0.062, WithAlpha(Ink, In), 0.5, 60);
		if (bEnding)
		{
			S.Text.Draw(TEXT("thank you for carrying it"), S.W * 0.5, Y + S.H * 0.075, S.H * 0.026, WithAlpha(Dim, In), 0.5, 300);
		}
		FString Stats = FString::Printf(TEXT("%s  ·  %s   ·   time %s   ·   %d %s   ·   embers %d of %d"), *Roman(G.LevelIndex + 1),
			UTF8_TO_TCHAR(G.CurrentLevel().Name.c_str()), *FormatClock(G.ResultTime), G.ResultDeaths, G.ResultDeaths == 1 ? TEXT("fall") : TEXT("falls"),
			G.Sim.EmbersTaken(), (int32)G.Sim.EmberTaken.size());
		S.Text.Draw(Stats, S.W * 0.5, Y + S.H * (bEnding ? 0.14 : 0.09), S.H * 0.023, WithAlpha(Dim, In), 0.5, 120);
		if (G.bNewBest && G.ScreenTime > 0.6)
		{
			S.Text.Draw(TEXT("new best"), S.W * 0.5, Y + S.H * (bEnding ? 0.185 : 0.135), S.H * 0.02, WithAlpha(Ember, SmoothStep(0.6, 1.2, G.ScreenTime)), 0.5, 400);
		}

		const double Px = S.H * 0.034, BY = S.H * 0.62, Step = S.H * 0.09;
		const double BIn = SmoothStep(0.5, 1.3, G.ScreenTime);
		if (bEnding)
		{
			S.Button(TEXT("PLAY AGAIN"), S.W * 0.5, BY, Px * 1.1, EHLAction::PlayAgain, 0, true, BIn);
			S.Button(TEXT("LEVELS"), S.W * 0.5, BY + Step, Px, EHLAction::Levels, 0, true, BIn);
			S.Button(TEXT("TITLE"), S.W * 0.5, BY + Step * 2, Px, EHLAction::QuitToTitle, 0, true, BIn);
		}
		else
		{
			S.Button(TEXT("NEXT LEVEL"), S.W * 0.5, BY, Px * 1.1, EHLAction::NextLevel, 0, true, BIn);
			S.Button(TEXT("PLAY AGAIN"), S.W * 0.5, BY + Step, Px, EHLAction::Replay, 0, true, BIn);
			S.Button(TEXT("LEVELS"), S.W * 0.5, BY + Step * 2, Px, EHLAction::Levels, 0, true, BIn);
		}
	}

	void DrawTouchControls(FScreenCtx& S)
	{
		const FHLTouchLayout L = FHLTouchLayout::Compute(S.W, S.H, S.Ctx.Safe);
		S.D.SetTransform(1, 0, 0);
		S.D.SetBlend(SE_BLEND_Translucent);
		auto Pad = [&](const FVector2D& C, double R, bool bDown)
		{
			S.D.Circle(C.X, C.Y, R, FLinearColor(1, 1, 1, bDown ? 0.16f : 0.06f), 36);
			S.D.Ring(C.X, C.Y, R - FMath::Max(1.5, S.U * 0.7), R, FLinearColor(1, 1, 1, bDown ? 0.55f : 0.28f), FLinearColor(1, 1, 1, bDown ? 0.55f : 0.28f), 40);
		};
		Pad(L.Left, L.Radius, S.Ctx.bLeftDown);
		Pad(L.Right, L.Radius, S.Ctx.bRightDown);
		Pad(L.Jump, L.JumpRadius, S.Ctx.bJumpDown);
		Pad(L.Slide, L.Radius, S.Ctx.bSlideDown);
		const bool bAct = ShowAct(S.Game.Sim);
		if (bAct) { Pad(L.Act, L.Radius, S.Ctx.bActDown); }
		const double A = L.Radius * 0.34;
		const FLinearColor G(1, 1, 1, 0.6f);
		S.D.Tri(L.Left.X - A, L.Left.Y, L.Left.X + A * 0.7, L.Left.Y - A, L.Left.X + A * 0.7, L.Left.Y + A, G);
		S.D.Tri(L.Right.X + A, L.Right.Y, L.Right.X - A * 0.7, L.Right.Y - A, L.Right.X - A * 0.7, L.Right.Y + A, G);
		S.Text.Draw(TEXT("JUMP"), L.Jump.X, L.Jump.Y, L.JumpRadius * 0.34, FLinearColor(1, 1, 1, 0.65f), 0.5, 250);
		S.Text.Draw(TEXT("SLIDE"), L.Slide.X, L.Slide.Y, L.Radius * 0.34, FLinearColor(1, 1, 1, 0.65f), 0.5, 200);
		if (bAct)
		{
			const FString Label = ActLabel(S.Game.Sim);
			S.Text.Draw(Label, L.Act.X, L.Act.Y, L.Radius * (Label.Len() > 5 ? 0.27 : 0.34), FLinearColor(1, 1, 1, 0.65f), 0.5, Label.Len() > 5 ? 80 : 200);
		}
	}

	void DrawPlayingHud(FScreenCtx& S)
	{
		FHLGame& G = S.Game;
		const HL::FLevelDef& L = G.CurrentLevel();

		// Level card
		const double T = G.LevelTime;
		const double CardA = SmoothStep(0.3, 1.1, T) * (1.0 - SmoothStep(3.4, 4.6, T));
		if (CardA > 0.001)
		{
			const double Y = S.H * 0.2;
			FHLUI::DrawSerifWord(S.D, Roman(G.LevelIndex + 1), S.W * 0.5, Y, S.H * 0.05, S.H * 0.006, WithAlpha(Ink, CardA));
			S.Text.Draw(UTF8_TO_TCHAR(L.Name.c_str()), S.W * 0.5, Y + S.H * 0.06, S.H * 0.036, WithAlpha(Ink, CardA), 0.5, 300);
			S.Text.Draw(UTF8_TO_TCHAR(L.Subtitle.c_str()), S.W * 0.5, Y + S.H * 0.11, S.H * 0.024, WithAlpha(Ink, CardA * 0.8), 0.5, 150);
		}

		// First-level controls hint
		if (G.LevelIndex == 0 && G.Sim.CheckpointIndex <= 0)
		{
			const double HintA = SmoothStep(4.0, 5.0, T) * (1.0 - SmoothStep(11.0, 12.5, T));
			if (HintA > 0.001)
			{
				S.Text.Draw(ControlsHint(S.Ctx), S.W * 0.5, S.H * 0.12, S.H * 0.024, WithAlpha(Ink, HintA * 0.85), 0.5, 120);
			}
		}

		// Top left: the clock (to the millisecond, for anyone racing) and this level's three embers.
		{
			const double LX = S.SafeL() + S.H * 0.05, LY = S.SafeT() + S.H * 0.075;
			if (G.Save->bTimer)
			{
				S.Text.Draw(FormatClock(G.Sim.PlayTime), LX, LY, S.H * 0.03, WithAlpha(Ink, 0.75), 0.0, 60);
			}
			S.D.SetTransform(1, 0, 0);
			S.D.SetBlend(SE_BLEND_Translucent);
			for (int32 K = 0; K < (int32)G.Sim.EmberTaken.size(); ++K)
			{
				const double EX = LX + S.H * 0.008 + K * S.H * 0.03, EY = LY + S.H * 0.045, ER = S.H * 0.009;
				const bool bGot = G.Sim.EmberTaken[K];
				S.D.Quad(FVector2D(EX, EY - ER), FVector2D(EX + ER * 0.75, EY), FVector2D(EX, EY + ER), FVector2D(EX - ER * 0.75, EY),
					bGot ? WithAlpha(Ember, 0.95) : FLinearColor(1, 1, 1, 0.2f));
			}
		}

		// The script, top right: what this stretch of the level is asking for.
		if (G.NoteAlpha > 0.01 && !G.NoteText.IsEmpty())
		{
			TArray<FString> Lines;
			G.NoteText.ParseIntoArray(Lines, TEXT("\n"));
			const double Px = S.H * 0.026, LH = Px * 1.5;
			const double XR = S.W - S.SafeR() - S.H * 0.045, Y0 = S.SafeT() + S.H * 0.17;
			double MaxW = 0;
			for (const FString& Line : Lines) { MaxW = FMath::Max(MaxW, S.Text.Size(Line, Px, 40).X); }
			const double Rule = FMath::Max(2.0, S.U * 0.8);
			S.D.SetTransform(1, 0, 0);
			S.D.SetBlend(SE_BLEND_Translucent);
			S.D.Rect(XR - MaxW - Px * 0.9, Y0 - LH * 0.8, XR + Px * 0.7, Y0 + LH * (Lines.Num() - 0.2), FLinearColor(0, 0, 0, (float)(0.42 * G.NoteAlpha)));
			S.D.Rect(XR + Px * 0.7 - Rule, Y0 - LH * 0.8, XR + Px * 0.7, Y0 + LH * (Lines.Num() - 0.2), WithAlpha(Ember, 0.8 * G.NoteAlpha));
			for (int32 I = 0; I < Lines.Num(); ++I)
			{
				S.Text.Draw(Lines[I], XR, Y0 + I * LH, Px, WithAlpha(Ink, G.NoteAlpha * 0.95), 1.0, 40);
			}
		}

		// Keyboard: say what E would do now.
		if (!S.Ctx.bShowTouch && ShowAct(G.Sim))
		{
			S.Text.Draw(FString::Printf(TEXT("E  ·  %s"), ActLabel(G.Sim)), S.W - S.SafeR() - S.H * 0.05, S.H - S.SafeB() - S.H * 0.06, S.H * 0.024,
				WithAlpha(Ink, G.Sim.ActKind() == HL::EActKind::Whistle ? 0.45 : 0.85), 1.0, 160);
		}

		// Pause button (top right)
		{
			const double R = S.H * 0.035;
			const FVector2D C(S.W - S.SafeR() - R * 2.0, S.SafeT() + R * 2.0);
			FHLButton B;
			B.Box = FBox2D(C - FVector2D(R * 1.6, R * 1.6), C + FVector2D(R * 1.6, R * 1.6));
			B.Action = EHLAction::Pause;
			G.Buttons.Add(B);
			S.D.SetTransform(1, 0, 0);
			S.D.SetBlend(SE_BLEND_Translucent);
			S.D.Ring(C.X, C.Y, R - FMath::Max(1.2, S.U * 0.5), R, FLinearColor(1, 1, 1, 0.3f), FLinearColor(1, 1, 1, 0.3f), 32);
			S.D.Rect(C.X - R * 0.32, C.Y - R * 0.4, C.X - R * 0.12, C.Y + R * 0.4, FLinearColor(1, 1, 1, 0.45f));
			S.D.Rect(C.X + R * 0.12, C.Y - R * 0.4, C.X + R * 0.32, C.Y + R * 0.4, FLinearColor(1, 1, 1, 0.45f));
		}

		if (S.Ctx.bShowTouch) { DrawTouchControls(S); }
	}
}

// -------------------------------------------------------------------------------------------------

FHLTouchLayout FHLTouchLayout::Compute(double W, double H, const FVector4& Safe)
{
	FHLTouchLayout L;
	L.Radius = H * 0.095;
	L.JumpRadius = H * 0.12;
	const double Y = H - Safe.W - H * 0.16;
	L.Left = FVector2D(Safe.X + H * 0.16, Y);
	L.Right = FVector2D(Safe.X + H * 0.16 + L.Radius * 2.5, Y);
	L.Jump = FVector2D(W - Safe.Z - H * 0.18, Y - H * 0.02);
	L.Slide = FVector2D(L.Jump.X - L.JumpRadius - L.Radius * 1.35, Y + H * 0.035);
	L.Act = FVector2D(L.Jump.X - L.JumpRadius * 0.55 - L.Radius * 0.9, L.Jump.Y - L.JumpRadius - L.Radius * 1.3);
	return L;
}

bool FHLTouchLayout::HitLeft(const FVector2D& P) const
{
	// Generous zones: everything left of the midpoint between the arrows is "left".
	const double Mid = (Left.X + Right.X) * 0.5;
	return P.X < Mid && P.Y > Left.Y - Radius * 2.0 && P.X < Right.X + Radius;
}

bool FHLTouchLayout::HitRight(const FVector2D& P) const
{
	const double Mid = (Left.X + Right.X) * 0.5;
	return P.X >= Mid && P.X < Right.X + Radius * 1.8 && P.Y > Right.Y - Radius * 2.0;
}

int32 FHLTouchLayout::NearestAction(const FVector2D& P) const
{
	// Generous zones; whichever pad is nearest (relative to its size) wins, so they never overlap.
	const double DJ = FVector2D::Distance(P, Jump) / JumpRadius, DS = FVector2D::Distance(P, Slide) / Radius, DA = FVector2D::Distance(P, Act) / Radius;
	if (DJ <= DS && DJ <= DA) { return DJ < 1.7 ? 0 : -1; }
	if (DS <= DA) { return DS < 1.6 ? 1 : -1; }
	return DA < 1.6 ? 2 : -1;
}

bool FHLTouchLayout::HitJump(const FVector2D& P) const { return NearestAction(P) == 0; }
bool FHLTouchLayout::HitSlide(const FVector2D& P) const { return NearestAction(P) == 1; }
bool FHLTouchLayout::HitAct(const FVector2D& P) const { return NearestAction(P) == 2; }

double FHLUI::SerifWordWidth(const FString& Word, double Height, double Tracking)
{
	double W = 0;
	for (int32 I = 0; I < Word.Len(); ++I)
	{
		W += GlyphAdvance(Word[I]) * Height;
		if (I + 1 < Word.Len()) { W += Tracking; }
	}
	return W;
}

void FHLUI::DrawSerifWord(FHLDraw& D, const FString& Word, double CenterX, double BaselineY, double Height, double Tracking, const FLinearColor& Color)
{
	if (Color.A <= 0.001f) { return; }
	D.SetTransform(1, 0, 0);
	D.SetBlend(SE_BLEND_Translucent);
	double X = CenterX - SerifWordWidth(Word, Height, Tracking) * 0.5;
	for (int32 I = 0; I < Word.Len(); ++I)
	{
		DrawGlyph(D, Word[I], X, BaselineY, Height, Color);
		X += GlyphAdvance(Word[I]) * Height + Tracking;
	}
}

void FHLUI::Draw(FHLDraw& D, UCanvas* Canvas, FHLGame& Game, const FHLUiContext& Ctx)
{
	Game.Buttons.Reset();
	FScreenCtx S{ D, Canvas, Game, Ctx, FText2{ D, Canvas, Ctx.Font }, D.ScreenW, D.ScreenH, D.ScreenH / HL::kViewHeight };
	D.SetTransform(1, 0, 0);
	D.SetBlend(SE_BLEND_Translucent);

	switch (Game.Screen)
	{
	case EHLScreen::Title: DrawTitle(S); break;
	case EHLScreen::LevelSelect: DrawLevelSelect(S); break;
	case EHLScreen::Settings: DrawSettings(S); break;
	case EHLScreen::Credits: DrawCredits(S); break;
	case EHLScreen::Playing: DrawPlayingHud(S); break;
	case EHLScreen::Paused: DrawPaused(S); break;
	case EHLScreen::LevelComplete: DrawResults(S, false); break;
	case EHLScreen::Ending: DrawResults(S, true); break;
	case EHLScreen::Calibrate: DrawCalibrate(S); break;
	}
	Game.Focus = FMath::Clamp(Game.Focus, 0, FMath::Max(0, Game.Buttons.Num() - 1));

	// Touch feedback: a faint ring under the finger on menus and the pause button.
	if (Ctx.bMenuTouchDown)
	{
		D.SetTransform(1, 0, 0);
		D.SetBlend(SE_BLEND_Translucent);
		const double R = D.ScreenH * 0.035;
		D.Ring(Ctx.MenuTouch.X, Ctx.MenuTouch.Y, R * 0.8, R, FLinearColor(1, 1, 1, 0.45f), FLinearColor(1, 1, 1, 0.45f), 32);
		D.Circle(Ctx.MenuTouch.X, Ctx.MenuTouch.Y, R * 0.8, FLinearColor(1, 1, 1, 0.08f), 24);
	}
	D.Flush();
}
