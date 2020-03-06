/*
 * Copyright (c) 2000  Microsoft Corporation
 * Copyright (c) 2019 Oracle and/or its affiliates
 *
 * Author:    Annie Li    Created for 2-netdev SRIOV live migration
 */

#include "precomp.h"

#if SRIOV
VOID
SendNetBufferListsToVF(
    IN NDIS_HANDLE      MiniportAdapterContext,
    IN PNET_BUFFER_LIST NetBufferLists,
    IN NDIS_PORT_NUMBER PortNumber,
    IN ULONG            SendFlags
)
{
    PPARANDIS_ADAPTER      pContext =
                           (PPARANDIS_ADAPTER)MiniportAdapterContext;
    PNDISPROT_OPEN_CONTEXT pOpenContext =
                           (PNDISPROT_OPEN_CONTEXT)pContext->pOpenContext;
    PNET_BUFFER_LIST       CurrentNetBufferList = NULL;
    NDIS_STATUS            Status = NDIS_STATUS_SUCCESS;
    PNDIS_HANDLE           pNdisHandle = NULL;
    ULONG                  SendCompleteFlags = 0;
    BOOLEAN                DispatchLevel = FALSE;

    DispatchLevel = NDIS_TEST_SEND_AT_DISPATCH_LEVEL(SendFlags);

    while (NetBufferLists != NULL)
    {
        CurrentNetBufferList = NetBufferLists;
        NetBufferLists = NET_BUFFER_LIST_NEXT_NBL(NetBufferLists);
        NET_BUFFER_LIST_NEXT_NBL(CurrentNetBufferList) = NULL;

        NPROT_ACQUIRE_LOCK(&pOpenContext->Lock, DispatchLevel);
        if (pOpenContext->State != NdisprotRunning ||
            !NPROT_TEST_FLAGS(pOpenContext->Flags,
                              NPROTO_BIND_FLAGS, NPROTO_BIND_ACTIVE))
        {
            Status = NDIS_STATUS_FAILURE;
            NPROT_RELEASE_LOCK(&pOpenContext->Lock, DispatchLevel);
            break;
        }
        pOpenContext->PendedSendCount++;
        NPROT_RELEASE_LOCK(&pOpenContext->Lock, DispatchLevel);

        //CurrentNetBufferList keeps original handle for completion.
        Status = NdisAllocateNetBufferListContext(CurrentNetBufferList,
                                                  sizeof(NDIS_HANDLE),
                                                  0,
                                                  NPROT_TAG);

        if (Status == NDIS_STATUS_SUCCESS)
        {
            pNdisHandle = (PNDIS_HANDLE)NET_BUFFER_LIST_CONTEXT_DATA_START(CurrentNetBufferList);
            NdisZeroMemory(pNdisHandle, sizeof(NDIS_HANDLE));
            *pNdisHandle = CurrentNetBufferList->SourceHandle;
            CurrentNetBufferList->SourceHandle = pContext->BindingHandle;

            //Clear this flag to avoid loopback the packets to VIO Protocol
            SendFlags &= ~NDIS_SEND_FLAGS_CHECK_FOR_LOOPBACK;
            //Send to VF miniport through BindingHandle of VF
            NdisSendNetBufferLists(pContext->BindingHandle,
                                   CurrentNetBufferList,
                                   PortNumber,
                                   SendFlags);
        }
        else
        {
            NPROT_ACQUIRE_LOCK(&pOpenContext->Lock, DispatchLevel);
            pOpenContext->PendedSendCount--;
            if ((NPROT_TEST_FLAGS(pOpenContext->Flags,
                 NPROTO_BIND_FLAGS, NPROTO_BIND_CLOSING))
                 && (pOpenContext->PendedSendCount == 0))
            {
                ASSERT(pOpenContext->ClosingEvent != NULL);
                NPROT_SIGNAL_EVENT(pOpenContext->ClosingEvent);
                pOpenContext->ClosingEvent = NULL;
            }
            NPROT_RELEASE_LOCK(&pOpenContext->Lock, DispatchLevel);

            NET_BUFFER_LIST_STATUS(CurrentNetBufferList) = Status;
            if (NDIS_TEST_SEND_AT_DISPATCH_LEVEL(SendFlags))
            {
                NDIS_SET_SEND_COMPLETE_FLAG(SendCompleteFlags,
                                  NDIS_SEND_COMPLETE_FLAGS_DISPATCH_LEVEL);
            }
            //Complete NetBufferList through VirtIO handle
            NdisMSendNetBufferListsComplete(pContext->MiniportHandle,
                                            CurrentNetBufferList,
                                            SendCompleteFlags);
            Status = NDIS_STATUS_SUCCESS;
        }
    }

    if (Status != NDIS_STATUS_SUCCESS)
    {
        PNET_BUFFER_LIST        TempNetBufferList;

        for(TempNetBufferList = CurrentNetBufferList;
            TempNetBufferList != NULL;
            TempNetBufferList = NET_BUFFER_LIST_NEXT_NBL(TempNetBufferList))
        {
            NET_BUFFER_LIST_STATUS(TempNetBufferList) = Status;
        }
        if (NDIS_TEST_SEND_AT_DISPATCH_LEVEL(SendFlags))
        {
            NDIS_SET_SEND_COMPLETE_FLAG(SendCompleteFlags,
                          NDIS_SEND_COMPLETE_FLAGS_DISPATCH_LEVEL);
        }

        NdisMSendNetBufferListsComplete(pContext->MiniportHandle,
                                        CurrentNetBufferList,
                                        SendCompleteFlags);
    }
}

