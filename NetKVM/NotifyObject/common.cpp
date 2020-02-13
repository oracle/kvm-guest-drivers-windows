/*
 * Copyright (C) Microsoft Corporation, 1992-2001.
 * Copyright (c) 2020 Oracle and/or its affiliates.
 *
 * Author:     Alok Sinha
 *             Annie Li     Reworked for 2-netdev SRIOV live migration
 */

#include <windows.h>
#include <stdio.h>
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
