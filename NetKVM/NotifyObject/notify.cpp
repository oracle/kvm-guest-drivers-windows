//+---------------------------------------------------------------------------
//
//  Microsoft Windows
//  Copyright (C) Microsoft Corporation, 1992-2001.
//
//  File:       N O T I F Y . C P P
//
//  Contents:   Sample notify object code
//  
//  Notes:
//
//  Author:     Alok Sinha

//----------------------------------------------------------------------------

#include "notify.h"

CMuxNotify::CMuxNotify(VOID)
{
}

CMuxNotify::~CMuxNotify(VOID)
{
}

//----------------------------------------------------------------------------
// INetCfgComponentControl
//----------------------------------------------------------------------------

STDMETHODIMP CMuxNotify::Initialize(INetCfgComponent *pNetCfgCom,
                                    INetCfg          *pNetCfg,
                                    BOOL              fInstalling)
{
    UNREFERENCED_PARAMETER(pNetCfgCom);
    UNREFERENCED_PARAMETER(pNetCfg);
    UNREFERENCED_PARAMETER(fInstalling);

    return S_OK;
}

STDMETHODIMP CMuxNotify::CancelChanges(VOID)
{
    return S_OK;
}

STDMETHODIMP CMuxNotify::ApplyRegistryChanges(VOID)
{
    return S_OK;
}

STDMETHODIMP CMuxNotify::ApplyPnpChanges(
                         INetCfgPnpReconfigCallback* pfCallback)
{
    UNREFERENCED_PARAMETER(pfCallback);

    return S_OK;
}

//----------------------------------------------------------------------------
// INetCfgComponentSetup
//----------------------------------------------------------------------------

STDMETHODIMP CMuxNotify::Install(DWORD dwSetupFlags)
{
    UNREFERENCED_PARAMETER(dwSetupFlags);

    return S_OK;
}

STDMETHODIMP CMuxNotify::Upgrade(IN DWORD dwSetupFlags,
                                 IN DWORD dwUpgradeFromBuildNo)
{
    UNREFERENCED_PARAMETER(dwSetupFlags);
    UNREFERENCED_PARAMETER(dwUpgradeFromBuildNo);

    return S_OK;
}

STDMETHODIMP CMuxNotify::ReadAnswerFile(PCWSTR pszAnswerFile,
                                        PCWSTR pszAnswerSection)
{
    UNREFERENCED_PARAMETER(pszAnswerFile);
    UNREFERENCED_PARAMETER(pszAnswerSection);

    return S_OK;
}

STDMETHODIMP CMuxNotify::Removing(VOID)
{
    return S_OK;
}

//----------------------------------------------------------------------------
// INetCfgComponentNotifyBinding
//----------------------------------------------------------------------------

STDMETHODIMP CMuxNotify::QueryBindingPath(IN DWORD dwChangeFlag,
                                          IN INetCfgBindingPath *pNetCfgBindPath)
{
    UNREFERENCED_PARAMETER(dwChangeFlag);
    UNREFERENCED_PARAMETER(pNetCfgBindPath);

    return S_OK;
}

