/*
 * Copyright (c) 2000  Microsoft Corporation
 * Copyright (c) 2020 Oracle and/or its affiliates.
 *
 * Author:    Annie Li    Reworked for 2-netdev SRIOV live migration
 */

#include "precomp.h"

#if SRIOV
NDISPROT_GLOBALS    Globals = {0};

#if DBG
INT                 ndisprotDebugLevel = DL_WARN;
#endif

NDIS_STATUS
NdisprotBindAdapter(
    IN NDIS_HANDLE                  ProtocolDriverContext,
    IN NDIS_HANDLE                  BindContext,
    IN PNDIS_BIND_PARAMETERS        BindParameters
    )
{
    PNDISPROT_OPEN_CONTEXT          pOpenContext = NULL;
    NDIS_STATUS                     Status;

    UNREFERENCED_PARAMETER(ProtocolDriverContext);

    Status = NdisAllocateMemoryWithTag((PVOID *)&pOpenContext,
                                       sizeof(NDISPROT_OPEN_CONTEXT),
                                       NPROT_ALLOC_TAG);
    if (Status != NDIS_STATUS_SUCCESS)
    {
        DEBUGP(DL_ERROR,
              ("[%s]: NdisAllocateMemoryWithTag fail with %x\n",
              __FUNCTION__, Status));
        return Status;
    }
    NdisZeroMemory(pOpenContext, sizeof(NDISPROT_OPEN_CONTEXT));
    NPROT_SET_SIGNATURE(pOpenContext, oc);
    NPROT_INIT_LOCK(&pOpenContext->Lock);
    //This is for Binding reference, shutdown binding does Deref
    ndisprotRefOpen(pOpenContext);
    pOpenContext->State = NdisprotInitializing;
    ndisprotRefOpen(pOpenContext);
    Status = ndisprotCreateBinding(pOpenContext,
                                   BindParameters,
                                   BindContext,
                                   (PUCHAR)BindParameters->AdapterName->Buffer,
                                   BindParameters->AdapterName->Length);
    ndisprotDerefOpen(pOpenContext);

    return Status;
}

/*
 * Completion routine called by NDIS if our call to NdisOpenAdapterEx
 * pends. Wake up the thread that called NdisOpenAdapterEx.
 */
VOID
NdisprotOpenAdapterComplete(
    IN NDIS_HANDLE                  ProtocolBindingContext,
    IN NDIS_STATUS                  Status
    )
{
    PNDISPROT_OPEN_CONTEXT           pOpenContext;

    pOpenContext = (PNDISPROT_OPEN_CONTEXT)ProtocolBindingContext;
    NPROT_STRUCT_ASSERT(pOpenContext, oc);
    pOpenContext->BindStatus = Status;
    NPROT_SIGNAL_EVENT(&pOpenContext->BindEvent);
}

NDIS_STATUS
NdisprotUnbindAdapter(
    IN NDIS_HANDLE                  UnbindContext,
    IN NDIS_HANDLE                  ProtocolBindingContext
    )
{
    PNDISPROT_OPEN_CONTEXT           pOpenContext;

    UNREFERENCED_PARAMETER(UnbindContext);

    pOpenContext = (PNDISPROT_OPEN_CONTEXT)ProtocolBindingContext;
    NPROT_STRUCT_ASSERT(pOpenContext, oc);

    NPROT_ACQUIRE_LOCK(&pOpenContext->Lock, FALSE);
    NPROT_SET_FLAGS(pOpenContext->Flags,
                    NPROTO_UNBIND_FLAGS, NPROTO_UNBIND_RECEIVED);
    pOpenContext->State = NdisprotClosing;
    NPROT_RELEASE_LOCK(&pOpenContext->Lock, FALSE);

    ndisprotShutdownBinding(pOpenContext);

    return NDIS_STATUS_SUCCESS;
}

/*
 * Called by NDIS to complete a pended call to NdisCloseAdapter.
 * We wake up the thread waiting for this completion.
 */
VOID
NdisprotCloseAdapterComplete(
    IN NDIS_HANDLE                  ProtocolBindingContext
    )
{
    PNDISPROT_OPEN_CONTEXT           pOpenContext;

    pOpenContext = (PNDISPROT_OPEN_CONTEXT)ProtocolBindingContext;
    NPROT_STRUCT_ASSERT(pOpenContext, oc);
    NPROT_SIGNAL_EVENT(&pOpenContext->BindEvent);
}

/*
 * Search the global list for an upper device context structure
 * that has same mac address with underlying bound device, and
 * return a pointer to it.
 */
