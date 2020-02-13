/*
 * Copyright (C) Microsoft Corporation, 1992-2001.
 * Copyright (c) 2020 Oracle and/or its affiliates.
 *
 * Author:     Alok Sinha
 *             Annie Li     Reworked for 2-netdev SRIOV live migration
 */

#ifndef NOTIFY_H_INCLUDE
#define NOTIFY_H_INCLUDE

#include <windows.h>
#include <atlbase.h>
extern CComModule _Module;  // required by atlcom.h
#include <atlcom.h>
#include <devguid.h>
#include <setupapi.h>
#include <notifyn.h>
#include <assert.h>
#include "resource.h"
#include "common.h"

#define  VFDEV          0x0001
#define  VIOPRO         0x0010

#define SET_FLAGS(Flag, Val)      (Flag) = ((Flag) | (Val))
#define TEST_FLAGS(Flag, Val)     ((Flag) & (Val))

class CNotifyObject :
               public CComObjectRoot,
               public CComCoClass<CNotifyObject, &CLSID_CNotifyObject>,
               public INetCfgComponentControl,
               public INetCfgComponentSetup,
               public INetCfgComponentNotifyBinding,
               public INetCfgComponentNotifyGlobal
{
   public:
      CNotifyObject(VOID);
      ~CNotifyObject(VOID);

      BEGIN_COM_MAP(CNotifyObject)
         COM_INTERFACE_ENTRY(INetCfgComponentControl)
         COM_INTERFACE_ENTRY(INetCfgComponentSetup)
         COM_INTERFACE_ENTRY(INetCfgComponentNotifyBinding)
         COM_INTERFACE_ENTRY(INetCfgComponentNotifyGlobal)
      END_COM_MAP()

      DECLARE_REGISTRY_RESOURCEID(IDR_REG_SAMPLE_NOTIFY)

      //INetCfgComponentControl
      STDMETHOD (Initialize)(IN INetCfgComponent *pNetCfgCom,
                             IN INetCfg          *pNetCfg,
                             IN BOOL              fInstalling);
      STDMETHOD (CancelChanges)();
      STDMETHOD (ApplyRegistryChanges)();
      STDMETHOD (ApplyPnpChanges)(IN INetCfgPnpReconfigCallback* pICallback);

      //INetCfgComponentSetup
      STDMETHOD (Install)(IN DWORD dwSetupFlags);
      STDMETHOD (Upgrade)(IN DWORD dwSetupFlags,
                          IN DWORD dwUpgradeFromBuildNo);
      STDMETHOD (ReadAnswerFile)(IN PCWSTR szAnswerFile,
                                 IN PCWSTR szAnswerSections);
      STDMETHOD (Removing)();

      //INetCfgNotifyBinding
      STDMETHOD (QueryBindingPath)(IN DWORD dwChangeFlag,
                                   IN INetCfgBindingPath *pNetCfgBindPath);
      STDMETHOD (NotifyBindingPath)(IN DWORD dwChangeFlag,
                                    IN INetCfgBindingPath *pNetCfgBindPath);

      //INetCfgNotifyGlobal
      STDMETHOD (GetSupportedNotifications)(OUT DWORD *pdwNotificationFlag );
      STDMETHOD (SysQueryBindingPath)(IN DWORD dwChangeFlag,
                                      IN INetCfgBindingPath *pNetCfgBindPath);
      STDMETHOD (SysNotifyBindingPath)(IN DWORD dwChangeFlag,
                                       IN INetCfgBindingPath *pNetCfgBindPath);
      STDMETHOD (SysNotifyComponent)(IN DWORD dwChangeFlag,
                                     IN INetCfgComponent *pNetCfgCom);

  private:
      INT CheckProtocolandDevInf(INetCfgBindingPath  *pNetCfgBindingPath,
                                 INetCfgComponent   **ppUpNetCfgCom,
                                 INetCfgComponent   **ppLowNetCfgCom);
      HRESULT EnableVFBindings(INetCfgComponent *pNetCfgCom,
                               BOOL bEnable);
      VOID EnableBinding(INetCfgBindingPath *pNetCfgBindPath,
                         BOOL bEnable);
};
#endif // NOTIFY_H_INCLUDE
