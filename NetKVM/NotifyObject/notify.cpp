/*
 * Copyright (C) Microsoft Corporation, 1992-2001.
 * Copyright (c) 2020 Oracle and/or its affiliates.
 *
 * Author:     Alok Sinha
 *             Annie Li     Reworked for 2-netdev SRIOV live migration
 */

#include "notify.h"

CNotifyObject::CNotifyObject(VOID)
{
}

CNotifyObject::~CNotifyObject(VOID)
{
}

//----------------------------------------------------------------------------
// INetCfgComponentControl
//----------------------------------------------------------------------------

STDMETHODIMP CNotifyObject::Initialize(INetCfgComponent *pNetCfgCom,
                                       INetCfg          *pNetCfg,
                                       BOOL              fInstalling)
{
    UNREFERENCED_PARAMETER(pNetCfgCom);
    UNREFERENCED_PARAMETER(pNetCfg);
    UNREFERENCED_PARAMETER(fInstalling);

    return S_OK;
}

STDMETHODIMP CNotifyObject::CancelChanges(VOID)
{
    return S_OK;
}

STDMETHODIMP CNotifyObject::ApplyRegistryChanges(VOID)
{
    return S_OK;
}

STDMETHODIMP CNotifyObject::ApplyPnpChanges(
                            INetCfgPnpReconfigCallback* pfCallback)
{
    UNREFERENCED_PARAMETER(pfCallback);

    return S_OK;
}

//----------------------------------------------------------------------------
// INetCfgComponentSetup
//----------------------------------------------------------------------------

STDMETHODIMP CNotifyObject::Install(DWORD dwSetupFlags)
{
    UNREFERENCED_PARAMETER(dwSetupFlags);

    return S_OK;
}

STDMETHODIMP CNotifyObject::Upgrade(IN DWORD dwSetupFlags,
                                    IN DWORD dwUpgradeFromBuildNo)
{
    UNREFERENCED_PARAMETER(dwSetupFlags);
    UNREFERENCED_PARAMETER(dwUpgradeFromBuildNo);

    return S_OK;
}

STDMETHODIMP CNotifyObject::ReadAnswerFile(PCWSTR pszAnswerFile,
                                           PCWSTR pszAnswerSection)
{
    UNREFERENCED_PARAMETER(pszAnswerFile);
    UNREFERENCED_PARAMETER(pszAnswerSection);

    return S_OK;
}

STDMETHODIMP CNotifyObject::Removing(VOID)
{
    return S_OK;
}

//----------------------------------------------------------------------------
// INetCfgComponentNotifyBinding
//----------------------------------------------------------------------------

STDMETHODIMP CNotifyObject::QueryBindingPath(IN DWORD dwChangeFlag,
                                             IN INetCfgBindingPath *pNetCfgBindPath)
{
    UNREFERENCED_PARAMETER(dwChangeFlag);
    UNREFERENCED_PARAMETER(pNetCfgBindPath);

    return S_OK;
}

/*
 * Check whether the binding is the one we need - the upper is the VirtIO
 * protocol and the lower is the VF miniport driver.
 * Return value - iRet, its bitmap(VFDEV/VIOPRO) represents existence
 * of the VirtIO protocol and VF miniport in the binding.
 */
INT CNotifyObject::CheckProtocolandDevInf(INetCfgBindingPath *pNetCfgBindingPath,
                                          INetCfgComponent  **ppUpNetCfgCom,
                                          INetCfgComponent  **ppLowNetCfgCom
)
{
    IEnumNetCfgBindingInterface    *pEnumNetCfgBindIf;
    INetCfgBindingInterface        *pNetCfgBindIf;
    ULONG                           ulNum;
    LPWSTR                          pszwLowInfId;
    LPWSTR                          pszwUpInfId;
    INT                             iRet = 0;
    INT                             i, nArraySize;

    TraceMsg(L"-->CNotifyObject::%s bRet %d.\n", __FUNCTION__, iRet);

    if (S_OK != pNetCfgBindingPath->EnumBindingInterfaces(&pEnumNetCfgBindIf))
        goto end4;

    if (S_OK != pEnumNetCfgBindIf->Next(1, &pNetCfgBindIf, &ulNum))
        goto end3;

    if (S_OK != pNetCfgBindIf->GetUpperComponent(ppUpNetCfgCom))
        goto end2;

    if (S_OK != pNetCfgBindIf->GetLowerComponent(ppLowNetCfgCom))
        goto end2;

    if (S_OK != (*ppUpNetCfgCom)->GetId(&pszwUpInfId))
        goto end2;

    if (S_OK != (*ppLowNetCfgCom)->GetId(&pszwLowInfId))
        goto end1;

    // Upper is VIO protocol
    if (!_wcsicmp(pszwUpInfId, c_szwKvmProtocol))
    {
         SET_FLAGS(iRet, VIOPRO);
         // Lower is VF device miniport
         nArraySize = sizeof(c_szwVFNetDevId) / sizeof(c_szwVFNetDevId[0]);
         for (i = 0; i < nArraySize; i++)
         {
             if (wcsstr(pszwLowInfId, c_szwVFNetDevId[i]))
             {
                 SET_FLAGS(iRet, VFDEV);
                 break;
             }
         }
         TraceMsg(L" pszwUpInfId  %s and pszwLowInfId %s iRet %d\n",
                  pszwUpInfId, pszwLowInfId, iRet);
    }

    CoTaskMemFree(pszwLowInfId);
end1:
    CoTaskMemFree(pszwUpInfId);
end2:
    ReleaseObj(pNetCfgBindIf);
end3:
    ReleaseObj(pEnumNetCfgBindIf);
end4:
    TraceMsg(L"<--CNotifyObject::%s iRet %d\n", __FUNCTION__, iRet);
    return iRet;
}

