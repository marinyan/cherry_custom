param([string]$Archive = (Join-Path $PSScriptRoot '../../cherry_1/src.rar'))
$ErrorActionPreference = 'Stop'
$archivePath = (Resolve-Path -LiteralPath $Archive).Path
$hasher = [Security.Cryptography.SHA256]::Create()
try { $archiveHash = [BitConverter]::ToString($hasher.ComputeHash([IO.File]::ReadAllBytes($archivePath))).Replace('-', '') }
finally { $hasher.Dispose() }
if ($archiveHash -ne '55C4FD47224A6949F3589986E288B1914B5DDC297BA11F0B4ACC400D011D640D') {
    throw 'Unsupported src.rar. See README for the supported archive hash.'
}
$build = Join-Path $PSScriptRoot 'build'
$upstream = Join-Path $build 'upstream'
[void][IO.Directory]::CreateDirectory($upstream)
& tar -xf $archivePath -C $upstream
if ($LASTEXITCODE -ne 0) { throw 'Source extraction failed.' }
function ReplaceOnce($text, $before, $after) {
    if ($text.IndexOf($before) -lt 0 -or $text.IndexOf($before) -ne $text.LastIndexOf($before)) {
        throw "Source anchor missing or ambiguous: $before"
    }
    return $text.Replace($before, $after)
}
$source = [IO.File]::ReadAllText((Join-Path $upstream 'chysharp.c')).Replace("`r`n", "`n")
$source = ReplaceOnce $source '#include "chysharp.h"' @'
#include "chysharp.h"
#include <stdlib.h>
#include <commdlg.h>
#include "wheel-image.h"

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
'@
# PowerShell literal above uses two backslashes for the C character literal.
$source = ReplaceOnce $source 'HANDLE chySharpTmplFile;' @'
HANDLE chySharpTmplFile;
    TCHAR templatePath[MAX_PATH];
    if (!MakeSiblingPath(templatePath, countof(templatePath), CHYSHARP_BINFNAME)) return FALSE;
'@
$source = ReplaceOnce $source 'CreateFile(CHYSHARP_BINFNAME,' 'CreateFile(templatePath,'
$source = ReplaceOnce $source 'if(readSize == CHYSHARP_BINSIZE)' 'if(readSize == CHYSHARP_BINSIZE && WheelTemplateMatches(chySharpTmplExe, readSize))'
$source = ReplaceOnce $source '_T("初期化に失敗しました")' '_T("初期化に失敗しました。同じフォルダーに対応する chysharp.bin を置いてください。")'
$source = ReplaceOnce $source '0, chySharpDlgProc);' '0, (DLGPROC) chySharpDlgProc);'
$source = ReplaceOnce $source 'MAKEINTRESOURCE(IDC_ARROW)' 'IDC_ARROW'
$source = ReplaceOnce $source 'TCHAR exeFilename[MAX_PATH] = _T("cherry.exe");' @'
TCHAR exeFilename[MAX_PATH] = _T("cherry-custom.exe");
                MakeSiblingPath(exeFilename, countof(exeFilename), _T("cherry-custom.exe"));
'@
$source = ReplaceOnce $source '_T("cherry.exe\0cherry.exe\0")' '_T("実行ファイル (*.exe)\0*.exe\0")'
$source = ReplaceOnce $source 'OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY' 'OFN_PATHMUSTEXIST | OFN_HIDEREADONLY'
$source = ReplaceOnce $source 'chySharpSaveAs(exeFilename, &prm);' @'
if (chySharpSaveAs(exeFilename, &prm))
                        MessageBox(hWnd, _T("保存しました。作成した実行ファイルを起動してください。"), _T("ChySharp Custom"), MB_OK | MB_ICONINFORMATION);
                    else
                        MessageBox(hWnd, _T("保存できませんでした。既存ファイルは上書きしません。別のファイル名と書き込み可能な場所を指定してください。"), _T("ChySharp Custom"), MB_OK | MB_ICONERROR);
'@
$saveAt = $source.IndexOf('/* 改変したバイナリを保存 */')
if ($saveAt -lt 0) { throw 'Save routine not found.' }
$source = $source.Substring(0, $saveAt) + @'
/* Add the modern wheel code AFTER applying all selected upstream options. */
    {
        BYTE *output = chySharpNewExe;
        DWORD outputSize = CHYSHARP_BINSIZE;
        DWORD writtenSize = 0;
        DWORD failure = ERROR_SUCCESS;
        if (prm->dispatchWheel && !WheelCreateImage(chySharpNewExe, CHYSHARP_BINSIZE, &output, &outputSize))
            return FALSE;
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
'@
[IO.File]::WriteAllText((Join-Path $upstream 'chysharp.c'), $source, [Text.UTF8Encoding]::new($false))
$rc = [Text.Encoding]::GetEncoding(932).GetString([IO.File]::ReadAllBytes((Join-Path $upstream 'chysharp.rc')))
$rc = $rc.Replace('afxres.h', 'windows.h').Replace('#pragma code_page(932)', '#pragma code_page(65001)')
$rc = $rc.Replace('Cherry 1.4.3# Patcher', 'Cherry 1.4.3# Patcher - Custom Wheel')
$rc = $rc.Replace('マウスホイール操作を有効にする', 'カーソル位置の縦・横ホイールスクロールを有効にする')
$rc = $rc.Replace('※保存時にバックアップは作成されません！ご注意ください！', '別名で作成します。既存ファイルは上書きしません。')
$rc = $rc.Replace('1,0,0,0', '1,1,0,0').Replace('1.0.0.0', '1.1.0.0')
$rc = $rc.Replace('"chysharp.exe"', '"chysharp-custom.exe"')
$rc = "#ifndef IDC_STATIC`n#define IDC_STATIC -1`n#endif`n" + $rc
[IO.File]::WriteAllText((Join-Path $upstream 'chysharp.rc'), $rc, [Text.UTF8Encoding]::new($false))
[byte[]]$payload = [IO.File]::ReadAllBytes((Join-Path $PSScriptRoot '../note-wheel/wheel-hook.bin'))
$hex = ($payload | ForEach-Object { '0x{0:x2}' -f $_ }) -join ','
[IO.File]::WriteAllText((Join-Path $build 'wheel-payload.h'), "/* Generated from note-wheel/wheel-hook.bin. */`nstatic const unsigned char wheelPayload[] = {$hex};`n")
Write-Host "Prepared modified upstream source: $upstream"
