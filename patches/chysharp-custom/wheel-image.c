#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "wheel-image.h"
#include "wheel-payload.h"

BOOL WheelTemplateMatches(const BYTE *image, DWORD size)
{
    static const BYTE expected[32] = {
        0x65,0x9f,0xa2,0x94,0x8f,0xc2,0x26,0x89,0xe2,0xd8,0x7c,0x0f,0x36,0xe3,0x3a,0x50,
        0x4e,0x9d,0x94,0xb2,0x46,0x93,0xd4,0xe3,0x31,0x5f,0x59,0x04,0xe6,0x6c,0x9f,0xb3
    };
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    BYTE actual[32];
    DWORD hashSize = sizeof(actual);
    BOOL result = FALSE;
    if (size != 926208) return FALSE;
    if (CryptAcquireContext(&provider, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) &&
        CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash) &&
        CryptHashData(hash, image, size, 0) &&
        CryptGetHashParam(hash, HP_HASHVAL, actual, &hashSize, 0))
        result = hashSize == sizeof(expected) && !memcmp(actual, expected, sizeof(expected));
    if (hash) CryptDestroyHash(hash);
    if (provider) CryptReleaseContext(provider, 0);
    return result;
}

static DWORD Align(DWORD value, DWORD alignment)
{
    return (value + alignment - 1) / alignment * alignment;
}

static DWORD RawOffset(const IMAGE_NT_HEADERS32 *nt, DWORD rva, DWORD length)
{
    const IMAGE_SECTION_HEADER *sections = IMAGE_FIRST_SECTION(nt);
    WORD i;
    for (i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if (rva >= sections[i].VirtualAddress &&
            rva - sections[i].VirtualAddress < sections[i].SizeOfRawData) {
            DWORD raw = sections[i].PointerToRawData + rva - sections[i].VirtualAddress;
            if (raw <= length && length - raw >= sizeof(DWORD)) return raw;
        }
    }
    return 0;
}

