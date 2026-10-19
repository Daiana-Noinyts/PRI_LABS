#include "ForestHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "CanvasItem.h"

void AForestHUD::DrawHUD()
{
	Super::DrawHUD();

	if (CrosshairTex)
	{
		FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
		FVector2D CrossHairDrawPosition(
			Center.X - (CrosshairTex->GetSurfaceWidth() * 0.5f),
			Center.Y - (CrosshairTex->GetSurfaceHeight() * 0.5f));

		FCanvasTileItem TileItem(CrossHairDrawPosition, CrosshairTex->GetResource(), FLinearColor::White);
		TileItem.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(TileItem);
	}
}