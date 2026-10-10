/**
 * chysharp.c: Cherry 1.4.3# Patcher
 * written by gocha, feel free to distribute ;)
 * 汚いソースや電波コメントが苦手な方は見ない方がいいよ・д・
 */

#include <windows.h>
#include <tchar.h>
#include "resource.h"
#include "chysharp.h"
#include <stdlib.h>
#include <commdlg.h>
#include "wheel-image.h"
#include "keys-image.h"

/* Custom extension: keep upstream author and distribution notices above. */
static BOOL MakeSiblingPath(TCHAR *path, DWORD capacity, const TCHAR *name)
{
    DWORD length = GetModuleFileName(NULL, path, capacity);
    TCHAR *slash;
    if (!length || length >= capacity) return FALSE;
    slash = _tcsrchr(path, _T('\\'));
    if (!slash || (size_t)(slash + 1 - path) + _tcslen(name) >= capacity) return FALSE;
    _tcscpy_s(slash + 1, capacity - (size_t)(slash + 1 - path), name);
    return TRUE;
}


#define CHYSHARP_BINFNAME        _T("chysharp.bin")
#define CHYSHARP_BINSIZE         926208
#define CHYSHARP_SETTINGS_BASEID 1002
#define CHYSHARP_SETTINGS_ENUM   16


BYTE* chySharpNewExe = NULL;
BYTE* chySharpTmplExe = NULL;


int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow);


BOOL chySharpInit(void);
void chySharpUninit(void);
int chySharpDlg(HINSTANCE hInstance, int nCmdShow);
LRESULT CALLBACK chySharpDlgProc(HWND hWnd, UINT uMessage, WPARAM wParam, LPARAM lParam);


/* メイン: 雑務はすべて使用人に任せておりますの */
int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	int result = 0;

	if(chySharpInit())
	{
		result = chySharpDlg(hInstance, nCmdShow);
	}
	else
	{
		MessageBox(NULL, _T("初期化に失敗しました。同じフォルダーに対応する chysharp.bin を置いてください。"), NULL, MB_ICONERROR);
	}
	chySharpUninit();
	return result;
}


