#include "stdafx.h"
#include "context.h"

#include <sddl.h>

static bool ctxp_remove_handle(context *ctx, DWORD Index)
{
        if ( !ctx || Index > _countof(ctx->handles) || Index > ctx->count )
                return false;

        EnterCriticalSection(&ctx->cs);
        for ( DWORD i = Index; i < ctx->count - 1; i++ ) {
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

static DWORD ctxp_find_handle_index(context *ctx, HANDLE Handle)
{
        DWORD result = -1;

        if ( !ctx || !Handle || Handle == INVALID_HANDLE_VALUE )
                return -1;

        EnterCriticalSection(&ctx->cs);
        for ( DWORD i = 0; i < ctx->count; i++ ) {
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
        DWORD MutexTag,
        const wchar_t *EventName,
        const wchar_t *EventStringSecurityDescriptor,
        DWORD EventTag)
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

DWORD ctx_add_handle(context *ctx, HANDLE Handle, DWORD Tag)
{
        DWORD result = -1;

        if ( !ctx || !Handle )
                return -1;

        EnterCriticalSection(&ctx->cs);
        result = ctxp_find_handle_index(ctx, Handle);
        if ( result != -1 ) {
                ctx->tags[result] = Tag;
        } else if ( ctx->count < _countof(ctx->handles) ) {
                ctx->handles[ctx->count] = Handle;
                ctx->tags[ctx->count] = Tag;

                result = ctx->count++;
        }
        LeaveCriticalSection(&ctx->cs);
        return result;
}

bool ctx_get_tag(context *ctx, HANDLE Handle, DWORD *pTag)
{
        bool result = false;
        DWORD index;

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

DWORD ctx_add_new_mutex(context *ctx,
        bool InitialOwner,
        const wchar_t *Name,
        DWORD Tag,
        HANDLE *pMutexHandle)
{
        HANDLE hMutex;
        DWORD result = -1;

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

DWORD ctx_add_new_mutex_fmt(context *ctx,
        bool InitialOwner,
        DWORD Tag,
        HANDLE *pMutexHandle,
        const wchar_t *const NameFormat,
        ...)
{
        HANDLE hMutex;
        DWORD result = -1;
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

DWORD ctx_add_event(context *ctx,
        const wchar_t *Name,
        const wchar_t *StringSecurityDescriptor,
        DWORD Tag,
        HANDLE *pEventHandle)
{
        DWORD result = -1;
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

DWORD ctx_wait_all_unsafe(context *ctx,
        bool Alertable)
{
        return WaitForMultipleObjectsEx(ctx->count,
                ctx->handles,
                TRUE,
                INFINITE,
                Alertable);
}

bool ctx_wait_all(context *ctx,
        bool Alertable,
        DWORD *pResult)
{
        int ret;
        DWORD count;
        HANDLE handles[MAXIMUM_WAIT_OBJECTS];

        EnterCriticalSection(&ctx->cs);
        count = ctx->count;
        ret = memcpy_s(handles, sizeof(handles), ctx->handles, count * (sizeof *handles));
        LeaveCriticalSection(&ctx->cs);

        if ( ret )
                return false;

        *pResult = WaitForMultipleObjectsEx(count,
                handles,
                TRUE,
                INFINITE,
                Alertable);
        return true;
}

DWORD ctx_wait_any_unsafe(context *ctx,
        bool Alertable)
{
        return WaitForMultipleObjectsEx(ctx->count,
                ctx->handles,
                FALSE,
                INFINITE,
                Alertable);
}

DWORD ctx_wait_any(context *ctx,
        bool Alertable,
        DWORD *pResult,
        HANDLE *pHandle,
        DWORD *pTag)
{
        DWORD count;
        HANDLE handles[MAXIMUM_WAIT_OBJECTS];
        DWORD tags[MAXIMUM_WAIT_OBJECTS];
        int ret;
        DWORD result;
        DWORD index;

        EnterCriticalSection(&ctx->cs);
        // make copies of the struct members on the stack before waiting, because
        // WaitForMultipleObjects(Ex) doesn't like it when you modify the contents of
        // the array it is waiting on.
        count = ctx->count;
        ret = memcpy_s(handles, sizeof(handles), ctx->handles, count * (sizeof *handles));
        if ( !ret )
                ret = memcpy_s(tags, sizeof(tags), ctx->tags, count * (sizeof *tags));
        LeaveCriticalSection(&ctx->cs);

        if ( ret )
                return -1;

        result = WaitForMultipleObjectsEx(count, handles, FALSE, INFINITE, Alertable);

        if ( result >= WAIT_OBJECT_0 || result < WAIT_OBJECT_0 + count )
                index = result - WAIT_OBJECT_0;
        else if ( result >= WAIT_ABANDONED_0 || result < WAIT_ABANDONED_0 + count )
                index = result - WAIT_ABANDONED_0;

        if ( pHandle )
                *pHandle = handles[index];
        if ( pTag )
                *pTag = tags[index];

        *pResult = result;
        return count;
}

bool ctx_close_and_remove_handle(context *ctx, HANDLE Handle)
{
        bool result = false;
        DWORD index;

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
        DWORD Tag)
{
        bool result = false;
        HANDLE hSrcProcess;
        HANDLE hTarget;
        DWORD index;

        hSrcProcess = GetCurrentProcess();

        if ( !DuplicateHandle(hSrcProcess, pSrc->mutex, hProcess, &pDst->mutex, SYNCHRONIZE, FALSE, 0) )
                return false;
        pDst->count++;

        if ( !DuplicateHandle(hSrcProcess, pSrc->uevent, hProcess, &pDst->uevent, SYNCHRONIZE, FALSE, 0) ) {
close_mutex:
                CloseHandle(pDst->mutex);
                pDst->mutex = INVALID_HANDLE_VALUE;
                pDst->count = 0;
                return false;
        }
        pDst->count++;
        if ( !DuplicateHandle(hSrcProcess, Handle, hProcess, &hTarget, 0, FALSE, DesiredAccess)
                || (index = ctx_add_handle(pDst, hTarget, Tag)) == -1 ) {

                CloseHandle(pDst->uevent);
                pDst->uevent = INVALID_HANDLE_VALUE;
                goto close_mutex;

        }
        pDst->mutex_tag = index;
        return true;
}