VOID
CancelSendNetBufferListsToVF(
    IN NDIS_HANDLE MiniportAdapterContext,
    IN PVOID       CancelId
)
{
    PPARANDIS_ADAPTER pContext = (PPARANDIS_ADAPTER)MiniportAdapterContext;

    //VF's miniport driver triggers VIO Protocol send completion routine
    //for this cancellation.
    NdisCancelSendNetBufferLists(pContext->BindingHandle, CancelId);
}

//Returns NetBufferList through NdisReturnNetBufferLists to VF
VOID
ReturnNetBufferListsToVF(
    IN NDIS_HANDLE      MiniportAdapterContext,
    IN PNET_BUFFER_LIST NetBufferLists,
    IN ULONG            ReturnFlags
)
{
    PPARANDIS_ADAPTER pContext = (PPARANDIS_ADAPTER)MiniportAdapterContext;

    //Return NetBufferList through handle of VF miniport
    NdisReturnNetBufferLists(pContext->BindingHandle,
                             NetBufferLists,
                             ReturnFlags);
}

//Forward NDIS request made on NetKVM to the lower binding
NDIS_STATUS
ForwardOidRequestToVF(
    IN PPARANDIS_ADAPTER            pContext,
    IN PNDIS_OID_REQUEST            Request
)
{
    NDIS_STATUS            Status;
    PFWD_NDIS_REQUEST      pFwdNdisRequest = &pContext->Request;
    PNDISPROT_OPEN_CONTEXT pOpenContext =
                      (PNDISPROT_OPEN_CONTEXT)pContext->pOpenContext;

    do
    {
        //If the miniport below is going away, fail the request
        NdisAcquireSpinLock(&pOpenContext->Lock);
        if (pOpenContext->State != NdisprotRunning ||
            !NPROT_TEST_FLAGS(pOpenContext->Flags,
                              NPROTO_BIND_FLAGS, NPROTO_BIND_ACTIVE))
        {
            NdisReleaseSpinLock(&pOpenContext->Lock);
            Status = NDIS_STATUS_FAILURE;
            break;
        }
        NdisReleaseSpinLock(&pOpenContext->Lock);

        //If the virtual miniport edge is at a low power
        //state, fail this request.
        if (pOpenContext->PowerState > NdisDeviceStateD0)
        {
            Status = NDIS_STATUS_ADAPTER_NOT_READY;
            break;
        }

        pFwdNdisRequest->Cancelled = FALSE;
        pFwdNdisRequest->OrigRequest = Request;
        pFwdNdisRequest->pCallback = PtCompleteForwardedRequest;
        pFwdNdisRequest->Request.RequestType = Request->RequestType;
        pFwdNdisRequest->Refcount = 1;

        pFwdNdisRequest->Request.Header.Type = NDIS_OBJECT_TYPE_OID_REQUEST;
        pFwdNdisRequest->Request.Header.Revision = NDIS_OID_REQUEST_REVISION_1;
        pFwdNdisRequest->Request.Header.Size = sizeof(NDIS_OID_REQUEST);

        switch(Request->RequestType)
        {
        case NdisRequestQueryInformation:
        case NdisRequestQueryStatistics:
            pFwdNdisRequest->Request.DATA.QUERY_INFORMATION.Oid =
                Request->DATA.QUERY_INFORMATION.Oid;
            pFwdNdisRequest->Request.DATA.QUERY_INFORMATION.InformationBuffer =
                Request->DATA.QUERY_INFORMATION.InformationBuffer;
            pFwdNdisRequest->Request.DATA.QUERY_INFORMATION.InformationBufferLength =
                Request->DATA.QUERY_INFORMATION.InformationBufferLength;
            break;

        case NdisRequestSetInformation:
            pFwdNdisRequest->Request.DATA.SET_INFORMATION.Oid =
                Request->DATA.SET_INFORMATION.Oid;
            pFwdNdisRequest->Request.DATA.SET_INFORMATION.InformationBuffer =
                Request->DATA.SET_INFORMATION.InformationBuffer;
            pFwdNdisRequest->Request.DATA.SET_INFORMATION.InformationBufferLength =
                Request->DATA.SET_INFORMATION.InformationBufferLength;
            break;

        case NdisRequestMethod:
        default:
            ASSERT(FALSE);
            break;
        }

        //If the miniport below is going away, fail the request
        NdisAcquireSpinLock(&pOpenContext->Lock);
        if ((pOpenContext->State == NdisprotClosing) ||
            NPROT_TEST_FLAGS(pOpenContext->Flags,
                             NPROTO_BIND_FLAGS, NPROTO_BIND_CLOSING))
        {
            NdisReleaseSpinLock(&pOpenContext->Lock);
            pFwdNdisRequest->OrigRequest = NULL;
            Status = NDIS_STATUS_FAILURE;
            break;
        }
        else
        {
            pOpenContext->PendedSendCount++;
            NdisReleaseSpinLock(&pOpenContext->Lock);
            Status = NdisOidRequest(pContext->BindingHandle,
                                    &pFwdNdisRequest->Request);
        }
        if (Status != NDIS_STATUS_PENDING)
        {
            NdisprotRequestComplete(pOpenContext,
                                    &pFwdNdisRequest->Request, Status);
            Status = NDIS_STATUS_PENDING;
            break;
        }
    } while (FALSE);

    return Status;
}