/* パッチ関係初期化 */
BOOL chySharpInit(void)
{
	BOOL result = FALSE;
	HANDLE chySharpTmplFile;
    TCHAR templatePath[MAX_PATH];
    if (!MakeSiblingPath(templatePath, countof(templatePath), CHYSHARP_BINFNAME)) return FALSE;

	chySharpTmplFile = CreateFile(templatePath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(chySharpTmplFile != INVALID_HANDLE_VALUE)
	{
		DWORD chySharpTmplSize;

		chySharpTmplSize = GetFileSize(chySharpTmplFile, NULL);
		if(chySharpTmplSize == CHYSHARP_BINSIZE)
		{
			chySharpNewExe = (BYTE*) malloc(CHYSHARP_BINSIZE);
			chySharpTmplExe = (BYTE*) malloc(CHYSHARP_BINSIZE);
			if(chySharpNewExe && chySharpTmplExe)
			{
				DWORD readSize;

				ReadFile(chySharpTmplFile, chySharpTmplExe, CHYSHARP_BINSIZE, &readSize, NULL);
				if(readSize == CHYSHARP_BINSIZE && WheelTemplateMatches(chySharpTmplExe, readSize))
				{
					result = TRUE;
				}
			}
		}

		CloseHandle(chySharpTmplFile);
	}
	return result;
}

/* パッチ関係開放 */
void chySharpUninit(void)
{
	if(chySharpNewExe)
		free(chySharpNewExe);
	if(chySharpTmplExe)
		free(chySharpTmplExe);
}

/* ダイアログ作成: ダイアログ様がお亡くなりになるまでご奉仕いたします */
int chySharpDlg(HINSTANCE hInstance, int nCmdShow)
{
	HWND hDlg;
	MSG msg;
	WNDCLASSEX wc;
	LPCTSTR wndClassName = _T("ChySharpPatClass");

	SecureZeroMemory(&wc, sizeof(WNDCLASSEX));
	wc.cbSize        = sizeof(WNDCLASSEX);
	wc.lpszClassName = wndClassName;
	wc.lpfnWndProc   = chySharpDlgProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = DLGWINDOWEXTRA;
	wc.hInstance     = hInstance;
	wc.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CHYSHARP));
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH) (COLOR_BTNFACE + 1);
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	if(!RegisterClassEx(&wc))
	{
		return FALSE;
	}

	hDlg = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_CHYSHARP), 0, (DLGPROC) chySharpDlgProc);
	if(!hDlg)
	{
		return FALSE;
	}
	ShowWindow(hDlg, nCmdShow);
	UpdateWindow(hDlg);

	while(GetMessage(&msg, NULL, 0, 0))
	{
		if(!IsDialogMessage(hDlg, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
}

/* ダイアログプロシージャ: ウィンドウプロシージャを奪われたウィンドウは動く事も話す事も出来ないの */
LRESULT CALLBACK chySharpDlgProc(HWND hWnd, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
	switch(uMessage)
	{
	case WM_INITDIALOG:
		{
			int i;

			for(i = 0; i < CHYSHARP_SETTINGS_ENUM; i++)
			{
				CheckDlgButton(hWnd, i+CHYSHARP_SETTINGS_BASEID, BST_CHECKED);
			}
		}
		break;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDC_SAVE:
			{
				OPENFILENAME ofn;
				TCHAR exeFilename[MAX_PATH] = _T("cherry-custom.exe");
                MakeSiblingPath(exeFilename, countof(exeFilename), _T("cherry-custom.exe"));

				SecureZeroMemory(&ofn, sizeof(OPENFILENAME));
				ofn.lStructSize = sizeof(OPENFILENAME);
				ofn.hwndOwner = hWnd;
				ofn.lpstrFilter = _T("実行ファイル (*.exe)\0*.exe\0");
				ofn.nFilterIndex = 1;
				ofn.lpstrFile = exeFilename;
				ofn.nMaxFile = countof(exeFilename);
				ofn.lpstrDefExt = _T("exe");
				ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
				if(GetSaveFileName(&ofn))
				{
					ChySharpPrms prm;

					prm.timeSigEx          = (IsDlgButtonChecked(hWnd, IDC_TIMESIG) == BST_CHECKED);
					prm.rpnFix             = (IsDlgButtonChecked(hWnd, IDC_RPNFIX) == BST_CHECKED);
					prm.smfLyrics          = (IsDlgButtonChecked(hWnd, IDC_LYRICS) == BST_CHECKED);
					prm.smfSwapTimeSig     = (IsDlgButtonChecked(hWnd, IDC_SWAPTIMESIGORDER) == BST_CHECKED);
					prm.smfNoNullMarker    = (IsDlgButtonChecked(hWnd, IDC_NONULLMARKER) == BST_CHECKED);
					prm.dispatchWheel      = (IsDlgButtonChecked(hWnd, IDC_MOUSEWHEEL) == BST_CHECKED);
					prm.sfxRememberDur     = (IsDlgButtonChecked(hWnd, IDC_REMEMBERDURATION) == BST_CHECKED);
					prm.sfxAllowDurArrange = (IsDlgButtonChecked(hWnd, IDC_DURATIONCHANGE) == BST_CHECKED);
					prm.ccAllowAllChars    = (IsDlgButtonChecked(hWnd, IDC_ACCEPTEXCEPTNUM) == BST_CHECKED);
					prm.ccListCompatible   = (IsDlgButtonChecked(hWnd, IDC_EVENTLISTCOMPATIBLE) == BST_CHECKED);
					prm.fixAfterTouch      = (IsDlgButtonChecked(hWnd, IDC_PATFIX) == BST_CHECKED);
					prm.fixIniAccess       = (IsDlgButtonChecked(hWnd, IDC_INIFIX) == BST_CHECKED);
					prm.durRemoveBar       = (IsDlgButtonChecked(hWnd, IDC_DURREMOVESCROLLBAR) == BST_CHECKED);
					prm.durFixZenkaku      = (IsDlgButtonChecked(hWnd, IDC_DURFIXZENKAKU) == BST_CHECKED);
					prm.useLwrCharExt      = (IsDlgButtonChecked(hWnd, IDC_LOWEREXT) == BST_CHECKED);
					prm.standardKeys       = (IsDlgButtonChecked(hWnd, IDC_STANDARDKEYS) == BST_CHECKED);

					if (chySharpSaveAs(exeFilename, &prm))
                        MessageBox(hWnd, _T("保存しました。作成した実行ファイルを起動してください。"), _T("ChySharp Custom"), MB_OK | MB_ICONINFORMATION);
                    else
                        MessageBox(hWnd, _T("保存できませんでした。既存ファイルは上書きしません。別のファイル名と書き込み可能な場所を指定してください。"), _T("ChySharp Custom"), MB_OK | MB_ICONERROR);
				}
				break;
			}

		default:
			return DefWindowProc(hWnd, uMessage, wParam, lParam);
		}
		break;

	case WM_CLOSE:
		DestroyWindow(hWnd);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, uMessage, wParam, lParam);
	}
	return 0;
}

