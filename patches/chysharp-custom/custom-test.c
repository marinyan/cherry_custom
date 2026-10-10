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

static void VerifyPreserved(const BYTE *output, DWORD size, BOOL wheel, BOOL keys)
{
    DWORD i;
    const IMAGE_NT_HEADERS32 *old = (const IMAGE_NT_HEADERS32 *)(chySharpNewExe+512);
    const IMAGE_NT_HEADERS32 *nt = (const IMAGE_NT_HEADERS32 *)(output+512);
    DWORD opt = (DWORD)((const BYTE *)&old->OptionalHeader - chySharpNewExe);
    DWORD tableEnd = (DWORD)((const BYTE *)(IMAGE_FIRST_SECTION(old)+11) - chySharpNewExe);
    if (!wheel && !keys) {
        assert(size == 926208 && !memcmp(output, chySharpNewExe, size));
        assert(*(const DWORD *)(output+0x9db94) == 0x48bd5c);
        return;
    }
    assert(size > 926208 && nt->FileHeader.NumberOfSections == 11+4*wheel+keys);
    if (wheel) assert(*(const DWORD *)(output+0x9db94) >= 0x4ec000);
    else assert(*(const DWORD *)(output+0x9db94) == 0x48bd5c);
    for (i=0; i<926208; ++i) {
        if ((i>=518 && i<520) || (i>=opt+56 && i<opt+68) ||
            (i>=opt+136 && i<opt+144) || (i>=0x9db94 && i<0x9db98) ||
            (i>=tableEnd && i<tableEnd+40*(4*wheel+keys)) ||
            (keys && ((i>=opt+8 && i<opt+12) || (i>=814848 && i<814856) ||
                (i>=814192 && i<814200)))) continue;
        assert(output[i] == chySharpNewExe[i]);
    }
}

static void VerifyResources(const wchar_t *path, BOOL keys)
{
    HMODULE module = LoadLibraryExW(path, NULL, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    HACCEL table;
    ACCEL entries[19];
    HMENU menu;
    WCHAR label[128];
    int count;
    assert(module);
    table = LoadAcceleratorsW(module, MAKEINTRESOURCEW(1));
    assert(table);
    count = CopyAcceleratorTableW(table, entries, 19);
    assert(count == (keys ? 19 : 17));
    assert(entries[0].key == VK_BACK && entries[0].cmd == 24321); /* Legacy Undo. */
    assert(entries[9].key == 'S' && entries[9].cmd == (keys ? 1002 : 10601));
    if (keys) {
        assert(entries[9].fVirt == (FVIRTKEY | FCONTROL));
        assert(entries[17].key == 'S' && entries[17].cmd == 10601 &&
            entries[17].fVirt == (FVIRTKEY | FCONTROL | FALT));
        assert(entries[18].key == 'Z' && entries[18].cmd == 24321 &&
            entries[18].fVirt == (FVIRTKEY | FCONTROL));
    }
    menu = LoadMenuW(module, MAKEINTRESOURCEW(1));
    assert(menu);
    assert(GetMenuStringW(GetSubMenu(menu,0), 1002, label, 128, MF_BYCOMMAND));
    assert((wcsstr(label, L"Ctrl+S") != NULL) == !!keys);
    assert(GetMenuStringW(GetSubMenu(menu,1), 24321, label, 128, MF_BYCOMMAND));
    assert((wcsstr(label, L"Ctrl+Z") != NULL) == !!keys);
    assert(wcsstr(label, L"Alt+BkSp"));
    DestroyMenu(menu);
    FreeLibrary(module);
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
    assert(sizeof(ChySharpPrms) == 16*sizeof(BOOL));
    for (test=-2; test<16; ++test) {
        BOOL options[16];
        ChySharpPrms settings;
        wchar_t path[MAX_PATH];
        BYTE *output, *again;
        DWORD size, againSize;
        for (j=0; j<16; ++j) options[j] = test != -2 && test != j;
        memcpy(&settings, options, sizeof(settings));
        if (test == -2) swprintf_s(path, MAX_PATH, L"%s\\all-off.exe", argv[2]);
        else if (test == -1) swprintf_s(path, MAX_PATH, L"%s\\all-on.exe", argv[2]);
        else swprintf_s(path, MAX_PATH, L"%s\\option-%02d-off.exe", argv[2], test);
        assert(chySharpSaveAs(path, &settings));
        output = Read(path, &size);
        VerifyPreserved(output, size, settings.dispatchWheel, settings.standardKeys);
        VerifyResources(path, settings.standardKeys);
        assert(!chySharpSaveAs(path, &settings));
        assert(GetLastError() == ERROR_FILE_EXISTS);
        again = Read(path, &againSize);
        assert(size == againSize && !memcmp(output, again, size));
        free(again);
        free(output);
    }
    free(chySharpNewExe);
    free(chySharpTmplExe);
    puts("PASS: 18 option combinations, native accelerators/menu, preserved fixes, no overwrite, template validation");
    return 0;
}