PPARANDIS_ADAPTER
ndisprotGetUpperDevice(
    PNDIS_BIND_PARAMETERS BindParameters
)
{
    PPARANDIS_ADAPTER           pContext;
    PLIST_ENTRY                 pEnt;

    pContext = NULL;

    NPROT_ACQUIRE_LOCK(&Globals.GlobalLock, FALSE);
    for(pEnt = Globals.UpAdaptList.Flink;
        pEnt != &Globals.UpAdaptList;
        pEnt = pEnt->Flink)
    {
        pContext = CONTAINING_RECORD(pEnt, PARANDIS_ADAPTER, Link);

        for(int i = 0; i < ETH_HARDWARE_ADDRESS_SIZE; i++)
            DEBUGP(DL_INFO,
                  ("[%s]: MacAddresschar1 0x%x bind MacAddresschar2  0x%x \n",
                   __FUNCTION__,
                   pContext->CurrentMacAddress[i],
                   BindParameters->CurrentMacAddress[i]));

        //Check if this has the mac address we are looking for.
        if (NdisEqualMemory(pContext->CurrentMacAddress,
                            BindParameters->CurrentMacAddress,
                            ETH_HARDWARE_ADDRESS_SIZE))
        {
            DEBUGP(DL_INFO,
                  ("[%s]: Find matched VirtIO device with same mac address\n",
                   __FUNCTION__));
            break;
        }
        pContext = NULL;
    }
    NPROT_RELEASE_LOCK(&Globals.GlobalLock, FALSE);

    return pContext;
}

/*
 * Handle completion of an NDIS request that was originally
 * submitted to VirtIO miniport and was forwarded down
 * to the lower VF binding.
 */
VOID
PtCompleteForwardedRequest(
    IN PNDISPROT_OPEN_CONTEXT       pOpenContext,
    IN PFWD_NDIS_REQUEST            pFwdNdisRequest,
    IN NDIS_STATUS                  Status
)
{
    PPARANDIS_ADAPTER   pContext = NULL;
    PNDIS_OID_REQUEST   pNdisRequest = &pFwdNdisRequest->Request;
    NDIS_OID            Oid;
    PNDIS_OID_REQUEST   OrigRequest = NULL;
    BOOLEAN             bCompleteRequest = FALSE;

    UNREFERENCED_PARAMETER(pOpenContext);
    pContext = (PPARANDIS_ADAPTER)pOpenContext->pContext;

    ASSERT(pContext != NULL);
    ASSERT(pFwdNdisRequest == &pContext->Request);
    if (Status != NDIS_STATUS_SUCCESS)
    {
        DEBUGP(DL_ERROR,
              ("[%s]: pOpenContext %p, OID %x, Status %x\n",
               __FUNCTION__,
               pContext,
               pFwdNdisRequest->Request.DATA.QUERY_INFORMATION.Oid,
               Status));
    }

    pFwdNdisRequest->Refcount--;
    if (pFwdNdisRequest->Refcount == 0)
    {
        bCompleteRequest = TRUE;
        OrigRequest = pFwdNdisRequest->OrigRequest;
        pFwdNdisRequest->OrigRequest = NULL;
    }

    if (!bCompleteRequest)
    {
        return;
    }

    //Complete the original request.
    switch(pNdisRequest->RequestType)
    {
    case NdisRequestQueryInformation:
    case NdisRequestQueryStatistics:
        OrigRequest->DATA.QUERY_INFORMATION.BytesWritten =
            pNdisRequest->DATA.QUERY_INFORMATION.BytesWritten;
        OrigRequest->DATA.QUERY_INFORMATION.BytesNeeded =
            pNdisRequest->DATA.QUERY_INFORMATION.BytesNeeded;

        //Before completing the request, do any necessary post-processing.
        Oid = pNdisRequest->DATA.QUERY_INFORMATION.Oid;
        if (Status == NDIS_STATUS_SUCCESS)
        {
            if (Oid == OID_GEN_LINK_SPEED)
            {
                NdisMoveMemory(&pContext->LinkSpeed,
                    pNdisRequest->DATA.QUERY_INFORMATION.InformationBuffer,
                    sizeof(ULONG));
            }
        }
        break;
    case NdisRequestSetInformation:
        OrigRequest->DATA.SET_INFORMATION.BytesRead =
            pNdisRequest->DATA.SET_INFORMATION.BytesRead;
        OrigRequest->DATA.QUERY_INFORMATION.BytesNeeded =
            pNdisRequest->DATA.SET_INFORMATION.BytesNeeded;

        //Before completing the request, cache relevant information
        //in our structure.
        if (Status == NDIS_STATUS_SUCCESS)
        {
            Oid = pNdisRequest->DATA.SET_INFORMATION.Oid;
            switch(Oid)
            {
            case OID_GEN_CURRENT_LOOKAHEAD:
                NdisMoveMemory(&pContext->LookAhead,
                    pNdisRequest->DATA.SET_INFORMATION.InformationBuffer,
                    sizeof(ULONG));
                break;
            case OID_GEN_CURRENT_PACKET_FILTER:
                DEBUGP(DL_INFO,
                      ("[%s]: packetfilter 0x%x\n",
                       __FUNCTION__,
                       *((PULONG)pNdisRequest->DATA.SET_INFORMATION.InformationBuffer)));
                pOpenContext->PacketFilter =
               *((PULONG)pNdisRequest->DATA.SET_INFORMATION.InformationBuffer);
                break;
            default:
                break;
            }
        }
        break;
    default:
        ASSERT(FALSE);
        break;
    }
    NdisMOidRequestComplete(pContext->MiniportHandle, OrigRequest, Status);
}