STDMETHODIMP CMuxNotify::NotifyBindingPath (IN DWORD dwChangeFlag,  
                                            IN INetCfgBindingPath *pncbp)
{
    INetCfgComponent     *pnccLower;
    INetCfgComponent     *pnccUpper;
    LPWSTR               pszwInfIdLower;
    LPWSTR               pszwInfIdUpper;
    DWORD                dwCharcteristics;
    HRESULT              hr = S_OK;

    TraceMsg( L"-->CMuxNotify INetCfgNotifyBinding::NotifyBindingPath.\n" );

     //
     // We are only interested to know 1) when a component is installed
     // and we are binding to it i.e. dwChangeFlag = NCN_ADD | NCN_ENABLE
     // and 2) when a component is removed to which we are bound i.e.
     // dwChangeFlag = NCN_REMOVE | NCN_ENABLE. dwChangeFlag is never
     // set to NCN_ADD or NCN_REMOVE only. So, checking for NCN_ENABLE
     // covers the case of NCN_ADD | NCN_ENABLE and checking for NCN_REMOVE
     // covers the case of NCN_REMOVE | NCN_ENABLE. We don't care about
     // NCN_ADD | NCN_DISABLE (case 1) and NCN_REMOVE | NCN_DISABLE (case 2).
     //

     if ( dwChangeFlag & (NCN_ENABLE | NCN_REMOVE) ) {

        //
        // Get the upper and lower components.
        //

        hr = HrGetUpperAndLower( pncbp,
                                 &pnccUpper,
                                 &pnccLower );

        if ( hr == S_OK ) {

            hr = pnccLower->GetCharacteristics( &dwCharcteristics );

            if ( hr == S_OK ) {
                hr = pnccLower->GetId( &pszwInfIdLower );

                if ( hr == S_OK ) {
                    hr = pnccUpper->GetId( &pszwInfIdUpper );

                    if ( hr == S_OK ) {

                        //
                        // We are interested only in binding to a
                        // physical ethernet adapters.
                        // 

                        if ( dwCharcteristics & NCF_PHYSICAL ) {

                            if ( !_wcsicmp( pszwInfIdUpper, c_szMuxProtocol ) ) {

                                if ( dwChangeFlag & NCN_ADD ) {
                                    //This code is for MUX N:1 model, comment it to pass compile
                                    //but leave it here for reference.
                                    //hr = HrAddAdapter( pnccLower );
                                    if( hr != S_OK ){
                                        // you may do something
                                    }
                                    //m_eApplyAction = eActAdd;

                                } else if ( dwChangeFlag & NCN_REMOVE ) {
                                    //This code is for MUX N:1 model, comment it to pass compile
                                    //but leave it here for reference.
                                    //hr = HrRemoveAdapter( pnccLower );
                                    if( hr != S_OK ){
                                        // you may do something
                                    }
                                    //m_eApplyAction = eActRemove;
                                }
                            }
                        } // Physical Adapters. 
                        else if (dwCharcteristics & NCF_VIRTUAL) {

                        }

                        CoTaskMemFree( pszwInfIdUpper );

                    } // Got the upper component id.

                    CoTaskMemFree( pszwInfIdLower );

                } // Got the lower component id.

            } // Got NIC's characteristics

            ReleaseObj(pnccLower);
            ReleaseObj(pnccUpper);

        } // Got the upper and lower components.

    } 

    TraceMsg( L"<--CMuxNotify INetCfgNotifyBinding::NotifyBindingPath(HRESULT = %x).\n",
            S_OK );

    return S_OK;
}

//----------------------------------------------------------------------------
// INetCfgComponentNotifyGlobal
//----------------------------------------------------------------------------

STDMETHODIMP CMuxNotify::GetSupportedNotifications (
                                             OUT DWORD* pdwNotificationFlag)
{
    TraceMsg( L"-->CMuxNotify INetCfgNotifyGlobal::GetSupportedNotifications.\n" );

    *pdwNotificationFlag = NCN_NET | NCN_NETTRANS | NCN_ADD | NCN_REMOVE |
                           NCN_BINDING_PATH | NCN_ENABLE | NCN_DISABLE;

    TraceMsg( L"<--CMuxNotify INetCfgNotifyGlobal::GetSupportedNotifications(HRESULT = %x).\n",
            S_OK );

    return S_OK;
}