BOOL WheelCreateImage(const BYTE *source, DWORD size, BYTE **output, DWORD *outputSize)
{
    BYTE *payload = NULL, *result = NULL, *relocations = NULL;
    const IMAGE_NT_HEADERS32 *old;
    IMAGE_NT_HEADERS32 *patch, *target;
    const IMAGE_SECTION_HEADER *sourceSections;
    IMAGE_SECTION_HEADER *payloadSections, *sections;
    DWORD shift, relocSize, oldRelocSize, newRelocSize, oldRelocRaw, newRelocRaw;
    DWORD firstRaw = MAXDWORD, firstRVA = MAXDWORD, raw, lastRVA, headerEnd;
    DWORD position, resultSize;
    WORD i, added = 0, originalCount;
    BOOL success = FALSE;
    *output = NULL;
    *outputSize = 0;
    /* Fixed layout comes from the checksum-verified chysharp template. */
    if (size != 926208 || *(const DWORD *)(source + 60) != 512 ||
        (*(const DWORD *)(source + 0x9db94) != 0x4e8200 &&
         *(const DWORD *)(source + 0x9db94) != 0x48bd5c)) goto done;
    old = (const IMAGE_NT_HEADERS32 *)(source + 512);
    if (old->Signature != IMAGE_NT_SIGNATURE || old->FileHeader.NumberOfSections != 11 ||
        old->OptionalHeader.ImageBase != 0x400000 ||
        old->OptionalHeader.SectionAlignment != 4096 || old->OptionalHeader.FileAlignment != 512)
        goto done;
    payload = (BYTE *)malloc(sizeof(wheelPayload));
    if (!payload) goto done;
    memcpy(payload, wheelPayload, sizeof(wheelPayload));
    patch = (IMAGE_NT_HEADERS32 *)(payload + ((IMAGE_DOS_HEADER *)payload)->e_lfanew);
    if (patch->OptionalHeader.ImageBase != old->OptionalHeader.ImageBase ||
        patch->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress) goto done;
    sourceSections = IMAGE_FIRST_SECTION(old);
    payloadSections = IMAGE_FIRST_SECTION(patch);
    originalCount = old->FileHeader.NumberOfSections;
    for (i = 0; i < originalCount; ++i)
        if (sourceSections[i].SizeOfRawData && sourceSections[i].PointerToRawData < firstRaw)
            firstRaw = sourceSections[i].PointerToRawData;
    for (i = 0; i < patch->FileHeader.NumberOfSections; ++i) {
        if (payloadSections[i].VirtualAddress < firstRVA) firstRVA = payloadSections[i].VirtualAddress;
        if (memcmp(payloadSections[i].Name, ".reloc", 6)) ++added;
    }
    ++added; /* combined relocation section */
    headerEnd = (DWORD)((const BYTE *)sourceSections - source) + (originalCount + added) * sizeof(IMAGE_SECTION_HEADER);
    if (headerEnd > firstRaw) goto done;
    shift = old->OptionalHeader.SizeOfImage - firstRVA;
    oldRelocSize = old->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;
    newRelocSize = patch->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;
    oldRelocRaw = RawOffset(old, old->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress, size);
    newRelocRaw = RawOffset(patch, patch->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress, sizeof(wheelPayload));
    if (!oldRelocRaw || !newRelocRaw || oldRelocSize > size-oldRelocRaw ||
        newRelocSize > sizeof(wheelPayload)-newRelocRaw) goto done;
    relocSize = oldRelocSize + newRelocSize;
    relocations = (BYTE *)malloc(relocSize);
    if (!relocations) goto done;
    memcpy(relocations, source + oldRelocRaw, oldRelocSize);
    memcpy(relocations + oldRelocSize, payload + newRelocRaw, newRelocSize);
    for (position = oldRelocSize; position < relocSize; ) {
        IMAGE_BASE_RELOCATION *block = (IMAGE_BASE_RELOCATION *)(relocations + position);
        DWORD j;
        if (relocSize-position < sizeof(*block) || block->SizeOfBlock < sizeof(*block) ||
            block->SizeOfBlock > relocSize-position) goto done;
        for (j = sizeof(*block); j < block->SizeOfBlock; j += sizeof(WORD)) {
            WORD entry = *(WORD *)((BYTE *)block + j);
            DWORD offset;
            if (!(entry >> 12)) continue;
            if ((entry >> 12) != IMAGE_REL_BASED_HIGHLOW) goto done;
            offset = RawOffset(patch, block->VirtualAddress + (entry & 0xfff), sizeof(wheelPayload));
            if (!offset) goto done;
            *(DWORD *)(payload + offset) += shift;
        }
        block->VirtualAddress += shift;
        position += block->SizeOfBlock;
    }
    resultSize = Align(size, 512) + Align(relocSize, 512);
    for (i = 0; i < patch->FileHeader.NumberOfSections; ++i)
        if (memcmp(payloadSections[i].Name, ".reloc", 6)) resultSize += payloadSections[i].SizeOfRawData;
    result = (BYTE *)calloc(resultSize, 1);
    if (!result) goto done;
    memcpy(result, source, size);
    target = (IMAGE_NT_HEADERS32 *)(result + 512);
    sections = IMAGE_FIRST_SECTION(target) + originalCount;
    raw = Align(size, 512);
    lastRVA = shift;
    added = 0;
    for (i = 0; i < patch->FileHeader.NumberOfSections; ++i) {
        IMAGE_SECTION_HEADER *s = &payloadSections[i], *t;
        DWORD end;
        if (!memcmp(s->Name, ".reloc", 6)) continue;
        t = &sections[added];
        memset(t, 0, sizeof(*t));
        sprintf_s((char *)t->Name, sizeof(t->Name), ".wh%u", added++);
        t->Misc.VirtualSize = s->Misc.VirtualSize;
        t->VirtualAddress = s->VirtualAddress + shift;
        t->SizeOfRawData = s->SizeOfRawData;
        t->PointerToRawData = s->SizeOfRawData ? raw : 0;
        t->Characteristics = s->Characteristics;
        if (s->SizeOfRawData) {
            if (s->PointerToRawData > sizeof(wheelPayload) ||
                s->SizeOfRawData > sizeof(wheelPayload)-s->PointerToRawData) goto done;
            memcpy(result+raw, payload+s->PointerToRawData, s->SizeOfRawData);
        }
        raw += s->SizeOfRawData;
        end = Align(t->VirtualAddress + max(t->Misc.VirtualSize,t->SizeOfRawData), 4096);
        if (end > lastRVA) lastRVA = end;
    }
    memset(&sections[added], 0, sizeof(*sections));
    memcpy(sections[added].Name, ".whrel", 6);
    sections[added].Misc.VirtualSize = relocSize;
    sections[added].VirtualAddress = lastRVA;
    sections[added].SizeOfRawData = Align(relocSize, 512);
    sections[added].PointerToRawData = raw;
    sections[added].Characteristics = 0x42000040;
    memcpy(result+raw, relocations, relocSize);
    target->FileHeader.NumberOfSections = originalCount + added + 1;
    target->OptionalHeader.SizeOfImage = Align(lastRVA+relocSize, 4096);
    target->OptionalHeader.SizeOfHeaders = Align(headerEnd, 512);
    target->OptionalHeader.CheckSum = 0;
    target->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress = lastRVA;
    target->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size = relocSize;
    *(DWORD *)(result+0x9db94) = old->OptionalHeader.ImageBase + shift + patch->OptionalHeader.AddressOfEntryPoint;
    *output = result;
    *outputSize = resultSize;
    result = NULL;
    success = TRUE;
done:
    free(result);
    free(payload);
    free(relocations);
    if (!success) SetLastError(ERROR_INVALID_DATA);
    return success;
}