NDIS_STATUS
NdisprotPnPEventHandler(
    IN NDIS_HANDLE                  ProtocolBindingContext,
    IN PNET_PNP_EVENT_NOTIFICATION  pNetPnPEventNotification
)
{
    PNDISPROT_OPEN_CONTEXT            pOpenContext;
    NDIS_STATUS                       Status = NDIS_STATUS_SUCCESS;

    pOpenContext = (PNDISPROT_OPEN_CONTEXT)ProtocolBindingContext;
    switch(pNetPnPEventNotification->NetPnPEvent.NetEvent)
    {
    case NetEventSetPower:
        NPROT_STRUCT_ASSERT(pOpenContext, oc);
        DEBUGP(DL_INFO,
              ("[%s]: Open %p, SetPower to %d\n",
               __FUNCTION__, pOpenContext,
               *(PNET_DEVICE_POWER_STATE)pNetPnPEventNotification->NetPnPEvent.Buffer));
        Status = NDIS_STATUS_SUCCESS;
        break;
    case NetEventQueryPower:
    case NetEventBindsComplete:
        Status = NDIS_STATUS_SUCCESS;
        break;
    case NetEventPause:
        NPROT_ACQUIRE_LOCK(&pOpenContext->Lock, FALSE);
        pOpenContext->State = NdisprotPausing;
        while (TRUE)
        {
            if (pOpenContext->PendedSendCount == 0)
                break;

            DEBUGP(DL_INFO,
                  ("[%s]: Open %p, outstanding count is %d\n",
                   __FUNCTION__,
                   pOpenContext,
                   pOpenContext->PendedSendCount));
            NPROT_RELEASE_LOCK(&pOpenContext->Lock, FALSE);
            NdisMSleep(100000);    // 100 ms.
            NPROT_ACQUIRE_LOCK(&pOpenContext->Lock, FALSE);
        }
        pOpenContext->State = NdisprotPaused;
        NPROT_RELEASE_LOCK(&pOpenContext->Lock, FALSE);
        break;
    case NetEventRestart:
        NPROT_ACQUIRE_LOCK(&pOpenContext->Lock, FALSE);
        ASSERT(pOpenContext->State == NdisprotPaused);
        pOpenContext->State = NdisprotRunning;
        NPROT_RELEASE_LOCK(&pOpenContext->Lock, FALSE);
        break;
    case NetEventQueryRemoveDevice:
    case NetEventCancelRemoveDevice:
    case NetEventReconfigure:
    case NetEventBindList:
    case NetEventPnPCapabilities:
        Status = NDIS_STATUS_SUCCESS;
        break;
    default:
        Status = NDIS_STATUS_NOT_SUPPORTED;
        break;
    }
    DEBUGP(DL_INFO,
          ("[%s]: Open %p, Event %d, Status %x\n",
           __FUNCTION__,
           pOpenContext,
           pNetPnPEventNotification->NetPnPEvent.NetEvent, Status));

    return Status;
}

