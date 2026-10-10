/* Add native accelerators, without intercepting text input or dialog keys. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include "keys-image.h"

#define ACCEL_RAW 867232
#define ACCEL_LEAF 814848
#define MENU_RAW 845148
#define MENU_SIZE 1790
#define MENU_LEAF 814192
#define CTRL_A_BRANCH 200708

static DWORD Align(DWORD value, DWORD alignment)
{
    return (value + alignment - 1) / alignment * alignment;
}

BOOL KeysCreateImage(const BYTE *source, DWORD size, BYTE **output, DWORD *outputSize)
{
    static const struct { WORD command; const WCHAR *label; } labels[] = {
        {1000, L"\tCtrl+O"}, {1001, L"\tCtrl+N"},
        {1002, L"\tCtrl+S"}, {1003, L"\tCtrl+Shift+S"},
        {24321, L"\tCtrl+Z / Alt+BkSp"}, {24322, L"\tCtrl+X / Shift+Del"},
        {24323, L"\tCtrl+C / Ctrl+Ins"}, {24324, L"\tCtrl+V / Shift+Ins"},
        {4200, L"\tCtrl+A"}, {4108, L"\tCtrl+Alt+A"}
    };
    static const WORD extra[][4] = {
        {FVIRTKEY | FCONTROL | FALT, 'S', 10601, 0},
        {FVIRTKEY | FCONTROL, 'Z', 24321, 0},
        {FVIRTKEY | FCONTROL | FALT, 'X', 10603, 0},
        {FVIRTKEY | FCONTROL | FALT, 'A', 4108, 0},
        {FVIRTKEY | FCONTROL, 'N', 1001, 0},
        {FVIRTKEY | FCONTROL, 'O', 1000, 0},
        {0x80 | FVIRTKEY | FCONTROL | FSHIFT, 'S', 1003, 0}
    };
    const IMAGE_NT_HEADERS32 *old;
    IMAGE_NT_HEADERS32 *nt;
    IMAGE_SECTION_HEADER *section;
    BYTE menu[MENU_SIZE+1024], accelerators[17*8+sizeof(extra)];
    BYTE *result;
    DWORD i = 0, length = 0, found = 0, firstRaw = MAXDWORD;
    DWORD headerEnd, raw, rva, contentSize, total;
    WORD count;
    *output = NULL;
    *outputSize = 0;
    if (size < 926208 || *(const DWORD *)(source+60) != 512) goto invalid;
    /* Cherry's key preprocessor consumes Ctrl+A before TranslateAccelerator,
     * sending Select All to the wrong window. Let the native accelerator route
     * it instead. Validate the whole conditional before changing JNE to JMP. */
    if (memcmp(source+CTRL_A_BRANCH-3, "\x83\xfa\x41\x75\x1e\x83\xf8\x02\x75\x19", 10))
        goto invalid;
    old = (const IMAGE_NT_HEADERS32 *)(source+512);
    count = old->FileHeader.NumberOfSections;
    if (old->Signature != IMAGE_NT_SIGNATURE || (count != 11 && count != 15) ||
        old->OptionalHeader.FileAlignment != 512 || old->OptionalHeader.SectionAlignment != 4096 ||
        *(const DWORD *)(source+ACCEL_LEAF) != ACCEL_RAW+35840 ||
        *(const DWORD *)(source+ACCEL_LEAF+4) != 17*8 ||
        *(const DWORD *)(source+MENU_LEAF) != MENU_RAW+35840 ||
        *(const DWORD *)(source+MENU_LEAF+4) != MENU_SIZE) goto invalid;
    memcpy(accelerators, source+ACCEL_RAW, 17*8);
    /* Preserve the old S/X/A commands on Ctrl+Alt+S/X/A. */
    if (*(WORD *)(accelerators+9*8) != 9 || *(WORD *)(accelerators+9*8+2) != 'S' ||
        *(WORD *)(accelerators+9*8+4) != 10601 ||
        *(WORD *)(accelerators+11*8) != 9 || *(WORD *)(accelerators+11*8+2) != 'X' ||
        *(WORD *)(accelerators+11*8+4) != 10603 ||
        *(WORD *)(accelerators+16*8) != 0x89 || *(WORD *)(accelerators+16*8+2) != 'A' ||
        *(WORD *)(accelerators+16*8+4) != 4108)
        goto invalid;
    *(WORD *)(accelerators+9*8+4) = 1002; /* Existing File / Save command. */
    *(WORD *)(accelerators+11*8+4) = 24322; /* Cut. */
    *(WORD *)(accelerators+16*8+4) = 4200; /* Select all. */
    *(WORD *)(accelerators+16*8) &= ~0x80;
    memcpy(accelerators+17*8, extra, sizeof(extra));
    /* Standard MENU template: header, then popup/command records in tree order.
     * Keep all original flags, command IDs, captions and submenu boundaries. */
    if (*(DWORD *)(source+MENU_RAW) != 0) goto invalid;
    memcpy(menu, source+MENU_RAW, 4);
    i = length = 4;
    while (i < MENU_SIZE) {
        WORD flags, command;
        DWORD prefix, start, captionEnd, end, j, suffixSize = 2;
        const WCHAR *suffix = L"";
        if (i+2 > MENU_SIZE) goto invalid;
        flags = *(const WORD *)(source+MENU_RAW+i);
        prefix = (flags & MF_POPUP) ? 2 : 4;
        if (i+prefix > MENU_SIZE) goto invalid;
        command = (flags & MF_POPUP) ? 0 : *(const WORD *)(source+MENU_RAW+i+2);
        start = i+prefix;
        end = start;
        while (end+2 <= MENU_SIZE && *(const WORD *)(source+MENU_RAW+end)) end += 2;
        if (end+2 > MENU_SIZE) goto invalid;
        captionEnd = end;
        for (j=0; j<sizeof(labels)/sizeof(labels[0]); ++j) {
            if (command != labels[j].command) continue;
            if (found & (1u<<j)) goto invalid;
            found |= 1u<<j;
            for (captionEnd=start; captionEnd<end; captionEnd+=2)
                if (*(const WORD *)(source+MENU_RAW+captionEnd) == '\t') break;
            suffix = labels[j].label;
            suffixSize = ((DWORD)wcslen(suffix)+1)*sizeof(WCHAR);
            break;
        }
        if (length+prefix+captionEnd-start+suffixSize > sizeof(menu)) goto invalid;
        memcpy(menu+length, source+MENU_RAW+i, prefix+captionEnd-start);
        length += prefix+captionEnd-start;
        memcpy(menu+length, suffix, suffixSize);
        length += suffixSize;
        i = end+2;
    }
    if (found != (1u<<(sizeof(labels)/sizeof(labels[0])))-1) goto invalid;
    section = IMAGE_FIRST_SECTION(old);
    for (i=0; i<count; ++i)
        if (section[i].SizeOfRawData && section[i].PointerToRawData < firstRaw)
            firstRaw = section[i].PointerToRawData;
    headerEnd = (DWORD)((const BYTE *)(section+count+1)-source);
    if (headerEnd > firstRaw) goto invalid;
    raw = Align(size, 512);
    rva = Align(old->OptionalHeader.SizeOfImage, 4096);
    contentSize = sizeof(accelerators) + length;
    total = raw + Align(contentSize, 512);
    result = (BYTE *)calloc(total, 1);
    if (!result) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return FALSE; }
    memcpy(result, source, size);
    result[CTRL_A_BRANCH] = 0xeb;
    memcpy(result+raw, accelerators, sizeof(accelerators));
    memcpy(result+raw+sizeof(accelerators), menu, length);
    nt = (IMAGE_NT_HEADERS32 *)(result+512);
    section = IMAGE_FIRST_SECTION(nt)+count;
    memset(section, 0, sizeof(*section));
    memcpy(section->Name, ".keys", 5);
    section->Misc.VirtualSize = contentSize;
    section->VirtualAddress = rva;
    section->SizeOfRawData = Align(contentSize, 512);
    section->PointerToRawData = raw;
    section->Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ;
    nt->FileHeader.NumberOfSections = count+1;
    nt->OptionalHeader.SizeOfInitializedData += section->SizeOfRawData;
    nt->OptionalHeader.SizeOfImage = Align(rva+contentSize, 4096);
    nt->OptionalHeader.SizeOfHeaders = Align(headerEnd, 512);
    nt->OptionalHeader.CheckSum = 0;
    *(DWORD *)(result+ACCEL_LEAF) = rva;
    *(DWORD *)(result+ACCEL_LEAF+4) = sizeof(accelerators);
    *(DWORD *)(result+MENU_LEAF) = rva+sizeof(accelerators);
    *(DWORD *)(result+MENU_LEAF+4) = length;
    *output = result;
    *outputSize = total;
    return TRUE;
invalid:
    SetLastError(ERROR_INVALID_DATA);
    return FALSE;
}
