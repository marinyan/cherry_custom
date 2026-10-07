#ifndef CHYSHARP_CUSTOM_WHEEL_IMAGE_H
#define CHYSHARP_CUSTOM_WHEEL_IMAGE_H
#include <windows.h>
BOOL WheelTemplateMatches(const BYTE *image, DWORD size);
/* Input is a copy of the verified template after applying chysharp options.
 * Caller owns the returned malloc buffer. No source buffer is modified. */
BOOL WheelCreateImage(const BYTE *source, DWORD size, BYTE **output, DWORD *outputSize);
#endif
