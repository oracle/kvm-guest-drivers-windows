//+---------------------------------------------------------------------------
//
//  Microsoft Windows
//  Copyright (C) Microsoft Corporation, 1992-2001.
//
//  File:       COMMON.CPP
//
//  Contents:   Debug & Utility functions
//
//  Notes:
//
//  Author:     Alok Sinha
//
//----------------------------------------------------------------------------

#include <windows.h>
#include <stdio.h>
#include <netcfgn.h>
#include "common.h"

#ifdef DBG

void
TraceMsg (
    _In_ LPWSTR szFormat,
    ...)
{
    static WCHAR szTempBuf[4096];

    va_list arglist;

    va_start(arglist, szFormat);

    StringCchVPrintfW ( szTempBuf,
        celems(szTempBuf),
        szFormat,
        arglist );

    OutputDebugStringW( szTempBuf );

    va_end(arglist);
}
#endif