NDIS_STATUS
ndisprotCreateBinding(
    IN PNDISPROT_OPEN_CONTEXT         pOpenContext,
    IN PNDIS_BIND_PARAMETERS          BindParameters,
    IN NDIS_HANDLE                    BindContext,
    IN PUCHAR                         pBindingInfo,
    IN ULONG                          BindingInfoLength
    )
{
    NDIS_STATUS              Status;
    NDIS_MEDIUM              MediumArray[1] = {NdisMedium802_3};
    NDIS_OPEN_PARAMETERS     OpenParameters;
    UINT                     SelectedMediumIndex;
    BOOLEAN                  bOpenComplete = FALSE;
    NET_FRAME_TYPE           FrameTypeArray[2] =
                             {NDIS_ETH_TYPE_802_1X, NDIS_ETH_TYPE_802_1Q};
    PPARANDIS_ADAPTER        pContext;
    ULONG                    PacketFiler;
    ULONG                    BytesWritten;

    DEBUGP(DL_INFO,
          ("[%s]: open %p/%x, device [%s]\n",
           __FUNCTION__,
           pOpenContext,
           pOpenContext->Flags,
           pBindingInfo));

    Status = NDIS_STATUS_SUCCESS;
    do
    {
        NPROT_ACQUIRE_LOCK(&pOpenContext->Lock, FALSE);
        NPROT_SET_FLAGS(pOpenContext->Flags,
                        NPROTO_BIND_FLAGS, NPROTO_BIND_OPENING);
        NPROT_RELEASE_LOCK(&pOpenContext->Lock, FALSE);

        //Copy in the device name. Add room for a NULL terminator.
        pOpenContext->DeviceName.Buffer = NULL;
        NdisAllocateMemoryWithTag((PVOID *)&pOpenContext->DeviceName.Buffer,
                                  BindingInfoLength + sizeof(WCHAR),
                                  NPROT_ALLOC_TAG);
        if (pOpenContext->DeviceName.Buffer == NULL)
        {
            DEBUGP(DL_WARN,
                  ("[%s]: failed to alloc device name buf (%d bytes)\n",
                   __FUNCTION__,
                   BindingInfoLength + sizeof(WCHAR)));
            Status = NDIS_STATUS_RESOURCES;
            break;
        }

        NdisMoveMemory(pOpenContext->DeviceName.Buffer,
                       pBindingInfo,
                       BindingInfoLength);
#pragma prefast(suppress: 12009, "DeviceName length will not cause overflow")
        *(PWCHAR)((PUCHAR)pOpenContext->DeviceName.Buffer + BindingInfoLength) = L'\0';
        NdisInitUnicodeString(&pOpenContext->DeviceName,
                              pOpenContext->DeviceName.Buffer);

        pOpenContext->PowerState = NetDeviceStateD0;
        NPROT_INIT_EVENT(&pOpenContext->BindEvent);

        NdisZeroMemory(&OpenParameters, sizeof(NDIS_OPEN_PARAMETERS));
        OpenParameters.Header.Revision = NDIS_OPEN_PARAMETERS_REVISION_1;
        OpenParameters.Header.Size = sizeof(NDIS_OPEN_PARAMETERS);
        OpenParameters.Header.Type = NDIS_OBJECT_TYPE_OPEN_PARAMETERS;
        OpenParameters.AdapterName = BindParameters->AdapterName;
        OpenParameters.MediumArray = &MediumArray[0];
        OpenParameters.MediumArraySize =
                       sizeof(MediumArray) / sizeof(NDIS_MEDIUM);
        OpenParameters.SelectedMediumIndex = &SelectedMediumIndex;
        OpenParameters.FrameTypeArray = &FrameTypeArray[0];
        OpenParameters.FrameTypeArraySize =
                       sizeof(FrameTypeArray) / sizeof(NET_FRAME_TYPE);

        Status = NdisOpenAdapterEx(Globals.NdisProtocolHandle,
                                   (NDIS_HANDLE)pOpenContext,
                                   &OpenParameters,
                                   BindContext,
                                   &pOpenContext->BindingHandle);

        if (Status == NDIS_STATUS_PENDING)
        {
            NPROT_WAIT_EVENT(&pOpenContext->BindEvent, 0);
            Status = pOpenContext->BindStatus;
        }

        if (Status != NDIS_STATUS_SUCCESS)
        {
            DEBUGP(DL_WARN,
                  ("[%s]: NdisOpenAdapter (%ws) failed: %x\n",
                   __FUNCTION__,
                   pOpenContext->DeviceName.Buffer,
                   Status));
            break;
        }

        pOpenContext->State = NdisprotPaused;
        bOpenComplete = TRUE;

        (VOID)NdisQueryAdapterInstanceName(&pOpenContext->DeviceDescr,
                                           pOpenContext->BindingHandle);

        NdisMoveMemory(&pOpenContext->CurrentAddress[0],
                       BindParameters->CurrentMacAddress,
                       NPROT_MAC_ADDR_LEN);

        //Get upper VirtIO network interface of same mac address,
        //and set its pOpenContext of underlying binding adapter.
        pContext = ndisprotGetUpperDevice(BindParameters);
        if (pContext)
        {
            NdisAcquireSpinLock(&pContext->BindingLock);
            pOpenContext->MiniportAdapterHandle = pContext->MiniportHandle;
            pContext->BindingHandle = pOpenContext->BindingHandle;
            pOpenContext->pContext = pContext;
            pContext->pOpenContext = pOpenContext;
            NdisReleaseSpinLock(&pContext->BindingLock);

            pOpenContext->BindParameters = *BindParameters;
            if (BindParameters->RcvScaleCapabilities)
            {
                pOpenContext->RcvScaleCapabilities =
                              (*BindParameters->RcvScaleCapabilities);
                pOpenContext->BindParameters.RcvScaleCapabilities =
                              &pOpenContext->RcvScaleCapabilities;
            }
            pContext->LookAhead = pOpenContext->BindParameters.LookaheadSize;
            pContext->LinkSpeed = pOpenContext->BindParameters.RcvLinkSpeed;

            DEBUGP(DL_INFO, ("[%s]: PacketFiler %x\n",
                   __FUNCTION__, pContext->PacketFilter));
            //in case pContext->PacketFilter isn't set in VirtIO driver, set
            //VF packetfilter as PARANDIS_PACKET_FILTERS by default.
            PacketFiler = pContext->PacketFilter ?
               pContext->PacketFilter : PARANDIS_PACKET_FILTERS;
            Status = ndisprotDoRequest(
                pOpenContext,
                NDIS_DEFAULT_PORT_NUMBER,
                NdisRequestSetInformation,
                OID_GEN_CURRENT_PACKET_FILTER,
                &PacketFiler,
                sizeof(ULONG),
                &BytesWritten);
            if (Status != NDIS_STATUS_SUCCESS)
            {
                DEBUGP(DL_WARN,
                      ("[%s]: set packet filter failed: %x\n",
                       __FUNCTION__, Status));
                //This failure does not affect the binding
                Status = NDIS_STATUS_SUCCESS;
            }
        }
        else
        {
            DEBUGP(DL_WARN,
             ("[%s]: Fails to get VirtIO context with matched mac address\n",
              __FUNCTION__));
            Status = NDIS_STATUS_FAILURE;
            break;
        }

        NPROT_ACQUIRE_LOCK(&pOpenContext->Lock, FALSE);
        NPROT_SET_FLAGS(pOpenContext->Flags, NPROTO_BIND_FLAGS, NPROTO_BIND_ACTIVE);
        ASSERT(!NPROT_TEST_FLAGS(pOpenContext->Flags,
                                 NPROTO_UNBIND_FLAGS, NPROTO_UNBIND_RECEIVED));
        NPROT_RELEASE_LOCK(&pOpenContext->Lock, FALSE);
    }
    while (FALSE);

    if (Status != NDIS_STATUS_SUCCESS)
    {
        NPROT_ACQUIRE_LOCK(&pOpenContext->Lock, FALSE);
        if (bOpenComplete)
            NPROT_SET_FLAGS(pOpenContext->Flags,
                            NPROTO_BIND_FLAGS, NPROTO_BIND_ACTIVE);
        else if (NPROT_TEST_FLAGS(pOpenContext->Flags,
                                  NPROTO_BIND_FLAGS, NPROTO_BIND_OPENING))
            NPROT_SET_FLAGS(pOpenContext->Flags,
                            NPROTO_BIND_FLAGS, NPROTO_BIND_FAILED);

        NPROT_RELEASE_LOCK(&pOpenContext->Lock, FALSE);

        ndisprotShutdownBinding(pOpenContext);
    }
    DEBUGP(DL_INFO, ("[%s]: OpenContext %p, Status %x\n",
           __FUNCTION__, pOpenContext, Status));

    return Status;
}

