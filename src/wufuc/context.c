#include "stdafx.h"
#include "context.h"

#include <sddl.h>

static bool ctxp_remove_handle(context *ctx, unsigned Index)
{
        if ( !ctx || Index > _countof(ctx->handles) || Index > ctx->count )
                return false;

        EnterCriticalSection(&ctx->cs);
        for ( unsigned i = Index; i < ctx->count - 1; i++ ) {
                ctx->handles[i] = ctx->handles[i + 1];
                ctx->tags[i] = ctx->tags[i + 1];
        }
        LeaveCriticalSection(&ctx->cs);
        return true;
}

static bool ctxp_create_new_mutex(bool InitialOwner,
        const wchar_t *MutexName,
        HANDLE *pMutexHandle)
{
        HANDLE hMutex;

        hMutex = CreateMutexW(NULL, InitialOwner, MutexName);
        if ( hMutex ) {
                if ( GetLastError() == ERROR_ALREADY_EXISTS ) {
                        CloseHandle(hMutex);
                        return false;
                }
                *pMutexHandle = hMutex;
                return true;
        }
        return false;
}

static bool ctxp_create_new_mutex_vfmt(bool InitialOwner,
        HANDLE *pMutexHandle,
        const wchar_t *const NameFormat,
        va_list ArgList)
{
        wchar_t *buffer;
        int ret;
        bool result;

        ret = _vscwprintf(NameFormat, ArgList) + 1;
        buffer = calloc(ret, sizeof *buffer);
        if ( !buffer )
                return false;

        ret = vswprintf_s(buffer, ret, NameFormat, ArgList);
        if ( ret == -1 )
                return false;

        result = ctxp_create_new_mutex(InitialOwner, buffer, pMutexHandle);
        free(buffer);
        return result;
}

static bool ctxp_create_event_with_string_security_descriptor(
        bool ManualReset,
        bool InitialState,
        const wchar_t *Name,
        const wchar_t *StringSecurityDescriptor,
        HANDLE *pEventHandle)
{
        SECURITY_ATTRIBUTES sa = { sizeof sa };
        HANDLE hEvent;

        if ( ConvertStringSecurityDescriptorToSecurityDescriptorW(
                StringSecurityDescriptor,
                SDDL_REVISION_1,
                &sa.lpSecurityDescriptor,
                NULL) ) {

                hEvent = CreateEventW(&sa, ManualReset, InitialState, Name);
                if ( hEvent ) {
                        *pEventHandle = hEvent;
                        return true;
                }
        }
        return false;
}

static unsigned ctxp_find_handle_index(context *ctx, HANDLE Handle)
{
        unsigned result = -1;

        if ( !ctx || !Handle || Handle == INVALID_HANDLE_VALUE )
                return -1;

        EnterCriticalSection(&ctx->cs);
        for ( unsigned i = 0; i < ctx->count; i++ ) {
                if ( ctx->handles[i] == Handle ) {
                        result = i;
                        break;
                }
        }
        LeaveCriticalSection(&ctx->cs);
        return result;
}

bool ctx_create(context *ctx,
        const wchar_t *MutexName,
        unsigned MutexTag,
        const wchar_t *EventName,
        const wchar_t *EventStringSecurityDescriptor,
        unsigned EventTag)
{
        bool result = false;
        
        if ( !ctx ) return false;
        
        ZeroMemory(ctx, sizeof *ctx);
        InitializeCriticalSection(&ctx->cs);
        EnterCriticalSection(&ctx->cs);

        if ( ctxp_create_new_mutex(true, MutexName, &ctx->mutex) ) {
                ctx->mutex_tag = MutexTag;
                ctx->count++;

                result = ctx_add_event(ctx, EventName, EventStringSecurityDescriptor, EventTag, NULL);
                if ( !result ) {
                        ReleaseMutex(ctx->mutex);
                        ctx_close_and_remove_handle(ctx, ctx->mutex);
                }
        }
        LeaveCriticalSection(&ctx->cs);
        return result;
}

bool ctx_delete(context *ctx)
{
        if ( !ctx || ctx->count > _countof(ctx->handles) )
                return false;

        EnterCriticalSection(&ctx->cs);
        while ( ctx->count-- ) {
                CloseHandle(ctx->handles[ctx->count]);
                ctx->handles[ctx->count] = INVALID_HANDLE_VALUE;
                ctx->tags[ctx->count] = -1;
        }
        LeaveCriticalSection(&ctx->cs);
        DeleteCriticalSection(&ctx->cs);
        return true;
}

unsigned ctx_add_handle(context *ctx, HANDLE Handle, unsigned Tag)
{
        unsigned result = -1;

        if ( !ctx || !Handle )
                return -1;

        EnterCriticalSection(&ctx->cs);
        result = ctxp_find_handle_index(ctx, Handle);
        if ( result != -1) {
                ctx->tags[result] = Tag;
        } else if ( ctx->count < _countof(ctx->handles) ) {
                ctx->handles[ctx->count] = Handle;
                ctx->tags[ctx->count] = Tag;

                result = ctx->count++;
        }
        LeaveCriticalSection(&ctx->cs);
        return result;
}

