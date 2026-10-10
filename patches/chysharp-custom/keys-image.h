#ifndef CHYSHARP_KEYS_IMAGE_H
#define CHYSHARP_KEYS_IMAGE_H
#include <windows.h>
/* Input is the checksum-verified template after applying selected fixes. */
BOOL KeysCreateImage(const BYTE *source, DWORD size, BYTE **output, DWORD *outputSize);
#endif
