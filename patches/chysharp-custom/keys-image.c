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

static DWORD Align(DWORD value, DWORD alignment)
{
    return (value + alignment - 1) / alignment * alignment;
}

BOOL KeysCreateImage(const BYTE *source, DWORD size, BYTE **output, DWORD *outputSize)
{
    static const WCHAR save[] = L"\x4e0a\x66f8\x304d\x4fdd\x5b58(&S)";
    static const WCHAR undo[] = L"\x5143\x306b\x623b\x3059(&U)\tAlt+BkSp";
    static const WCHAR saveLabel[] = L"\x4e0a\x66f8\x304d\x4fdd\x5b58(&S)\tCtrl+S";
    static const WCHAR undoLabel[] = L"\x5143\x306b\x623b\x3059(&U)\tCtrl+Z / Alt+BkSp";
    const IMAGE_NT_HEADERS32 *old;
    IMAGE_NT_HEADERS32 *nt;
    IMAGE_SECTION_HEADER *section;
    BYTE menu[MENU_SIZE+128], accelerators[19*8];
    BYTE *result;
    DWORD i = 0, length = 0, found = 0, firstRaw = MAXDWORD;
    DWORD headerEnd, raw, rva, contentSize, total;
    WORD count;
    *output = NULL;
    *outputSize = 0;
    if (size < 926208 || *(const DWORD *)(source+60) != 512) goto invalid;
    old = (const IMAGE_NT_HEADERS32 *)(source+512);
    count = old->FileHeader.NumberOfSections;
    if (old->Signature != IMAGE_NT_SIGNATURE || (count != 11 && count != 15) ||
        old->OptionalHeader.FileAlignment != 512 || old->OptionalHeader.SectionAlignment != 4096 ||
        *(const DWORD *)(source+ACCEL_LEAF) != ACCEL_RAW+35840 ||
        *(const DWORD *)(source+ACCEL_LEAF+4) != 17*8 ||
        *(const DWORD *)(source+MENU_LEAF) != MENU_RAW+35840 ||
        *(const DWORD *)(source+MENU_LEAF+4) != MENU_SIZE) goto invalid;
    memcpy(accelerators, source+ACCEL_RAW, 17*8);
    /* Ctrl+S used command 10601 (WM_VSCROLL/SB_PAGEUP). Preserve it on Ctrl+Alt+S. */
    if (*(WORD *)(accelerators+9*8) != 9 || *(WORD *)(accelerators+9*8+2) != 'S' ||
        *(WORD *)(accelerators+9*8+4) != 10601 || *(WORD *)(accelerators+16*8) != 0x89)
        goto invalid;
    memcpy(accelerators+17*8, accelerators+9*8, 8);
    *(WORD *)(accelerators+17*8) = FVIRTKEY | FCONTROL | FALT;
    *(WORD *)(accelerators+9*8+4) = 1002; /* Existing File / Save command. */
    *(WORD *)(accelerators+16*8) &= ~0x80;
    memset(accelerators+18*8, 0, 8);
    *(WORD *)(accelerators+18*8) = 0x80 | FVIRTKEY | FCONTROL;
    *(WORD *)(accelerators+18*8+2) = 'Z';
    *(WORD *)(accelerators+18*8+4) = 24321; /* Existing Undo command. */
    while (i < MENU_SIZE) {
        if (i+sizeof(save) <= MENU_SIZE && !memcmp(source+MENU_RAW+i, save, sizeof(save))) {
            memcpy(menu+length, saveLabel, sizeof(saveLabel));
            length += sizeof(saveLabel); i += sizeof(save); found += 1;
        } else if (i+sizeof(undo) <= MENU_SIZE && !memcmp(source+MENU_RAW+i, undo, sizeof(undo))) {
            memcpy(menu+length, undoLabel, sizeof(undoLabel));
            length += sizeof(undoLabel); i += sizeof(undo); found += 2;
        } else {
            if (length+2 > sizeof(menu)) goto invalid;
            memcpy(menu+length, source+MENU_RAW+i, 2); length += 2; i += 2;
        }
    }
    if (found != 3) goto invalid;
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
