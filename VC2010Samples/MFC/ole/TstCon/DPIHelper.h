#pragma once

#define GDIPVER 0x0110
#include "Gdiplus.h"
#pragma comment(lib, "Gdiplus.lib")

class C_AutoGdiplusStartup
{
public:
	C_AutoGdiplusStartup()
	{
		Gdiplus::GdiplusStartupInput gdiplusStartupInput;
		Gdiplus::GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);
	}
	virtual ~C_AutoGdiplusStartup()
	{
		Gdiplus::GdiplusShutdown(m_gdiplusToken);
	}
protected:
	ULONG_PTR m_gdiplusToken;
};

//---------------- _CDPI ------------ 
// Definition: relative pixel = 1 pixel at 96 DPI and scaled based on actual DPI.
// On Windows dpiX and dpiY are always equal
#ifndef USER_DEFAULT_SCREEN_DPI
#define USER_DEFAULT_SCREEN_DPI 96
#endif

class _CDPI
{
public:
	_CDPI() : _fInitialized(false), _dpi(USER_DEFAULT_SCREEN_DPI) { }

	// Get screen DPI.
	int GetDPI() { _Init(); return _dpi; }

	// Convert between raw pixels and relative pixels.
	int Scale(int x) { _Init(); return ::MulDiv(x, _dpi, USER_DEFAULT_SCREEN_DPI); }
	float ScaleF(float x) { _Init(); return x * float(_dpi) / float(USER_DEFAULT_SCREEN_DPI); }
	int Unscale(int x) { _Init(); return ::MulDiv(x, USER_DEFAULT_SCREEN_DPI, _dpi); }
	float UnscaleF(float x) { _Init(); return x * float(USER_DEFAULT_SCREEN_DPI) / float(_dpi); }

	// Determine the screen dimensions in relative pixels.
	int ScaledScreenWidth() { return _ScaledSystemMetric(SM_CXSCREEN); }
	int ScaledScreenHeight() { return _ScaledSystemMetric(SM_CYSCREEN); }

	// Scale rectangle from raw pixels to relative pixels.
	void ScaleRect(__inout RECT *pRect)
	{
		pRect->left = Scale(pRect->left);
		pRect->right = Scale(pRect->right);
		pRect->top = Scale(pRect->top);
		pRect->bottom = Scale(pRect->bottom);
	}
	void UnscaleRect(__inout RECT *pRect)
	{
		pRect->left = Unscale(pRect->left);
		pRect->right = Unscale(pRect->right);
		pRect->top = Unscale(pRect->top);
		pRect->bottom = Unscale(pRect->bottom);
	}

	// Scale SIZE from raw pixels to relative pixels.
	void ScaleSize(__inout SIZE *pSize)
	{
		pSize->cx = Scale(pSize->cx);
		pSize->cy = Scale(pSize->cy);
	}
	void UnscaleSize(__inout SIZE *pSize)
	{
		pSize->cx = Unscale(pSize->cx);
		pSize->cy = Unscale(pSize->cy);
	}

	// Determine if screen resolution meets minimum requirements in relative
	// pixels.
	bool IsResolutionAtLeast(int cxMin, int cyMin)
	{
		return (ScaledScreenWidth() >= cxMin) && (ScaledScreenHeight() >= cyMin);
	}

	// Convert a point size (1/72 of an inch) to raw pixels.
	int PointsToPixels(int pt) { _Init(); return ::MulDiv(pt, _dpi, 72); }

	// Invalidate any cached metrics.
	void Invalidate() { _fInitialized = false; }

	bool NeedScaling() { _Init(); return (_dpi != USER_DEFAULT_SCREEN_DPI); }

	HICON ScaleIcon(HICON hIcon)
	{
		ICONINFO info = { 0 };
		if(!::GetIconInfo(hIcon, &info)) { ATLASSERT(0); return NULL; }

		BITMAP bmp = { 0 };
		if(::GetObject(info.hbmColor, sizeof(BITMAP), (LPVOID)&bmp) != sizeof(bmp)) { ATLASSERT(0); return NULL; }

		HDC hDC = ::CreateCompatibleDC(NULL);

		BITMAPINFO bi;
		ZeroMemory(&bi, sizeof(BITMAPINFO));
		bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bi.bmiHeader.biWidth = bmp.bmWidth;
		bi.bmiHeader.biHeight = bmp.bmHeight;
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biBitCount = 32;
		bi.bmiHeader.biCompression = BI_RGB;
		void *pBits;
		HBITMAP hBitmap = ::CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &pBits, NULL, 0);