VOID
ndisprotShutdownBinding(
    IN PNDISPROT_OPEN_CONTEXT        pOpenContext
    )
{
    NDIS_STATUS             Status;
    BOOLEAN                 DoCloseBinding = FALSE;
    NPROT_EVENT             ClosingEvent;
    PPARANDIS_ADAPTER       pContext =
                            (PPARANDIS_ADAPTER)pOpenContext->pContext;

    do
    {
        NPROT_ACQUIRE_LOCK(&pOpenContext->Lock, FALSE);
        if (NPROT_TEST_FLAGS(pOpenContext->Flags,
                             NPROTO_BIND_FLAGS, NPROTO_BIND_FAILED))
        {
            NPROT_RELEASE_LOCK(&pOpenContext->Lock, FALSE);
            break;
        }

        if (NPROT_TEST_FLAGS(pOpenContext->Flags,
                             NPROTO_BIND_FLAGS, NPROTO_BIND_ACTIVE))
        {
            ASSERT(pOpenContext->ClosingEvent == NULL);
            pOpenContext->ClosingEvent = NULL;
            NPROT_SET_FLAGS(pOpenContext->Flags,
                            NPROTO_BIND_FLAGS, NPROTO_BIND_CLOSING);

            if (pOpenContext->PendedSendCount != 0)
            {
                pOpenContext->ClosingEvent = &ClosingEvent;
                NPROT_INIT_EVENT(&ClosingEvent);
            }

            DoCloseBinding = TRUE;
        }
        NPROT_RELEASE_LOCK(&pOpenContext->Lock, FALSE);

        if (DoCloseBinding)
        {
            ULONG    PacketFilter = 0;
            ULONG    BytesRead = 0;

            //Set Packet filter to 0 before closing the binding
            Status = ndisprotDoRequest(
                        pOpenContext,
                        NDIS_DEFAULT_PORT_NUMBER,
                        NdisRequestSetInformation,
                        OID_GEN_CURRENT_PACKET_FILTER,
                        &PacketFilter,
                        sizeof(PacketFilter),
                        &BytesRead);

            if (Status != NDIS_STATUS_SUCCESS)
            {
                DEBUGP(DL_WARN, ("[%s]: set packet filter failed: %x\n",
                       __FUNCTION__, Status));
            }

            //Set multicast list to null before closing the binding
            Status = ndisprotDoRequest(
                        pOpenContext,
                        NDIS_DEFAULT_PORT_NUMBER,
                        NdisRequestSetInformation,
                        OID_802_3_MULTICAST_LIST,
                        NULL,
                        0,
                        &BytesRead);

            if (Status != NDIS_STATUS_SUCCESS)
            {
                DEBUGP(DL_WARN, ("[%s]: set multicast list failed: %x\n",
                       __FUNCTION__, Status));
            }

            //Wait for any pending sends, requests or receives on
            //the binding to complete.
            ndisprotWaitForPendingIO(pOpenContext);

            NPROT_INIT_EVENT(&pOpenContext->BindEvent);
            DEBUGP(DL_INFO, ("[%s]: Closing OpenContext %p,"
                   " BindingHandle %p\n",
                   __FUNCTION__, pOpenContext, pOpenContext->BindingHandle));
            Status = NdisCloseAdapterEx(pOpenContext->BindingHandle);
            if (Status == NDIS_STATUS_PENDING)
            {
                NPROT_WAIT_EVENT(&pOpenContext->BindEvent, 0);
                Status = pOpenContext->BindStatus;
            }
            NPROT_ASSERT(Status == NDIS_STATUS_SUCCESS);

            NPROT_ACQUIRE_LOCK(&pOpenContext->Lock, FALSE);
            NPROT_SET_FLAGS(pOpenContext->Flags,
                            NPROTO_BIND_FLAGS, NPROTO_BIND_IDLE);
            NPROT_SET_FLAGS(pOpenContext->Flags, NPROTO_UNBIND_FLAGS, 0);
            NPROT_RELEASE_LOCK(&pOpenContext->Lock, FALSE);

            NdisAcquireSpinLock(&pContext->BindingLock);
            pOpenContext->BindingHandle = NULL;
            pContext->BindingHandle = NULL;
            NdisReleaseSpinLock(&pContext->BindingLock);
            ParaNdis_SendGratuitousArpPacket(pContext);
            DEBUGP(DL_INFO, ("[%s]: Sending out Grutuitous Arp pContext %p",
                   __FUNCTION__, pContext ));
        }
    } while (FALSE);

    //Free any other resources allocated for this bind.
    ndisprotFreeBindResources(pOpenContext);
    ndisprotDerefOpen(pOpenContext);  //Shutdown binding
}