STDMETHODIMP CMuxNotify::SysQueryBindingPath (DWORD dwChangeFlag,
                                              INetCfgBindingPath* pncbp)
{
    INetCfgComponent     *pnccLower;
    INetCfgComponent     *pnccUpper;
    LPWSTR               pszwInfIdLower;
    LPWSTR               pszwInfIdUpper;
    DWORD                dwCharcteristics;
    HRESULT              hr = S_OK;


    TraceMsg( L"-->CMuxNotify INetCfgNotifyGlobal::SysQueryBindingPath.\n" );

    if ( dwChangeFlag & NCN_ENABLE ) {

        //
        // Get the upper and lower components.
        //

        hr = HrGetUpperAndLower( pncbp,
                                 &pnccUpper,
                                 &pnccLower );

        if ( hr == S_OK ) {
            hr = pnccLower->GetCharacteristics( &dwCharcteristics );

            if ( hr == S_OK ) {
                hr = pnccLower->GetId( &pszwInfIdLower );

                if ( hr == S_OK ) {
                    hr = pnccUpper->GetId( &pszwInfIdUpper );

                    if ( hr == S_OK ) {

                        //
                        // We are interested only in bindings to physical 
                        // ethernet adapters.
                        // 

                        if ( dwCharcteristics & NCF_PHYSICAL ) {

#ifdef DISABLE_PROTOCOLS_TO_PHYSICAL

                            //
                            // If it not our protocol binding to the
                            // physical adapter then, disable the
                            // binding.
                            //

                            if (_wcsicmp( pszwInfIdUpper, c_szMuxProtocol ) ) {

                                TraceMsg( L"   Disabling the binding between %s "
                                          L"and %s.\n",
                                          pszwInfIdUpper,
                                          pszwInfIdLower );

                                hr = NETCFG_S_DISABLE_QUERY;
                            }
#endif

                        } // Physical Adapters. 
                        else {
                            if (dwCharcteristics & NCF_VIRTUAL) {

                                // If the lower component is our miniport
                                // and the upper component is our protocol
                                // then also, disable the binding.

                                if ( !_wcsicmp(pszwInfIdLower, c_szMuxMiniport) &&
                                     !_wcsicmp(pszwInfIdUpper, c_szMuxProtocol) ) {
                                  
                                    TraceMsg( L"   Disabling the binding between %s "
                                              L"and %s.\n",
                                              pszwInfIdUpper,
                                              pszwInfIdLower );

                                    hr = NETCFG_S_DISABLE_QUERY;
                                }

                            } // Virtual Adapters

                        }

                        CoTaskMemFree( pszwInfIdUpper );

                    } // Got the upper component id.

                    CoTaskMemFree( pszwInfIdLower );

                } // Got the lower component id.

            } // Got NIC's characteristics

            ReleaseObj(pnccLower);
            ReleaseObj(pnccUpper);

        }

    }

    TraceMsg( L"<--CMuxNotify INetCfgNotifyGlobal::SysQueryBindingPath(HRESULT = %x).\n",
            hr );

    return hr;
}

STDMETHODIMP CMuxNotify::SysNotifyBindingPath(DWORD dwChangeFlag,
                                             INetCfgBindingPath* pNetCfgBindPath)
{
    UNREFERENCED_PARAMETER(dwChangeFlag);
    UNREFERENCED_PARAMETER(pNetCfgBindPath);

    return S_OK;
}

STDMETHODIMP CMuxNotify::SysNotifyComponent(DWORD dwChangeFlag,
                                            INetCfgComponent* pNetCfgCom)
{
    UNREFERENCED_PARAMETER(dwChangeFlag);
    UNREFERENCED_PARAMETER(pNetCfgCom);

    return S_OK;
}

//----------------------------------------------------------------------------
//
//  Function:   CMuxNotify::HrGetUpperAndLower
//
//  Purpose:    Get the upper and lower component of the first interface
//              of a binding path.
//
//  Arguments:  
//              IN  pncbp     : Binding path.
//              OUT ppnccUpper: Upper component.
//              OUT ppnccLower: Lower component.
//
//  Returns:    S_OK, or an error.
//
//
//  Notes:
//

