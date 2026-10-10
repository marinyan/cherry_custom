/**
 * chysharp.h: Cherry 1.4.3# Patcher
 * written by gocha, feel free to distribute ;)
 */

#ifndef CHYSHARP_H
#define CHYSHARP_H


#include <windows.h>


#ifndef countof
#define countof(a)  (sizeof(a) / sizeof(a[0]))
#endif /* !countof */


typedef struct TagChySharpPrms
{
	BOOL timeSigEx;
	BOOL rpnFix;
	BOOL smfLyrics;
	BOOL smfSwapTimeSig;
	BOOL smfNoNullMarker;
	BOOL dispatchWheel;
	BOOL sfxRememberDur;
	BOOL sfxAllowDurArrange;
	BOOL ccAllowAllChars;
	BOOL ccListCompatible;
	BOOL fixAfterTouch;
	BOOL fixIniAccess;
	BOOL durRemoveBar;
	BOOL durFixZenkaku;
	BOOL useLwrCharExt;
	BOOL standardKeys;
} ChySharpPrms;

BOOL chySharpSaveAs(LPCTSTR filename, ChySharpPrms* prm);


#endif /* !CHYSHARP_H */