VOID
ndisprotFreeBindResources(
    IN PNDISPROT_OPEN_CONTEXT       pOpenContext
    )
{
    if (pOpenContext->DeviceName.Buffer != NULL)
    {
        NdisFreeMemory(pOpenContext->DeviceName.Buffer, 0, 0);
        pOpenContext->DeviceName.Buffer = NULL;
        pOpenContext->DeviceName.Length =
        pOpenContext->DeviceName.MaximumLength = 0;
    }

    if (pOpenContext->DeviceDescr.Buffer != NULL)
    {
        //This would have been allocated by NdisQueryAdpaterInstanceName.
        NdisFreeMemory(pOpenContext->DeviceDescr.Buffer, 0, 0);
        pOpenContext->DeviceDescr.Buffer = NULL;
    }
}

VOID
ndisprotWaitForPendingIO(
    IN PNDISPROT_OPEN_CONTEXT            pOpenContext
    )
{
    //Wait for any pending sends or requests to be processed
    NPROT_ACQUIRE_LOCK(&pOpenContext->Lock, FALSE);
    if (pOpenContext->PendedSendCount == 0)
        ASSERT(pOpenContext->ClosingEvent == NULL);
    else
    {
        ASSERT(pOpenContext->ClosingEvent != NULL);
        DEBUGP(DL_WARN, ("[%s]: Open %p, %d pended sends\n",
            __FUNCTION__, pOpenContext, pOpenContext->PendedSendCount));
        NPROT_WAIT_EVENT(pOpenContext->ClosingEvent, 0);
    }
    NPROT_RELEASE_LOCK(&pOpenContext->Lock, FALSE);
}

VOID
ndisprotDoProtocolUnload(
    VOID
    )
{
    NDIS_HANDLE     ProtocolHandle;

    DEBUGP(DL_INFO, ("[%s] ProtocolHandle %lp\n",
        __FUNCTION__, Globals.NdisProtocolHandle));

    if (Globals.NdisProtocolHandle != NULL)
    {
        ProtocolHandle = Globals.NdisProtocolHandle;
        Globals.NdisProtocolHandle = NULL;
        NdisDeregisterProtocolDriver(ProtocolHandle);
    }
}

VOID
NdisprotUnload(
    IN PDRIVER_OBJECT DriverObject
    )
{
    UNICODE_STRING     win32DeviceName;
    PAGED_CODE();
    UNREFERENCED_PARAMETER(DriverObject);

    DEBUGP(DL_LOUD, ("Unload Enter\n"));

    //First delete the Control deviceobject and the corresponding
    //symbolicLink
    RtlInitUnicodeString(&win32DeviceName, DOS_DEVICE_NAME);
    IoDeleteSymbolicLink(&win32DeviceName);
    if (Globals.ControlDeviceObject)
    {
        IoDeleteDevice(Globals.ControlDeviceObject);
        Globals.ControlDeviceObject = NULL;
    }
    ndisprotDoProtocolUnload();
    DEBUGP(DL_LOUD, ("Unload Exit\n"));
}