HRESULT CMuxNotify::HrGetUpperAndLower (INetCfgBindingPath* pncbp,
                                        INetCfgComponent **ppnccUpper,
                                        INetCfgComponent **ppnccLower)
{
    IEnumNetCfgBindingInterface*    pencbi;
    INetCfgBindingInterface*        pncbi;
    ULONG                           ulCount;
    HRESULT                         hr;

    TraceMsg( L"-->CMuxNotify::HrGetUpperAndLowerComponent.\n" );

    *ppnccUpper = NULL;
    *ppnccLower = NULL;

    hr = pncbp->EnumBindingInterfaces(&pencbi);

    if (S_OK == hr) {
     
        //
        // get the first binding interface
        //

        hr = pencbi->Next(1, &pncbi, &ulCount);

        if ( hr == S_OK ) {

            hr = pncbi->GetUpperComponent( ppnccUpper );

            if ( hr == S_OK ) {

                hr = pncbi->GetLowerComponent ( ppnccLower );
            }
            else {
                if( ppnccUpper != NULL )
                {
                    ReleaseObj( *ppnccUpper );
                }
            }

            ReleaseObj( pncbi );
        }

        ReleaseObj( pencbi );
    }

    TraceMsg( L"<--CMuxNotify::HrGetUpperAndLowerComponent(HRESULT = %x).\n",
            hr );

    return hr;
}

#ifdef DISABLE_PROTOCOLS_TO_PHYSICAL

// ----------------------------------------------------------------------
//
// Function:  CMuxNotify::EnableBindings
//
// Purpose:   Enable/Disable the bindings of other protocols to 
//            the physical adapter.
//
// Arguments:
//            IN pnccAdapter: Pointer to the physical adapter.
//            IN bEnable: TRUE/FALSE to enable/disable respectively.
//
// Returns:   None.
//
// Notes:
//

VOID CMuxNotify::EnableBindings (INetCfgComponent *pnccAdapter,
                                 BOOL bEnable)
{
    IEnumNetCfgBindingPath      *pencbp;
    INetCfgBindingPath          *pncbp;
    HRESULT                     hr;
  
    TraceMsg( L"-->CMuxNotify::EnableBindings.\n" );


    //
    // Get the binding path enumerator.
    //

    hr = HrGetBindingPathEnum( pnccAdapter,
                               EBP_ABOVE,
                               &pencbp );
    if ( hr == S_OK ) {

        hr = HrGetBindingPath( pencbp,
                               &pncbp );

        //
        // Traverse each binding path.
        //

        while( hr == S_OK ) {

            //
            // If our protocol does exist in the binding path then,
            // disable it.
            //

            if ( !IfExistMux(pncbp) ) {

                pncbp->Enable( bEnable );
            }

            ReleaseObj( pncbp );

            hr = HrGetBindingPath( pencbp,
                                   &pncbp );
        }

        ReleaseObj( pencbp );
    }
    else {
        TraceMsg( L"   Couldn't get the binding path enumerator, "
                  L"bindings will not be %s.\n",
                  bEnable ? L"enabled" : L"disabled" );
    }

    TraceMsg( L"<--CMuxNotify::EnableBindings.\n" );

    return;
}

// ----------------------------------------------------------------------
//
// Function:  CMuxNotify::IfExistMux
//
// Purpose:   Determine if a given binding path contains our protocol.
//
// Arguments:
//            IN pncbp: Pointer to the binding path.
//
// Returns:   TRUE if our protocol exists, otherwise FALSE.
//
// Notes:
//

BOOL CMuxNotify::IfExistMux (INetCfgBindingPath *pncbp)
{
    IEnumNetCfgBindingInterface *pencbi;
    INetCfgBindingInterface     *pncbi;
    INetCfgComponent            *pnccUpper;
    LPWSTR                      lpszIdUpper;
    HRESULT                     hr;
    BOOL                        bExist = FALSE;

    TraceMsg( L"-->CMuxNotify::IfExistMux.\n" );

    //
    // Get the binding interface enumerator.
    //

    hr = HrGetBindingInterfaceEnum( pncbp,
                                  &pencbi );

    if ( hr == S_OK ) {

        //
        // Traverse each binding interface.
        //

        hr = HrGetBindingInterface( pencbi,
                                    &pncbi );

        while( !bExist && (hr == S_OK) ) {

            //
            // Is the upper component our protocol?
            //

            hr = pncbi->GetUpperComponent( &pnccUpper );

            if ( hr == S_OK ) {

                hr = pnccUpper->GetId( &lpszIdUpper );

                if ( hr == S_OK ) {

                    bExist = !_wcsicmp( lpszIdUpper, c_szMuxProtocol );

                    CoTaskMemFree( lpszIdUpper );
                }
                else {
                    TraceMsg( L"   Failed to get the upper component of the interface.\n" );
                }

                ReleaseObj( pnccUpper );
            }
            else {
                TraceMsg( L"   Failed to get the upper component of the interface.\n" );
            }

            ReleaseObj( pncbi );

            if ( !bExist ) {
                hr = HrGetBindingInterface( pencbi,
                                            &pncbi );
            }
        }

        ReleaseObj( pencbi );
    }
    else {
        TraceMsg( L"   Couldn't get the binding interface enumerator.\n" );
    }

    TraceMsg( L"<--CMuxNotify::IfExistMux(BOOL = %x).\n",
            bExist );

    return bExist;
}

