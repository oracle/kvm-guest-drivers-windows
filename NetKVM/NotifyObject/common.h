/*
 * Copyright (C) Microsoft Corporation, 1992-2001.
 * Copyright (c) 2020 Oracle and/or its affiliates.
 *
 * Author:     Alok Sinha
 *             Annie Li     Reworked for 2-netdev SRIOV live migration
 */

#ifndef COMMON_H_INCLUDED

#define COMMON_H_INCLUDED

#include <devguid.h>
#include <strsafe.h>

#define celems(_x)          (sizeof(_x) / sizeof(_x[0]))

/*
 * Intel VF net device ID, VEN&DEV&SUBSYS.
 * NOTE: Right now, prototype only supports Intel VF SR-IOV live
 * migration. New device IDs will be added in future to support VFs
 * from other vendors.
 */
const WCHAR c_szwIntelNetDevId[] = L"ven_8086&dev_1515";

// PnP ID, also referred to as Hardware ID, of the protocol interface.
const WCHAR c_szwKvmProtocol[] = L"vioprot";

#define ReleaseObj( x )  if ( x ) \
                            ((IUnknown*)(x))->Release();
#if DBG
void TraceMsg ( _In_ LPWSTR szFormat, ...);
#else
#define TraceMsg
#endif
#endif // COMMON_H_INCLUDED