VOID
ndisprotRefOpen(
    IN PNDISPROT_OPEN_CONTEXT        pOpenContext
    )
{
    NdisInterlockedIncrement((PLONG)&pOpenContext->RefCount);
}

VOID
ndisprotDerefOpen(
    IN PNDISPROT_OPEN_CONTEXT        pOpenContext
    )
{
    if (NdisInterlockedDecrement((PLONG)&pOpenContext->RefCount) == 0)
    {
        DEBUGP(DL_INFO, ("DerefOpen: Open %p, Flags %x, ref count is zero!\n",
            pOpenContext, pOpenContext->Flags));

        NPROT_ASSERT(pOpenContext->BindingHandle == NULL);
        NPROT_ASSERT(pOpenContext->RefCount == 0);

        pOpenContext->oc_sig++;
        NPROT_FREE_LOCK(&Globals.GlobalLock);
        NPROT_FREE_LOCK(&pOpenContext->Lock);
        NdisFreeMemory(pOpenContext, 0, 0);
    }
}

NDIS_STATUS
ndisprotDoRequest(
    IN PNDISPROT_OPEN_CONTEXT       pOpenContext,
    IN NDIS_PORT_NUMBER             PortNumber,
    IN NDIS_REQUEST_TYPE            RequestType,
    IN NDIS_OID                     Oid,
    IN PVOID                        InformationBuffer,
    IN ULONG                        InformationBufferLength,
    OUT PULONG                      pBytesProcessed
    )
{
    FWD_NDIS_REQUEST            ReqContext;
    PNDIS_OID_REQUEST           pNdisRequest = &ReqContext.Request;
    NDIS_STATUS                 Status;

    NdisZeroMemory(&ReqContext, sizeof(ReqContext));
    NPROT_INIT_EVENT(&ReqContext.Event);
    pNdisRequest->Header.Type = NDIS_OBJECT_TYPE_OID_REQUEST;
    pNdisRequest->Header.Revision = NDIS_OID_REQUEST_REVISION_1;
    pNdisRequest->Header.Size = sizeof(NDIS_OID_REQUEST);
    pNdisRequest->RequestType = RequestType;
    pNdisRequest->PortNumber = PortNumber;

    switch (RequestType)
    {
        case NdisRequestQueryInformation:
            pNdisRequest->DATA.QUERY_INFORMATION.Oid = Oid;
            pNdisRequest->DATA.QUERY_INFORMATION.InformationBuffer =
                                    InformationBuffer;
            pNdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength =
                                    InformationBufferLength;
            break;
        case NdisRequestSetInformation:
            pNdisRequest->DATA.SET_INFORMATION.Oid = Oid;
            pNdisRequest->DATA.SET_INFORMATION.InformationBuffer =
                                    InformationBuffer;
            pNdisRequest->DATA.SET_INFORMATION.InformationBufferLength =
                                    InformationBufferLength;
            break;
        default:
            NPROT_ASSERT(FALSE);
            break;
    }

    pNdisRequest->RequestId = NPROT_GET_NEXT_CANCEL_ID();
    Status = NdisOidRequest(pOpenContext->BindingHandle, pNdisRequest);

    if (Status == NDIS_STATUS_PENDING)
    {
        NPROT_WAIT_EVENT(&ReqContext.Event, 0);
        Status = ReqContext.Status;
    }

    if (Status == NDIS_STATUS_SUCCESS)
    {
        *pBytesProcessed = (RequestType == NdisRequestQueryInformation)?
                            pNdisRequest->DATA.QUERY_INFORMATION.BytesWritten:
                            pNdisRequest->DATA.SET_INFORMATION.BytesRead;

        //The driver below should set the correct value to BytesWritten
        //or BytesRead. But now, we just truncate the value to
        //InformationBufferLength
        if (*pBytesProcessed > InformationBufferLength)
        {
            *pBytesProcessed = InformationBufferLength;
        }
        NPROT_ACQUIRE_LOCK(&pOpenContext->Lock, FALSE);
        pOpenContext->PendedSendCount++;
        NdisReleaseSpinLock(&pOpenContext->Lock);
    }

    return Status;
}