		HBITMAP hbmpOld = (HBITMAP)::SelectObject(hDC, hBitmap);

		HIMAGELIST hilOrig = ImageList_Create(bmp.bmWidth, bmp.bmHeight, ILC_COLOR32 | ILC_MASK, 0, 1);
		ImageList_AddIcon(hilOrig, hIcon);

		ImageList_Draw(hilOrig, 0, hDC, 0, 0, ILD_TRANSPARENT);

		::ImageList_Destroy(hilOrig);

		::SelectObject(hDC, hbmpOld);

		::DeleteDC(hDC);

		HBITMAP hScaledBitmap = ScaleBitmap(hBitmap);

		BITMAP bmpScaled;
		::GetObject(hScaledBitmap, sizeof(bmpScaled), &bmpScaled);
		HIMAGELIST hil = ImageList_Create(bmpScaled.bmWidth, bmpScaled.bmHeight, ILC_COLOR32 | ILC_MASK, 0, 1);
		if(hil == NULL) { ATLASSERT(0); return NULL; }
		ImageList_Add(hil, hScaledBitmap, NULL);
		HICON hScaledIcon = ::ImageList_GetIcon(hil, 0, ILD_PRESERVEALPHA);
		::DeleteObject(hScaledBitmap);
		::DeleteObject(hBitmap);
		::ImageList_Destroy(hil);

		if(info.hbmMask != NULL)
			::DeleteObject(info.hbmMask);
		if(info.hbmColor != NULL)
			::DeleteObject(info.hbmColor);