bool ctx_get_tag(context *ctx, HANDLE Handle, unsigned *pTag)
{
        bool result = false;
        unsigned index;

        if ( !ctx || !Handle || !pTag )
                return false;

        EnterCriticalSection(&ctx->cs);
        index = ctxp_find_handle_index(ctx, Handle);
        if ( index != -1 ) {
                if ( pTag ) {
                        *pTag = ctx->tags[index];
                        result = true;
                }
        }
        LeaveCriticalSection(&ctx->cs);
        return result;
}

unsigned ctx_add_new_mutex(context *ctx,
        bool InitialOwner,
        const wchar_t *Name,
        unsigned Tag,
        HANDLE *pMutexHandle)
{
        HANDLE hMutex;
        unsigned result = -1;

        EnterCriticalSection(&ctx->cs);
        if ( ctxp_create_new_mutex(InitialOwner, Name, &hMutex) ) {
                result = ctx_add_handle(ctx, hMutex, Tag);
                if ( result != -1 ) {
                        if ( pMutexHandle )
                                *pMutexHandle = hMutex;
                } else {
                        CloseHandle(hMutex);
                }
        }
        LeaveCriticalSection(&ctx->cs);
        return result;
}

unsigned ctx_add_new_mutex_fmt(context *ctx,
        bool InitialOwner,
        unsigned Tag,
        HANDLE *pMutexHandle,
        const wchar_t *const NameFormat,
        ...)
{
        HANDLE hMutex;
        unsigned result = -1;
        va_list arglist;

        EnterCriticalSection(&ctx->cs);
        va_start(arglist, NameFormat);
        if ( ctxp_create_new_mutex_vfmt(InitialOwner, &hMutex, NameFormat, arglist) ) {
                result = ctx_add_handle(ctx, hMutex, Tag);
                if ( result != -1 ) {
                        if ( pMutexHandle )
                                *pMutexHandle = hMutex;
                } else {
                        CloseHandle(hMutex);
                }
        }
        va_end(arglist);
        LeaveCriticalSection(&ctx->cs);
        return result;
}

unsigned ctx_add_event(context *ctx,
        const wchar_t *Name,
        const wchar_t *StringSecurityDescriptor,
        unsigned Tag,
        HANDLE *pEventHandle)
{
        unsigned result = -1;
        HANDLE hEvent;

        EnterCriticalSection(&ctx->cs);
        if ( ctxp_create_event_with_string_security_descriptor(
                true,
                false,
                Name,
                StringSecurityDescriptor,
                &hEvent) ) {
                result = ctx_add_handle(ctx, hEvent, Tag);
                if ( result != -1 ) {
                        if ( pEventHandle )
                                *pEventHandle = hEvent;
                } else {
                        CloseHandle(hEvent);
                }
        }
        LeaveCriticalSection(&ctx->cs);
        return result;
}

bool ctx_wait(context *ctx, bool WaitAll, bool Alertable, DWORD *pResult)
{
        int ret;
        unsigned count;
        HANDLE handles[MAXIMUM_WAIT_OBJECTS];

        EnterCriticalSection(&ctx->cs);
        count = ctx->count;
        ret = memcpy_s(handles, sizeof(handles), ctx->handles, count * (sizeof *handles));
        LeaveCriticalSection(&ctx->cs);

        if ( !ret ) {
                *pResult = WaitForMultipleObjectsEx(count,
                        handles,
                        WaitAll,
                        INFINITE,
                        Alertable);
                return true;
        }
        return false;
}

bool ctx_close_and_remove_handle(context *ctx,
        HANDLE Handle)
{
        bool result = false;
        unsigned index;

        EnterCriticalSection(&ctx->cs);
        index = ctxp_find_handle_index(ctx, Handle);
        result = index != -1
                && CloseHandle(Handle)
                && ctxp_remove_handle(ctx, index);
        LeaveCriticalSection(&ctx->cs);
        return result;
}

bool ctx_duplicate_context(const context *pSrc,
        HANDLE hProcess,
        context *pDst,
        HANDLE Handle,
        DWORD DesiredAccess,
        unsigned Tag)
{
        bool result = false;
        HANDLE hSrcProcess;
        HANDLE hTarget;
        size_t index;

        hSrcProcess = GetCurrentProcess();

        if ( !DuplicateHandle(hSrcProcess, pSrc->mutex, hProcess, &pDst->mutex, SYNCHRONIZE, FALSE, 0) )
                return false;

        if ( !DuplicateHandle(hSrcProcess, pSrc->uevent, hProcess, &pDst->uevent, SYNCHRONIZE, FALSE, 0) ) {
close_mutex:
                CloseHandle(pDst->mutex);
                pDst->mutex = INVALID_HANDLE_VALUE;
                return false;
        }
        if ( !DuplicateHandle(hSrcProcess, Handle, hProcess, &hTarget, 0, FALSE, DesiredAccess)
                || (index = ctx_add_handle(pDst, hTarget, Tag)) == -1 ) {

                CloseHandle(pDst->uevent);
                pDst->uevent = INVALID_HANDLE_VALUE;
                goto close_mutex;

        }
        pDst->tags[0] = index;
        return true;
}