VOID
NdisprotRequestComplete(
    IN NDIS_HANDLE                  ProtocolBindingContext,
    IN PNDIS_OID_REQUEST            pNdisRequest,
    IN NDIS_STATUS                  Status
    )
{
    PNDISPROT_OPEN_CONTEXT       pOpenContext;
    PFWD_NDIS_REQUEST            pFwdNdisRequest;

    pOpenContext = (PNDISPROT_OPEN_CONTEXT)ProtocolBindingContext;
    NPROT_STRUCT_ASSERT(pOpenContext, oc);

    //Get the Super-structure for NDIS_REQUEST before getting the callback
    //functions so make sure NdisRequest is a filled into a FWD_NDIS_REQUEST
    //before using this function
    pFwdNdisRequest = CONTAINING_RECORD(pNdisRequest,
                                        FWD_NDIS_REQUEST, Request);

    //This falls in ndisprotDoRequest case, pCallback isn't set
    if (pFwdNdisRequest->pCallback == NULL)
    {
        DEBUGP(DL_INFO, ("[%s]: orig %p callback %p, req %p, ref %d\n",
            __FUNCTION__, pFwdNdisRequest->OrigRequest,
            pFwdNdisRequest->pCallback, pFwdNdisRequest->Request,
            pFwdNdisRequest->Refcount));
    }
    else
    {
        (*pFwdNdisRequest->pCallback)(pOpenContext,
                                      pFwdNdisRequest,
                                      Status);
    }

    NdisAcquireSpinLock(&pOpenContext->Lock);
    pOpenContext->PendedSendCount--;
    if ((pOpenContext->PendedSendCount == 0) &&
       (pOpenContext->ClosingEvent != NULL))
    {
        NPROT_SIGNAL_EVENT(pOpenContext->ClosingEvent);
        pOpenContext->ClosingEvent = NULL;
    }
    NdisReleaseSpinLock(&pOpenContext->Lock);

    //Save away the completion status.
    pFwdNdisRequest->Status = Status;

    //Wake up the thread blocked for this request to complete
    //in ndisprotDoRequest
    if (pFwdNdisRequest->pCallback == NULL)
        NPROT_SIGNAL_EVENT(&pFwdNdisRequest->Event);
}

VOID
NdisprotStatus(
    IN NDIS_HANDLE                  ProtocolBindingContext,
    IN PNDIS_STATUS_INDICATION      StatusIndication
    )
{
    PNDISPROT_OPEN_CONTEXT       pOpenContext;
    NDIS_STATUS                  GeneralStatus;
    PNDIS_LINK_STATE             LinkState;

    pOpenContext = (PNDISPROT_OPEN_CONTEXT)ProtocolBindingContext;
    NPROT_STRUCT_ASSERT(pOpenContext, oc);

    if ((StatusIndication->Header.Type != NDIS_OBJECT_TYPE_STATUS_INDICATION)
         || (StatusIndication->Header.Size != sizeof(NDIS_STATUS_INDICATION)))
    {
        DEBUGP(DL_INFO,
        ("[%s]: Status: Received an invalid status indication: Open %p, StatusIndication %p\n",
        __FUNCTION__, pOpenContext, StatusIndication));
        return;
    }

    GeneralStatus = StatusIndication->StatusCode;

    NPROT_ACQUIRE_LOCK(&pOpenContext->Lock, FALSE);
    if (pOpenContext->PowerState != NetDeviceStateD0)
        DEBUGP(DL_WARN, ("[%s]: The device is in a low power state.\n",
               __FUNCTION__))

    switch(GeneralStatus)
    {
        case NDIS_STATUS_RESET_START:
            NPROT_ASSERT(!NPROT_TEST_FLAGS(pOpenContext->Flags,
                                           NPROTO_RESET_FLAGS,
                                           NPROTO_RESET_IN_PROGRESS));

            NPROT_SET_FLAGS(pOpenContext->Flags,
                            NPROTO_RESET_FLAGS,
                            NPROTO_RESET_IN_PROGRESS);

            break;
        case NDIS_STATUS_RESET_END:
            NPROT_ASSERT(NPROT_TEST_FLAGS(pOpenContext->Flags,
                                          NPROTO_RESET_FLAGS,
                                          NPROTO_RESET_IN_PROGRESS));

            NPROT_SET_FLAGS(pOpenContext->Flags,
                            NPROTO_RESET_FLAGS,
                            NPROTO_NOT_RESETTING);
            break;
        case NDIS_STATUS_LINK_STATE:
            ASSERT(StatusIndication->StatusBufferSize
                                               >= sizeof(NDIS_LINK_STATE));
            LinkState = (PNDIS_LINK_STATE)StatusIndication->StatusBuffer;
            if (LinkState->MediaConnectState == MediaConnectStateConnected)
            {
                NPROT_SET_FLAGS(pOpenContext->Flags,
                                NPROTO_MEDIA_FLAGS,
                                NPROTO_MEDIA_CONNECTED);
            }
            else
            {
                NPROT_SET_FLAGS(pOpenContext->Flags,
                                NPROTO_MEDIA_FLAGS,
                                NPROTO_MEDIA_DISCONNECTED);
            }

            break;
        default:
            break;
    }

    NPROT_RELEASE_LOCK(&pOpenContext->Lock, FALSE);
}
#endif
