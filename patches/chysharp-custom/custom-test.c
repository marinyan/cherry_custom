#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chysharp.h"
#include "wheel-image.h"

extern BYTE *chySharpNewExe, *chySharpTmplExe;
static BYTE *Read(const wchar_t *path, DWORD *size)
{
    FILE *file = NULL;
    BYTE *bytes;
    long length;
    assert(!_wfopen_s(&file, path, L"rb") && file);
    assert(!fseek(file, 0, SEEK_END));
    length = ftell(file);
    assert(length > 0);
    rewind(file);
    *size = (DWORD)length;
    bytes = (BYTE *)malloc(*size);
    assert(bytes && fread(bytes, 1, *size, file) == *size);
    fclose(file);
    return bytes;
}

static void VerifyPreserved(const BYTE *output, DWORD size, BOOL wheel)
{
    DWORD i;
    const IMAGE_NT_HEADERS32 *old = (const IMAGE_NT_HEADERS32 *)(chySharpNewExe+512);
    const IMAGE_NT_HEADERS32 *nt = (const IMAGE_NT_HEADERS32 *)(output+512);
    DWORD opt = (DWORD)((const BYTE *)&old->OptionalHeader - chySharpNewExe);
    DWORD tableEnd = (DWORD)((const BYTE *)(IMAGE_FIRST_SECTION(old)+11) - chySharpNewExe);
    if (!wheel) {
        assert(size == 926208 && !memcmp(output, chySharpNewExe, size));
        assert(*(const DWORD *)(output+0x9db94) == 0x48bd5c);
        return;
    }
    assert(size > 926208 && nt->FileHeader.NumberOfSections == 15);
    assert(*(const DWORD *)(output+0x9db94) >= 0x4ec000);
    for (i=0; i<926208; ++i) {
        if ((i>=518 && i<520) || (i>=opt+56 && i<opt+68) ||
            (i>=opt+136 && i<opt+144) || (i>=0x9db94 && i<0x9db98) ||
            (i>=tableEnd && i<tableEnd+160)) continue;
        assert(output[i] == chySharpNewExe[i]);
    }
}

int wmain(int argc, wchar_t **argv)
{
    DWORD templateSize;
    int test, j;
    assert(argc == 3);
    chySharpTmplExe = Read(argv[1], &templateSize);
    assert(WheelTemplateMatches(chySharpTmplExe, templateSize));
    chySharpTmplExe[0xe2c] ^= 1;
    assert(!WheelTemplateMatches(chySharpTmplExe, templateSize));
    chySharpTmplExe[0xe2c] ^= 1;
    chySharpNewExe = (BYTE *)malloc(templateSize);
    assert(chySharpNewExe);
    assert(sizeof(ChySharpPrms) == 15*sizeof(BOOL));
    for (test=-2; test<15; ++test) {
        BOOL options[15];
        ChySharpPrms settings;
        wchar_t path[MAX_PATH];
        BYTE *output, *again;
        DWORD size, againSize;
        for (j=0; j<15; ++j) options[j] = test != -2 && test != j;
        memcpy(&settings, options, sizeof(settings));
        if (test == -2) swprintf_s(path, MAX_PATH, L"%s\\all-off.exe", argv[2]);
        else if (test == -1) swprintf_s(path, MAX_PATH, L"%s\\all-on.exe", argv[2]);
        else swprintf_s(path, MAX_PATH, L"%s\\option-%02d-off.exe", argv[2], test);
        assert(chySharpSaveAs(path, &settings));
        output = Read(path, &size);
        VerifyPreserved(output, size, settings.dispatchWheel);
        assert(!chySharpSaveAs(path, &settings));
        assert(GetLastError() == ERROR_FILE_EXISTS);
        again = Read(path, &againSize);
        assert(size == againSize && !memcmp(output, again, size));
        free(again);
        free(output);
    }
    free(chySharpNewExe);
    free(chySharpTmplExe);
    puts("PASS: all enabled, all disabled, each of 15 options disabled, unchanged existing fixes, no overwrite, template validation");
    return 0;
}