/*
 * When the new binding(VirtIO Protocol<->VF Miniport) is added, enumerate
 * all other bindings(non VirtIO Protocol<->VF Miniport) and disable them.
 * When the a binding(VirtIO Protocol<->VF Miniport) is removed, enumerate
 * all other bindings(non VirtIO Protocol<->VF Miniport) and enable them.
 */
STDMETHODIMP CNotifyObject::NotifyBindingPath(IN DWORD dwChangeFlag,
                                              IN INetCfgBindingPath *pNetCfgBindPath)
{
    INetCfgComponent     *pUpNetCfgCom;
    INetCfgComponent     *pLowNetCfgCom;
    BOOL                  bAdd, bRemove;
    INT                   iRet = 0;

    pUpNetCfgCom = NULL;
    pLowNetCfgCom = NULL;

    TraceMsg( L"-->CNotifyObject INetCfgNotifyBinding::%s \n", __FUNCTION__);

    bAdd = dwChangeFlag & NCN_ADD;
    bRemove = dwChangeFlag & NCN_REMOVE;
    assert(!(bAdd && bRemove));

    // Check and operate when binding is being added or removed
    if (bAdd || bRemove) {
        iRet = CheckProtocolandDevInf(pNetCfgBindPath,
                                      &pUpNetCfgCom, &pLowNetCfgCom);
        if (TEST_FLAGS(iRet, VFDEV) && TEST_FLAGS(iRet, VIOPRO)) {
            if (bAdd) {
                // Enumerate and disable other bindings except for
                // VIOprotocol<->VF miniport
                if (EnableVFBindings(pLowNetCfgCom, FALSE))
                    TraceMsg(L"Failed to disable non VIO protocol to VF miniport\n");
            }
            else {
                // Enumerate and enable other bindings except for
                // VIOprotocol<->VF miniport
                if (EnableVFBindings(pLowNetCfgCom, TRUE))
                    TraceMsg(L"Failed to enable non VIO protocol to VF miniport\n");
            }
        }
        if (pUpNetCfgCom != NULL)
        {
            ReleaseObj(pUpNetCfgCom);
        }
        if (pLowNetCfgCom != NULL)
        {
            ReleaseObj(pLowNetCfgCom);
        }
    }
    TraceMsg(L"<--CNotifyObject INetCfgNotifyBinding::%s \n", __FUNCTION__);

    return S_OK;
}

//----------------------------------------------------------------------------
// INetCfgComponentNotifyGlobal
//----------------------------------------------------------------------------

STDMETHODIMP CNotifyObject::GetSupportedNotifications(
                                  OUT DWORD *pdwNotificationFlag)
{
    TraceMsg( L"-->CNotifyObject INetCfgNotifyGlobal::%s\n", __FUNCTION__);

    *pdwNotificationFlag = NCN_NET | NCN_NETTRANS | NCN_ADD | NCN_REMOVE |
                           NCN_BINDING_PATH | NCN_ENABLE | NCN_DISABLE;

    TraceMsg( L"<--CNotifyObject INetCfgNotifyGlobal::%s\n", __FUNCTION__);

    return S_OK;
}