NDIS_STATUS
OidRequestToVF(
    IN    NDIS_HANDLE             MiniportAdapterContext,
    IN    PNDIS_OID_REQUEST       NdisRequest
)
{
    PPARANDIS_ADAPTER pContext = (PPARANDIS_ADAPTER)MiniportAdapterContext;
    NDIS_REQUEST_TYPE       RequestType;
    NDIS_STATUS             Status;

    RequestType = NdisRequest->RequestType;

    switch(RequestType)
    {
    case NdisRequestMethod:
        Status = NDIS_STATUS_NOT_SUPPORTED;
        break;

    case NdisRequestSetInformation:
    case NdisRequestQueryInformation:
    case NdisRequestQueryStatistics:
        Status = ForwardOidRequestToVF(pContext, NdisRequest);
        if ((Status != NDIS_STATUS_SUCCESS) &&
           (Status != NDIS_STATUS_PENDING))
        {
            DEBUGP(DL_WARN, ("[%s]: VELAN %p, Status = 0x%08x\n",
                   __FUNCTION__, pContext, Status));
        }
        break;

    default:
        Status = NDIS_STATUS_NOT_SUPPORTED;
        break;
    }

    return Status;
}

//Protocol entry point called by NDIS if the driver below
//uses NDIS6 NetBufferList indications.
VOID
NdisprotReceiveNetBufferLists(
    IN NDIS_HANDLE                  ProtocolBindingContext,
    IN PNET_BUFFER_LIST             pNetBufferLists,
    IN NDIS_PORT_NUMBER             PortNumber,
    IN ULONG                        NumberOfNetBufferLists,
    IN ULONG                        ReceiveFlags
)
{
    PNDISPROT_OPEN_CONTEXT  pOpenContext;
    ULONG                   ReturnFlags = 0;
    ULONG                   NewReceiveFlags;
    BOOLEAN                 DispatchLevel = FALSE;

    UNREFERENCED_PARAMETER(PortNumber);
    UNREFERENCED_PARAMETER(NumberOfNetBufferLists);

    pOpenContext = (PNDISPROT_OPEN_CONTEXT)ProtocolBindingContext;
    DispatchLevel = (BOOLEAN)NDIS_TEST_RECEIVE_AT_DISPATCH_LEVEL(ReceiveFlags);
    if (DispatchLevel)
    {
        NDIS_SET_RETURN_FLAG(ReturnFlags, NDIS_RETURN_FLAGS_DISPATCH_LEVEL);
    }

    ASSERT(pNetBufferLists != NULL);
    NPROT_STRUCT_ASSERT(pOpenContext, oc);

    NPROT_ACQUIRE_LOCK(&pOpenContext->Lock, DispatchLevel);
    if (pOpenContext->State != NdisprotRunning ||
        !NPROT_TEST_FLAGS(pOpenContext->Flags,
                          NPROTO_BIND_FLAGS, NPROTO_BIND_ACTIVE))
    {
        if (NDIS_TEST_RECEIVE_CAN_PEND(ReceiveFlags))
        {
            //Return NetBufferList since no Indication for it
            NdisReturnNetBufferLists(pOpenContext->BindingHandle,
                                     pNetBufferLists,
                                     ReturnFlags);
            NPROT_RELEASE_LOCK(&pOpenContext->Lock, DispatchLevel);
            return;
        }
    }
    NPROT_RELEASE_LOCK(&pOpenContext->Lock, DispatchLevel);

    NewReceiveFlags = ReceiveFlags;
    if (NDIS_TEST_RECEIVE_CANNOT_PEND(ReceiveFlags))
        NDIS_SET_RECEIVE_FLAG(NewReceiveFlags, NDIS_RECEIVE_FLAGS_RESOURCES);

    //Indicate packets through handle of VirtIO miniport
    NdisMIndicateReceiveNetBufferLists(pOpenContext->MiniportAdapterHandle,
        pNetBufferLists,
        PortNumber,
        NumberOfNetBufferLists,
        NewReceiveFlags);
}