		return hScaledIcon;
	}

	HBITMAP ScaleBitmap(HBITMAP hbmpOrg)
	{
		_Init();
		return ScaleBitmap(hbmpOrg, USER_DEFAULT_SCREEN_DPI, _dpi);
	}

	static HBITMAP ScaleBitmap(HBITMAP hbmpOrg, int scaleFrom, int scaleTo)
	{
		BITMAP bmp;
		if(::GetObject(hbmpOrg, sizeof(bmp), &bmp) != sizeof(bmp)) return NULL;

		BITMAPINFOHEADER bih; memset(&bih, 0, sizeof(bih));
		void *pBits;
		bih.biSize = sizeof(bih);
		bih.biWidth = ::MulDiv(bmp.bmWidth, scaleTo, scaleFrom);
		bih.biHeight = ::MulDiv(bmp.bmHeight, scaleTo, scaleFrom);
		bih.biPlanes = 1;
		bih.biBitCount = 32; //(bmp.bmBitsPixel == 32) ? 32 : 24;
		bih.biCompression = BI_RGB;
		bih.biSizeImage = 0;
		HBITMAP hbmpRet = ::CreateDIBSection(NULL, (BITMAPINFO*)&bih, DIB_RGB_COLORS, &pBits, NULL, 0);
		if(hbmpRet == NULL) return NULL;

		HDC hDC = ::GetWindowDC(NULL);
		HDC hMemDC = ::CreateCompatibleDC(hDC);
		HDC hOrgDC = ::CreateCompatibleDC(hDC);

		HBITMAP hbmpOld = (HBITMAP)::SelectObject(hMemDC, hbmpRet);
		HBITMAP hbmpOldOrg = (HBITMAP)::SelectObject(hOrgDC, hbmpOrg);

		if(scaleTo == scaleFrom) {
			::BitBlt(hMemDC, 0, 0, bmp.bmWidth, bmp.bmHeight, hOrgDC, 0, 0, SRCCOPY);
		}
		else {
			// More ore less based on .NET DpiHelper.cs (mostly comments...)

			// We will prefer NearestNeighbor algorithm for 200, 300, 400, etc zoom factors, in which each pixel become a 2x2, 3x3, 4x4, etc rectangle. 
			// This produces sharp edges in the scaled image and doesn't cause distortions of the original image.
			// For any other scale factors we will prefer a high quality resizing algorithm. While that introduces fuzziness in the resulting image, 
			// it will not distort the original (which is extremely important for small zoom factors like 125%, 150%).
			// We'll use Bicubic in those cases, except on reducing (zoom < 100, which we shouldn't have anyway), in which case Linear produces better 
			// results because it uses less neighboring pixels.

			// Ok, decide to always use HighQualityBicubic

			Gdiplus::InterpolationMode interpolationMode = Gdiplus::InterpolationModeHighQualityBicubic;
			if ((scaleTo % scaleFrom) == 0) 
				interpolationMode = Gdiplus::InterpolationModeNearestNeighbor;
			else 
			if(scaleTo < scaleFrom)
				interpolationMode = Gdiplus::InterpolationModeHighQualityBilinear;
			else
				interpolationMode = Gdiplus::InterpolationModeHighQualityBicubic;

			//// fill background with white, otherwise DrawState with DSS_MONO does not work
			//RECT rc = { 0, 0, bih.biWidth, bih.biHeight };
			//::FillRect(hMemDC, &rc, (HBRUSH)::GetStockObject(WHITE_BRUSH));

			Gdiplus::Graphics graphics(hMemDC);

			Gdiplus::Bitmap *bitmap = Gdiplus::Bitmap::FromHBITMAP(hbmpOrg, NULL);

			graphics.SetInterpolationMode(interpolationMode);
			graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias8x8 /*Gdiplus::SmoothingModeNone*/);

			//// Sharpen, seems not look good and don't produce proper alpha channel
			//Gdiplus::SharpenParams sharenParams;
			//sharenParams.amount = 50.f;
			//sharenParams.radius = 0.5f;
			//Gdiplus::Sharpen sharpen;
			//sharpen.SetParameters(&sharenParams);
			//Gdiplus::REAL scale = Gdiplus::REAL(scaleTo) / Gdiplus::REAL(scaleFrom);
			//Gdiplus::Matrix matrix(scale, 0.0f, 0.0f, scale, 0.0f, 0.0f);
			////graphics.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);

			Gdiplus::RectF sourceRect(0, 0, (Gdiplus::REAL)bmp.bmWidth, (Gdiplus::REAL)bmp.bmHeight);
			Gdiplus::RectF destRect(0, 0, (Gdiplus::REAL)bih.biWidth, (Gdiplus::REAL)bih.biHeight);

			// Specify a source rectangle shifted by half of pixel to account for GDI+ considering the source origin the center of top-left pixel
			// Failing to do so will result in the right and bottom of the bitmap lines being interpolated with the graphics' background color,
			// and will appear black even if we cleared the background with transparent color. 
			// The apparition of these artifacts depends on the interpolation mode, on the dpi scaling factor, etc.
			// E.g. at 150% DPI, Bicubic produces them and NearestNeighbor is fine, but at 200% DPI NearestNeighbor also shows them.
			//
			// It seems that we have no such issues here but anyway...
			//sourceRect.Offset(-0.5f, -0.5f);

			Gdiplus::ImageAttributes imageAttributes;
			imageAttributes.SetWrapMode(Gdiplus::WrapModeTileFlipXY);	// eliminate black border on edges

			//Gdiplus::PixelFormat format = bitmap->GetPixelFormat();
			//if(format == PixelFormat32bppRGB) {
			UINT pixel = Gdiplus::GetPixelFormatSize(bitmap->GetPixelFormat());
			if(pixel == 32) {
				Gdiplus::BitmapData bmData;
				Gdiplus::Rect bmBounds(0, 0, bitmap->GetWidth(), bitmap->GetHeight());

				bitmap->LockBits(&bmBounds, Gdiplus::ImageLockModeRead, bitmap->GetPixelFormat(), &bmData);
				{
					Gdiplus::Bitmap bitmapARGB(bmData.Width, bmData.Height, bmData.Stride, PixelFormat32bppARGB, (BYTE*)(bmData.Scan0));

					//graphics.DrawImage(&bitmapARGB, &sourceRect, &matrix, &sharpen, NULL, Gdiplus::UnitPixel); //GDIPVER >= 0x0110, sharpen

					graphics.DrawImage(&bitmapARGB, destRect, sourceRect, Gdiplus::UnitPixel, &imageAttributes); //GDIPVER >= 0x0110
					//graphics.DrawImage(bitmap, destRect, sourceRect.X, sourceRect.Y, sourceRect.Width, sourceRect.Height, Gdiplus::UnitPixel, &imgAtt);
				}
				bitmap->UnlockBits(&bmData);
			}
			else {
				Gdiplus::Color colourOfTopLeftPixel;
				bitmap->GetPixel(0, 0, &colourOfTopLeftPixel);
				Gdiplus::ImageAttributes imageAttributes;
				imageAttributes.SetColorKey(colourOfTopLeftPixel, colourOfTopLeftPixel, Gdiplus::ColorAdjustTypeBitmap);

				//graphics.DrawImage(bitmap, &sourceRect, &matrix, &sharpen, &imageAttributes, Gdiplus::UnitPixel); //GDIPVER >= 0x0110, sharpen

				graphics.DrawImage(bitmap, destRect, sourceRect, Gdiplus::UnitPixel, &imageAttributes); //GDIPVER >= 0x0110
				//graphics.DrawImage(bitmap, destRect, sourceRect.X, sourceRect.Y, sourceRect.Width, sourceRect.Height, Gdiplus::UnitPixel, &imgAtt);
			}

			delete bitmap;
		}

		::SelectObject(hOrgDC, hbmpOldOrg);
		::SelectObject(hMemDC, hbmpOld);

		::DeleteDC(hMemDC);
		::DeleteDC(hOrgDC);
		::ReleaseDC(NULL, hDC);

		return hbmpRet;
	}

	HBITMAP LoadAndScaleBitmap(HINSTANCE hInst, LPCTSTR name)
	{
		HBITMAP hBmp = (HBITMAP)::LoadImage(hInst, name, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
		if(hBmp == NULL)
			return NULL;
		_Init();
		if(USER_DEFAULT_SCREEN_DPI == _dpi)
			return hBmp;
		HBITMAP hBmpScaled = ScaleBitmap(hBmp, USER_DEFAULT_SCREEN_DPI, _dpi);
		::DeleteObject(hBmp);
		return hBmpScaled;
	}

	HBITMAP LoadAndScaleBitmap2x(HINSTANCE hInst, LPCTSTR name)
	{
		HBITMAP hBmp = (HBITMAP)::LoadImage(hInst, name, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
		if(hBmp == NULL)
			return NULL;
		_Init();
		if(USER_DEFAULT_SCREEN_DPI * 2 == _dpi)
			return hBmp;
		HBITMAP hBmpScaled = ScaleBitmap(hBmp, USER_DEFAULT_SCREEN_DPI * 2, _dpi);
		::DeleteObject(hBmp);
		return hBmpScaled;
	}

	void ScaleFont(HFONT &hFont)
	{
		_Init();
		if(USER_DEFAULT_SCREEN_DPI != _dpi) {
			LOGFONT lf = { 0 };
			::GetObject(hFont, sizeof(lf), &lf);
			lf.lfHeight = Scale(lf.lfHeight);
			::DeleteObject(hFont);
			hFont = ::CreateFontIndirect(&lf);
		}
	}

private:
	void _Init()
	{
		if(!_fInitialized)
		{
			HDC hdc = ::GetDC(NULL);
			if(hdc)
			{
				_dpi = ::GetDeviceCaps(hdc, LOGPIXELSX);
				::ReleaseDC(NULL, hdc);
			}
			_fInitialized = true;
		}
	}

	int _ScaledSystemMetric(int nIndex)
	{
		_Init();
		return ::MulDiv(::GetSystemMetrics(nIndex), 96, USER_DEFAULT_SCREEN_DPI);
	}

private:
	bool _fInitialized;

	int _dpi;
};