//When addition of a binding path is about to occur,
//disable it if it is VirtIO protocol<->non VF miniport binding.
STDMETHODIMP CNotifyObject::SysQueryBindingPath(DWORD dwChangeFlag,
                                                INetCfgBindingPath *pNetCfgBindPath)
{
    INetCfgComponent     *pUpNetCfgCom;
    INetCfgComponent     *pLowNetCfgCom;
    INT                  iRet = 0;
    HRESULT              hResult = S_OK;

    pUpNetCfgCom = NULL;
    pLowNetCfgCom = NULL;
    TraceMsg(L"-->CNotifyObject INetCfgNotifyGlobal::%s\n", __FUNCTION__);

    if (dwChangeFlag & (NCN_ENABLE | NCN_ADD)) {
        iRet = CheckProtocolandDevInf(pNetCfgBindPath,
                                      &pUpNetCfgCom, &pLowNetCfgCom);
        if (!TEST_FLAGS(iRet, VFDEV) && TEST_FLAGS(iRet, VIOPRO)) {
            // Upper protocol is virtio protocol and lower id is not
            // vf device id, disable the binding.
            hResult = NETCFG_S_DISABLE_QUERY;
        }
        if (pUpNetCfgCom != NULL)
        {
            ReleaseObj(pUpNetCfgCom);
        }
        if (pLowNetCfgCom != NULL)
        {
            ReleaseObj(pLowNetCfgCom);
        }
    }
    TraceMsg(L"<--CNotifyObject INetCfgNotifyGlobal::%s HRESULT = %x\n",
             __FUNCTION__, hResult);

    return hResult;
}

STDMETHODIMP CNotifyObject::SysNotifyBindingPath(DWORD dwChangeFlag,
                                                 INetCfgBindingPath* pNetCfgBindPath)
{
    UNREFERENCED_PARAMETER(dwChangeFlag);
    UNREFERENCED_PARAMETER(pNetCfgBindPath);

    return S_OK;
}

STDMETHODIMP CNotifyObject::SysNotifyComponent(DWORD dwChangeFlag,
                                               INetCfgComponent* pNetCfgCom)
{
    UNREFERENCED_PARAMETER(dwChangeFlag);
    UNREFERENCED_PARAMETER(pNetCfgCom);

    return S_OK;
}

//Enable/Disable the bindings of non VIO protocols to the VF miniport
HRESULT CNotifyObject::EnableVFBindings(INetCfgComponent *pNetCfgCom,
                                        BOOL bEnable)
{
    IEnumNetCfgBindingPath      *pEnumNetCfgBindPath;
    INetCfgBindingPath          *pNetCfgBindPath;
    INetCfgComponentBindings    *pNetCfgComBind;
    HRESULT                     hResult;
    ULONG                       ulNum;

    TraceMsg(L"-->CNotifyObject::%s bEnable = %d \n", __FUNCTION__, bEnable);

    // Get the binding path enumerator.
    pEnumNetCfgBindPath = NULL;
    pNetCfgComBind = NULL;
    hResult = pNetCfgCom->QueryInterface(IID_INetCfgComponentBindings,
                                         (PVOID *)&pNetCfgComBind);
    if (S_OK == hResult) {
        hResult = pNetCfgComBind->EnumBindingPaths(EBP_ABOVE,
                                                   &pEnumNetCfgBindPath);
        ReleaseObj(pNetCfgComBind);
    }
    else
        return hResult;

    if (hResult == S_OK) {
        pNetCfgBindPath = NULL;
        hResult = pEnumNetCfgBindPath->Next(1, &pNetCfgBindPath, &ulNum);
        // Enumerate every binding path.
        while (hResult == S_OK) {
            EnableBinding(pNetCfgBindPath, bEnable);
            ReleaseObj(pNetCfgBindPath);
            pNetCfgBindPath = NULL;
            hResult = pEnumNetCfgBindPath->Next(1, &pNetCfgBindPath, &ulNum);
        }
        ReleaseObj(pEnumNetCfgBindPath);
    }
    else {
        TraceMsg(L"Failed to get the binding path enumerator, "
                 L"bindings will not be %s.\n",
                 bEnable ? L"enabled" : L"disabled");
    }

    TraceMsg(L"<--CNotifyObject::%s\n", __FUNCTION__);

    return hResult;
}

//Enable or disable bindings with non VIO protocol
VOID CNotifyObject::EnableBinding(INetCfgBindingPath *pNetCfgBindPath,
                                  BOOL bEnable)
{
    INetCfgComponent     *pUpNetCfgCom;
    INetCfgComponent     *pLowNetCfgCom;
    INT                   iRet;

    pUpNetCfgCom = NULL;
    pLowNetCfgCom = NULL;

    iRet = CheckProtocolandDevInf(pNetCfgBindPath,
                                  &pUpNetCfgCom, &pLowNetCfgCom);
    if (!TEST_FLAGS(iRet, VIOPRO))
        pNetCfgBindPath->Enable(bEnable);
    if (pUpNetCfgCom != NULL)
        ReleaseObj(pUpNetCfgCom);
    if (pLowNetCfgCom != NULL)
        ReleaseObj(pLowNetCfgCom);

    return;
}