// ----------------------------------------------------------------------
//
// Function:  CMuxNotify::HrGetBindingPathEnum
//
// Purpose:   Returns the binding path enumerator.
//
// Arguments:
//            IN  pnccAdapter  : Pointer to the physical adapter.
//            IN  dwBindingType: Type of binding path enumerator.
//            OUT ppencbp      : Pointer to the binding path enumerator.
//
// Returns:   S_OK on success, otherwise an error code.
//
// Notes:
//

HRESULT CMuxNotify::HrGetBindingPathEnum (
                                     INetCfgComponent *pnccAdapter,
                                     DWORD dwBindingType,
                                     IEnumNetCfgBindingPath **ppencbp)
{
    INetCfgComponentBindings *pnccb = NULL;
    HRESULT                  hr;

    *ppencbp = NULL;

    hr = pnccAdapter->QueryInterface( IID_INetCfgComponentBindings,
                               (PVOID *)&pnccb );

    if ( hr == S_OK ) {
        hr = pnccb->EnumBindingPaths( dwBindingType,
                                      ppencbp );

        ReleaseObj( pnccb );
    }

    return hr;
}

// ----------------------------------------------------------------------
//
// Function:  CMuxNotify::HrGetBindingPath
//
// Purpose:   Returns a binding path.
//
// Arguments:
//            IN  pencbp  : Pointer to the binding path enumerator.
//            OUT ppncbp  : Pointer to the binding path.
//
// Returns:   S_OK on success, otherwise an error code.
//
// Notes:
//

HRESULT CMuxNotify::HrGetBindingPath (IEnumNetCfgBindingPath *pencbp,
                                      INetCfgBindingPath **ppncbp)
{
    ULONG   ulCount;
    HRESULT hr;

    *ppncbp = NULL;

    hr = pencbp->Next( 1,
                       ppncbp,
                       &ulCount );

    return hr;
}

// ----------------------------------------------------------------------
//
// Function:  CMuxNotify::HrGetBindingInterfaceEnum
//
// Purpose:   Returns the binding interface enumerator.
//
// Arguments:
//            IN  pncbp  : Pointer to the binding path.
//            OUT ppencbi: Pointer to the binding path enumerator.
//
// Returns:   S_OK on success, otherwise an error code.
//
// Notes:
//

HRESULT CMuxNotify::HrGetBindingInterfaceEnum (
                                     INetCfgBindingPath *pncbp,
                                     IEnumNetCfgBindingInterface **ppencbi)
{
    HRESULT hr;

    *ppencbi = NULL;

    hr = pncbp->EnumBindingInterfaces( ppencbi );

    return hr;
}

// ----------------------------------------------------------------------
//
// Function:  CMuxNotify::HrGetBindingInterface
//
// Purpose:   Returns a binding interface.
//
// Arguments:
//            IN  pencbi  : Pointer to the binding interface enumerator.
//            OUT ppncbi  : Pointer to the binding interface.
//
// Returns:   S_OK on success, otherwise an error code.
//
// Notes:
//

HRESULT CMuxNotify::HrGetBindingInterface (
                                     IEnumNetCfgBindingInterface *pencbi,
                                     INetCfgBindingInterface **ppncbi)
{
    ULONG   ulCount;
    HRESULT hr;

    *ppncbi = NULL;

    hr = pencbi->Next( 1,
                       ppncbi,
                       &ulCount );

    return hr;
}

#endif