/* ここにたどり着くまでに一日かかった: バカばっか */
BOOL chySharpSaveAs(LPCTSTR filename, ChySharpPrms* prm)
{
	BOOL result = FALSE;
	HANDLE newExeFile;

	/* 完全改造後のバージョンをコピー、必要に応じて逆改造する */
	memcpy(chySharpNewExe, chySharpTmplExe, CHYSHARP_BINSIZE);

	if(!prm->timeSigEx)
	{
		/* 逆修正: 拍子の分母を編集可能にする */
		BYTE codeGetTimeSig[] = {
			0x8b, 0x41, 0x0c, 0xff, 0x50, 0x78
		};
		BYTE codeSetTimeSig[] = {
			0x68, 0x00, 0x01, 0x00, 0x00, 0x8d, 0x85, 0x00, 0xff, 0xff, 0xff, 0x50, 0x8b, 0x53, 0x1d, 0x52,
			0xe8, 0x1d, 0x4c, 0x07, 0x00, 0x83, 0xc4, 0x0c, 0x8d, 0x8d, 0x00, 0xff, 0xff, 0xff, 0x51, 0xe8,
			0xf3, 0x87, 0x08, 0x00, 0x8b, 0x53, 0x2d, 0x59, 0x88, 0x42, 0x02
		};

		/* 逆修正: 拍子データの表示方法 */
		memcpy(&chySharpNewExe[0x0000e68a], codeGetTimeSig, sizeof(codeGetTimeSig));
		/* 逆修正: 拍子データの変更方法 */
		memcpy(&chySharpNewExe[0x0000e789], codeSetTimeSig, sizeof(codeSetTimeSig));
		/* 逆修正: リスト/ドロップダウン */
		chySharpNewExe[0x000d1b78] = 0x03;
	}
	if(!prm->rpnFix)
	{
		/* 逆修正: NRPN/RPNをmidiOutShortMsgで送信する */
		BYTE codeInterceptLongMsgFunc[] = {
			0xff, 0x25, 0x3c, 0xac, 0x4c, 0x00
		};

		/* 逆修正: midiOutLongMsg関数のエントリを別の関数に修正 */
		memcpy(&chySharpNewExe[0x0009d99a], codeInterceptLongMsgFunc, sizeof(codeInterceptLongMsgFunc));
	}
	if(!prm->smfLyrics)
	{
		/* 逆修正: SMF出力時に歌詞イベントを出力する */
		BYTE codeOutputLyrics[] = {
			0x8b, 0x10, 0xff, 0x52, 0x18
		};

		/* 逆修正: 歌詞出力を行う関数を呼び出す */
		memcpy(&chySharpNewExe[0x00026f9b], codeOutputLyrics, sizeof(codeOutputLyrics));
	}
	if(!prm->smfSwapTimeSig)
	{
		/* 逆修正: SMF出力時に拍子・調号イベントの順を反転する */
		BYTE codeSwapTimeSigOrder[] = {
			0x8b, 0x45, 0x14, 0x8b, 0x7d, 0x0c, 0xa8, 0x10, 0x8b, 0xc8, 0x0f, 0x94, 0xc2, 0x83, 0xe1, 0x08,
			0x8b, 0x75, 0x08, 0x8b, 0x5d, 0x10, 0x89, 0x4d, 0xfc, 0x83, 0xe2, 0x01, 0x83, 0xe0, 0x07, 0x83,
			0x7d, 0xfc, 0x00, 0x74, 0x07, 0xf6, 0xd8, 0x88, 0x45, 0xdc, 0xeb, 0x03, 0x88, 0x45, 0xdc, 0x33,
			0xc0, 0x85, 0xd2, 0x75, 0x01, 0x40, 0x88, 0x45, 0xdd, 0x6a, 0x02, 0x8d, 0x55, 0xdc, 0x52, 0x6a,
			0x59, 0x6a, 0x00, 0x56, 0x8b, 0x0e, 0xff, 0x51, 0x1c, 0x83, 0xc4, 0x14, 0x8b, 0xc7, 0x88, 0x45,
			0xdc, 0x33, 0xc0, 0x8b, 0xc8, 0xba, 0x01, 0x00, 0x00, 0x00, 0xd3, 0xe2, 0x3b, 0xda, 0x75, 0x03,
			0x88, 0x45, 0xdd, 0x40, 0x83, 0xf8, 0x10, 0x7c, 0xea, 0xc6, 0x45, 0xde, 0x18, 0xc6, 0x45, 0xdf,
			0x08, 0x6a, 0x04, 0x8d, 0x45, 0xdc, 0x50, 0x6a, 0x58, 0x6a, 0x00, 0x56, 0x8b, 0x16, 0xff, 0x52
		};

		/* 逆修正: 拍子・調号イベントの出力順を反転する */
		memcpy(&chySharpNewExe[0x00005e41], codeSwapTimeSigOrder, sizeof(codeSwapTimeSigOrder));
	}
	if(!prm->smfNoNullMarker)
	{
		/* 逆修正: SMF出力時に空白マーカを出力しないようにする */
		BYTE codeNoNullMarker[] = {
			0x50, 0x57, 0x6a, 0x06, 0x6a, 0x00, 0x8b, 0x43, 0x5d, 0x50, 0x8b, 0x10, 0xff, 0x52, 0x1c, 0x83,
			0xc4, 0x14, 0x6a, 0x18, 0x33, 0xc9, 0x8a, 0x8b, 0xdd, 0x93, 0x03, 0x00, 0x51, 0x33, 0xc0, 0x8a,
			0x83, 0xdc, 0x93, 0x03, 0x00, 0x50, 0x33, 0xd2, 0x8a, 0x93, 0xdb, 0x93, 0x03, 0x00
		};

		/* 逆修正: 空白マーカを無視する */
		memcpy(&chySharpNewExe[0x0002824d], codeNoNullMarker, sizeof(codeNoNullMarker));
	}
	if(!prm->dispatchWheel)
	{
		/* 逆修正: マウスホイール操作を有効にする */
		BYTE codeInjectAnotherEP[] = {
			0x5c, 0xbd, 0x48, 0x00
		};


		/* 逆修正: OEPの類を書き換える */
		memcpy(&chySharpNewExe[0x0009db94], codeInjectAnotherEP, sizeof(codeInjectAnotherEP));
	}
	if(!prm->sfxRememberDur)
	{
		/* 逆修正: Rhythm/SFXトラックにおいてデュレーションを記憶する */
		chySharpNewExe[0x00024e56] = 0x04;
		chySharpNewExe[0x00024e59] = 0x75;
	}
	if(!prm->sfxAllowDurArrange)
	{
		/* 逆修正: Rhythm/SFXトラックにおいてデュレーションをピアノロールから変更可能にする */
		chySharpNewExe[0x00024cb4] = 0x20;
		chySharpNewExe[0x00024cb8] = 0x1c;
	}
	if(!prm->ccAllowAllChars)
	{
		/* 逆修正: CC入力ダイアログにおいて数字以外の入力を可能にする */
		chySharpNewExe[0x000cfc05] = 0x20;
	}
	if(!prm->ccListCompatible)
	{
		/* 逆修正: CC入力ダイアログにおける値の解釈をイベントリスト互換にする */
		BYTE codeToStrtol1[] = {
			0xb9, 0xb3, 0x08, 0x00
		};
		BYTE codeToStrtol2[] = {
			0x90, 0xb3, 0x08, 0x00
		};

		/* 逆修正: 読み取り時解釈変更 */
		chySharpNewExe[0x0000b8d5] = 0x0d;
		/* 逆修正: 書き込み時解釈変更 */
		chySharpNewExe[0x0000bbea] = 0x0d;
		memcpy(&chySharpNewExe[0x0000bbe3], codeToStrtol1, sizeof(codeToStrtol1));
		memcpy(&chySharpNewExe[0x0000bc0c], codeToStrtol2, sizeof(codeToStrtol2));
	}
	if(!prm->fixAfterTouch)
	{
		/* 逆修正: リアルタイム入力におけるアフタータッチの番号を修正する */
		chySharpNewExe[0x00000e2c] = 0x96;
	}
	if(!prm->fixIniAccess)
	{
		/* 逆修正: cherry.iniにおいてControlChangeキーの代わりにControlキーを読むようにする */
		BYTE codeToRefControl[] = {
			0x7f, 0x2a, 0x4b, 0x00
		};

		/* 逆修正: 参照キー名を変更 */
		memcpy(&chySharpNewExe[0x0004f664], codeToRefControl, sizeof(codeToRefControl));
	}
	if(!prm->durRemoveBar)
	{
		/* 逆修正: デュレーションリストを長くしてスクロールバーを取り除く */
		chySharpNewExe[0x0001ac76] = 0xc8;
	}
	if(!prm->durFixZenkaku)
	{
		/* 逆修正: デュレーションリストの全角文字を半角文字に修正する */
		BYTE codeZenToHan[] = {
			0x82, 0x53, 0x95, 0xaa, 0x89, 0xb9, 0x95, 0x84
		};

		/* 逆修正: 全角→半角修正 */
		memcpy(&chySharpNewExe[0x000b05bb], codeZenToHan, sizeof(codeZenToHan));
	}
	if(!prm->useLwrCharExt)
	{
		/* 逆修正: ファイル保存時の拡張子補完を小文字にする */
		BYTE codeEsr[] = { 'E', 'S', 'R' };
		BYTE codeChy[] = { 'C', 'H', 'Y' };
		BYTE codeMid[] = { 'M', 'I', 'D' };
		BYTE codeSmf[] = { 'S', 'M', 'F' };
		BYTE codeRmi[] = { 'R', 'M', 'I' };
		BYTE codeRcp[] = { 'R', 'C', 'P' };
		BYTE codeR36[] = { 'R', '3', '6' };
		BYTE codeG18[] = { 'G', '1', '8' };
		BYTE codeG36[] = { 'G', '3', '6' };
		BYTE codeMff[] = { 'M', 'F', 'F' };
		BYTE codeCpg[] = { 'C', 'P', 'G' };

		memcpy(&chySharpNewExe[0x000ab40d], codeEsr, sizeof(codeEsr));
		memcpy(&chySharpNewExe[0x000ab43c], codeChy, sizeof(codeChy));
		memcpy(&chySharpNewExe[0x000ab442], codeMid, sizeof(codeMid));
		memcpy(&chySharpNewExe[0x000ab448], codeSmf, sizeof(codeSmf));
		memcpy(&chySharpNewExe[0x000ab44e], codeRmi, sizeof(codeRmi));
		memcpy(&chySharpNewExe[0x000ab454], codeRcp, sizeof(codeRcp));
		memcpy(&chySharpNewExe[0x000ab45a], codeR36, sizeof(codeR36));
		memcpy(&chySharpNewExe[0x000ab460], codeG18, sizeof(codeG18));
		memcpy(&chySharpNewExe[0x000ab466], codeG36, sizeof(codeG36));

		memcpy(&chySharpNewExe[0x000ab48f], codeChy, sizeof(codeChy));
		memcpy(&chySharpNewExe[0x000ab4ba], codeMid, sizeof(codeMid));
		memcpy(&chySharpNewExe[0x000ab4c0], codeSmf, sizeof(codeSmf));
		memcpy(&chySharpNewExe[0x000ab4c6], codeRmi, sizeof(codeRmi));
		memcpy(&chySharpNewExe[0x000ab4fa], codeRcp, sizeof(codeRcp));
		memcpy(&chySharpNewExe[0x000ab500], codeR36, sizeof(codeR36));
		memcpy(&chySharpNewExe[0x000ab506], codeG18, sizeof(codeG18));
		memcpy(&chySharpNewExe[0x000ab50c], codeG36, sizeof(codeG36));

		memcpy(&chySharpNewExe[0x000ab5c7], codeChy, sizeof(codeChy));
		memcpy(&chySharpNewExe[0x000ab5cd], codeMid, sizeof(codeMid));
		memcpy(&chySharpNewExe[0x000ab5d3], codeSmf, sizeof(codeSmf));
		memcpy(&chySharpNewExe[0x000ab5d9], codeRmi, sizeof(codeRmi));
		memcpy(&chySharpNewExe[0x000ab5df], codeRcp, sizeof(codeRcp));
		memcpy(&chySharpNewExe[0x000ab5e5], codeR36, sizeof(codeR36));
		memcpy(&chySharpNewExe[0x000ab5eb], codeG18, sizeof(codeG18));
		memcpy(&chySharpNewExe[0x000ab5f1], codeG36, sizeof(codeG36));

		memcpy(&chySharpNewExe[0x000ab61a], codeChy, sizeof(codeChy));
		memcpy(&chySharpNewExe[0x000ab645], codeMid, sizeof(codeMid));
		memcpy(&chySharpNewExe[0x000ab64b], codeSmf, sizeof(codeSmf));
		memcpy(&chySharpNewExe[0x000ab651], codeRmi, sizeof(codeRmi));
		memcpy(&chySharpNewExe[0x000ab685], codeRcp, sizeof(codeRcp));
		memcpy(&chySharpNewExe[0x000ab68b], codeR36, sizeof(codeR36));
		memcpy(&chySharpNewExe[0x000ab691], codeG18, sizeof(codeG18));
		memcpy(&chySharpNewExe[0x000ab697], codeG36, sizeof(codeG36));

		memcpy(&chySharpNewExe[0x000ab6ff], codeMid, sizeof(codeMid));
		memcpy(&chySharpNewExe[0x000ab705], codeSmf, sizeof(codeSmf));
		memcpy(&chySharpNewExe[0x000ab70b], codeRmi, sizeof(codeRmi));
		memcpy(&chySharpNewExe[0x000ab711], codeMff, sizeof(codeMff));

		memcpy(&chySharpNewExe[0x000ab778], codeMid, sizeof(codeMid));
		memcpy(&chySharpNewExe[0x000ab77e], codeSmf, sizeof(codeSmf));
		memcpy(&chySharpNewExe[0x000ab784], codeRmi, sizeof(codeRmi));
		memcpy(&chySharpNewExe[0x000ab78a], codeMff, sizeof(codeMff));

		memcpy(&chySharpNewExe[0x000ab7d8], codeChy, sizeof(codeChy));

		memcpy(&chySharpNewExe[0x000b032f], codeCpg, sizeof(codeCpg));
	}

	/* Add the modern wheel code AFTER applying all selected upstream options. */
    {
        BYTE *output = chySharpNewExe;
        DWORD outputSize = CHYSHARP_BINSIZE;
        DWORD writtenSize = 0;
        DWORD failure = ERROR_SUCCESS;
        if (prm->dispatchWheel && !WheelCreateImage(chySharpNewExe, CHYSHARP_BINSIZE, &output, &outputSize))
            return FALSE;
        if (prm->standardKeys) {
            BYTE *withKeys = NULL;
            DWORD withKeysSize = 0;
            BOOL keysOK = KeysCreateImage(output, outputSize, &withKeys, &withKeysSize);
            if (output != chySharpNewExe) free(output);
            if (!keysOK) return FALSE;
            output = withKeys;
            outputSize = withKeysSize;
        }
        newExeFile = CreateFile(filename, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (newExeFile != INVALID_HANDLE_VALUE) {
            result = WriteFile(newExeFile, output, outputSize, &writtenSize, NULL) &&
                writtenSize == outputSize && FlushFileBuffers(newExeFile);
            if (!result) failure = GetLastError();
            CloseHandle(newExeFile);
            if (!result) DeleteFile(filename); /* Only our just-created incomplete output. */
        } else failure = GetLastError();
        if (output != chySharpNewExe) free(output);
        if (!result) SetLastError(failure);
    }
    return result;
}