//Routine of any Send Completion
VOID
NdisprotSendComplete(
    IN NDIS_HANDLE                  ProtocolBindingContext,
    IN PNET_BUFFER_LIST             pNetBufferList,
    IN ULONG                        SendCompleteFlags
)
{
    NDIS_STATUS             Status;
    PNET_BUFFER_LIST        CurrentNetBufferList;
    PNDIS_HANDLE            pSendContext = NULL;
    PNDISPROT_OPEN_CONTEXT  pOpenContext =
                           (PNDISPROT_OPEN_CONTEXT)ProtocolBindingContext;
    BOOLEAN                 DispatchLevel;

    DEBUGP(DL_VERY_LOUD,
          ("==> [%s]: ProtocolBindingContext %p, NetBufferLists %p\n",
           __FUNCTION__, ProtocolBindingContext, pNetBufferList));

    DispatchLevel = NDIS_TEST_SEND_COMPLETE_AT_DISPATCH_LEVEL(SendCompleteFlags);

    while (pNetBufferList)
    {
        CurrentNetBufferList = pNetBufferList;
        pNetBufferList = NET_BUFFER_LIST_NEXT_NBL(pNetBufferList);
        NET_BUFFER_LIST_NEXT_NBL(CurrentNetBufferList) = NULL;

        Status = NET_BUFFER_LIST_STATUS(CurrentNetBufferList);
        pSendContext =
        (PNDIS_HANDLE)NET_BUFFER_LIST_CONTEXT_DATA_START(CurrentNetBufferList);
        CurrentNetBufferList->SourceHandle = *pSendContext;

        NdisFreeNetBufferListContext(CurrentNetBufferList,
                                     sizeof(NDIS_HANDLE));

        NdisMSendNetBufferListsComplete(pOpenContext->MiniportAdapterHandle,
                                        CurrentNetBufferList,
                                        SendCompleteFlags);

        NPROT_ACQUIRE_LOCK(&pOpenContext->Lock, DispatchLevel);
        pOpenContext->PendedSendCount--;

        if ((NPROT_TEST_FLAGS(pOpenContext->Flags,
             NPROTO_BIND_FLAGS, NPROTO_BIND_CLOSING))
             && (pOpenContext->PendedSendCount == 0))
        {
            ASSERT(pOpenContext->ClosingEvent != NULL);
            NPROT_SIGNAL_EVENT(pOpenContext->ClosingEvent);
            pOpenContext->ClosingEvent = NULL;
        }
        NPROT_RELEASE_LOCK(&pOpenContext->Lock, DispatchLevel);
    }

    DEBUGP(DL_VERY_LOUD,
          ("<== [%s]: ProtocolBindingContext %p, NetBufferLists %p\n",
          __FUNCTION__, ProtocolBindingContext, pNetBufferList));
}
#endif
