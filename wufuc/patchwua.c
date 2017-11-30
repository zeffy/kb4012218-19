#include "stdafx.h"
#include "patchwua.h"
#include "patternfind.h"

#ifdef _M_AMD64
static const PatchSet X64PatchSet = { "FFF3 4883EC?? 33DB 391D???????? 7508 8B05????????", 0xA, 0x12 };
#elif defined(_M_IX86)
static const PatchSet Win7X86PatchSet = { "833D????????00 743E E8???????? A3????????", 0x2, 0xF };
static const PatchSet Win81X86PatchSet = { "8BFF 51 833D????????00 7507 A1????????", 0x5, 0xD };
#endif

bool calculate_pointers(uintptr_t lpfn, const PatchSet *ps, LPBOOL *ppba, LPBOOL *ppbb)
{
#ifdef _M_AMD64
        *ppba = (LPBOOL)(lpfn + ps->Offset1 + sizeof(uint32_t) + *(uint32_t *)(lpfn + ps->Offset1));
        *ppbb = (LPBOOL)(lpfn + ps->Offset2 + sizeof(uint32_t) + *(uint32_t *)(lpfn + ps->Offset2));
        return true;
#elif defined(_M_IX86)
        *ppba = (LPBOOL)(*(uintptr_t *)(lpfn + ps->Offset1));
        *ppbb = (LPBOOL)(*(uintptr_t *)(lpfn + ps->Offset2));
        return true;
#endif
        return false;
}

bool patch_wua(void *lpBaseOfDll, size_t SizeOfImage, const wchar_t *fname)
{
        bool result = false;

        const PatchSet *pps = NULL;
#ifdef _M_AMD64
        pps = &X64PatchSet;
#elif defined(_M_IX86)
        if ( 1/*verify_win7()*/ )
                pps = &Win7X86PatchSet;
        else if ( 1/*verify_win81()*/ )
                pps = &Win81X86PatchSet;
        else
                goto L1;
#endif
        size_t offset = patternfind(lpBaseOfDll, SizeOfImage, pps->Pattern);
        if ( offset == -1 ) {
                trace(L"No pattern match! (couldn't patch)");
                goto L1;
        }
        uintptr_t ptr = (uintptr_t)lpBaseOfDll + offset;

        LPBOOL pba, pbb;
        if ( calculate_pointers((uintptr_t)ptr, pps, &pba, &pbb) ) {
                DWORD flOldProtect;
                if ( *pba == TRUE ) {
                        if ( VirtualProtect(pba, sizeof *pba, PAGE_READWRITE, &flOldProtect) ) {
                                *pba = FALSE;
                                trace(L"Patched value #1 at %ls!%p: %08X", fname, pba, *pba);
                                if ( !VirtualProtect(pba, sizeof *pba, flOldProtect, &flOldProtect) )
                                        trace(L"Failed to restore memory region permissions at %ls!%p (error code=%08X)", fname, pba, GetLastError());
                        } else trace(L"Failed to change memory region permissions at %ls!%p (error code=%08X)", fname, pba, GetLastError());
                }
                if ( *pbb == FALSE ) {
                        if ( VirtualProtect(pbb, sizeof *pbb, PAGE_READWRITE, &flOldProtect) ) {
                                *pbb = TRUE;
                                trace(L"Patched value #2 at %ls!%p: %08X", fname, pbb, *pbb);
                                if ( !VirtualProtect(pbb, sizeof *pbb, flOldProtect, &flOldProtect) )
                                        trace(L"Failed to restore memory region permissions at %ls!%p: (error code=%08X)", fname, pbb, GetLastError());
                        } else trace(L"Failed to change memory region permissions at %ls!%p (error code=%08X)", fname, pbb, GetLastError());
                }
                result = !*pba && *pbb;
        }
L1:     return result;
}
