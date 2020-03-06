/*
 * Copyright (c) 2000  Microsoft Corporation
 * Copyright (c) 2020 Oracle and/or its affiliates.
 */

#ifndef __NDISPROT__H
#define __NDISPROT__H

#include "ndis56common.h"

// Define the NDIS protocol interface version that this driver targets.
#if (defined(NDIS60_MINIPORT))
#  define NDIS_PROT_MAJOR_VERSION             6
#  define NDIS_PROT_MINOR_VERSION             0
#elif (defined(NDIS620_MINIPORT))
#  define NDIS_PROT_MAJOR_VERSION             6
#  define NDIS_PROT_MINOR_VERSION            20
#elif (defined(NDIS630_MINIPORT))
#  define NDIS_PROT_MAJOR_VERSION             6
#  define NDIS_PROT_MINOR_VERSION            30
#else
#  error Unsupported NDIS version
#endif

#define NT_DEVICE_NAME          L"\\Device\\VioProt"
#define DOS_DEVICE_NAME         L"\\Global??\\VioProt"

//Abstract types
typedef NDIS_EVENT              NPROT_EVENT, *PNPROT_EVENT;
typedef struct _tagPARANDIS_ADAPTER PARANDIS_ADAPTER, *PPARANDIS_ADAPTER;

#define MAX_MULTICAST_ADDRESS   0x20

#define NPROT_MAC_ADDR_LEN      6
#define NPROT_TAG               'SorP'

typedef enum _NDISPROT_OPEN_STATE{
    NdisprotInitializing,
    NdisprotRunning,
    NdisprotPausing,
    NdisprotPaused,
    NdisprotRestarting,
    NdisprotClosing
} NDISPROT_OPEN_STATE;

//  The Open Context represents an open of our device object.
//  We allocate this on processing a BindAdapter from NDIS,
//  and free it when all references (see below) to it are gone.
typedef struct _NDISPROT_OPEN_CONTEXT
{
    LIST_ENTRY              Link;           //Link into global list
    ULONG                   Flags;          //State information
    ULONG                   RefCount;
    NDIS_SPIN_LOCK          Lock;

    NDIS_HANDLE             BindingHandle;
    NDIS_HANDLE             MiniportAdapterHandle;
    PPARANDIS_ADAPTER       pContext;

    //BindParameters passed to protocol giving it information on
    //the miniport below
    NDIS_BIND_PARAMETERS    BindParameters;
    NDIS_RECEIVE_SCALE_CAPABILITIES RcvScaleCapabilities;

    ULONG                   PacketFilter;
    ULONG                   PendedSendCount;
    NET_DEVICE_POWER_STATE  PowerState;
    NDIS_STRING             DeviceName;     //used in NdisOpenAdapter
    NDIS_STRING             DeviceDescr;    //friendly name

    NDIS_STATUS             BindStatus;     //for Open/CloseAdapter
    NPROT_EVENT             BindEvent;      //for Open/CloseAdapter

    ULONG                   oc_sig;         //Signature for sanity
    NDISPROT_OPEN_STATE     State;
    PNPROT_EVENT            ClosingEvent;
    UCHAR                   CurrentAddress[NPROT_MAC_ADDR_LEN];
} NDISPROT_OPEN_CONTEXT, *PNDISPROT_OPEN_CONTEXT;

#define oc_signature        'OiuN'

//  Definitions for Flags above.
#define NPROTO_BIND_IDLE             0x00000000
#define NPROTO_BIND_OPENING          0x00000001
#define NPROTO_BIND_FAILED           0x00000002
#define NPROTO_BIND_ACTIVE           0x00000004
#define NPROTO_BIND_CLOSING          0x00000008
#define NPROTO_BIND_FLAGS            0x0000000F  //State of the binding

#define NPROTO_RESET_IN_PROGRESS     0x00000100
#define NPROTO_NOT_RESETTING         0x00000000
#define NPROTO_RESET_FLAGS           0x00000100

#define NPROTO_MEDIA_CONNECTED       0x00000000
#define NPROTO_MEDIA_DISCONNECTED    0x00000200
#define NPROTO_MEDIA_FLAGS           0x00000200

#define NPROTO_UNBIND_RECEIVED       0x10000000
#define NPROTO_UNBIND_FLAGS          0x10000000

//Globals structure shared between NeyKVM driver and protocol driver
typedef struct _NDISPROT_GLOBALS
{
    PDEVICE_OBJECT          ControlDeviceObject;
    NDIS_HANDLE             NdisProtocolHandle;
    UCHAR                   PartialCancelId;    //for cancelling sends
    ULONG                   LocalCancelId;
    LIST_ENTRY              UpAdaptList;        //of PARANDIS_ADAPTER structures
    NDIS_SPIN_LOCK          GlobalLock;         //to protect the above
} NDISPROT_GLOBALS, *PNDISPROT_GLOBALS;

#include <pshpack1.h>
#include <poppack.h>

extern NDISPROT_GLOBALS      Globals;

#define NPROT_ALLOC_TAG      'oiuN'

DRIVER_UNLOAD NdisprotUnload;
VOID
NdisprotUnload(
    IN PDRIVER_OBJECT   pDriverObject
    );

VOID
ndisprotRefOpen(
    IN PNDISPROT_OPEN_CONTEXT       pOpenContext
    );

VOID
ndisprotDerefOpen(
    IN PNDISPROT_OPEN_CONTEXT       pOpenContext
    );

PROTOCOL_BIND_ADAPTER_EX NdisprotBindAdapter;

PROTOCOL_OPEN_ADAPTER_COMPLETE_EX NdisprotOpenAdapterComplete;

PROTOCOL_UNBIND_ADAPTER_EX NdisprotUnbindAdapter;

PROTOCOL_CLOSE_ADAPTER_COMPLETE_EX NdisprotCloseAdapterComplete;

PROTOCOL_NET_PNP_EVENT NdisprotPnPEventHandler;

NDIS_STATUS
ndisprotCreateBinding(
    IN PNDISPROT_OPEN_CONTEXT       pOpenContext,
    IN PNDIS_BIND_PARAMETERS        BindParameters,
    IN NDIS_HANDLE                  BindContext,
    IN PUCHAR                       pBindingInfo,
    IN ULONG                        BindingInfoLength
    );

VOID
ndisprotShutdownBinding(
    IN PNDISPROT_OPEN_CONTEXT       pOpenContext
    );

VOID
ndisprotFreeBindResources(
    IN PNDISPROT_OPEN_CONTEXT       pOpenContext
    );

VOID
ndisprotWaitForPendingIO(
    IN PNDISPROT_OPEN_CONTEXT       pOpenContext
    );

VOID
ndisprotDoProtocolUnload(
    VOID
    );

NDIS_STATUS
ndisprotDoRequest(
    IN PNDISPROT_OPEN_CONTEXT       pOpenContext,
    IN NDIS_PORT_NUMBER             PortNumber,
    IN NDIS_REQUEST_TYPE            RequestType,
    IN NDIS_OID                     Oid,
    IN PVOID                        InformationBuffer,
    IN ULONG                        InformationBufferLength,
    OUT PULONG                      pBytesProcessed
    );

PROTOCOL_OID_REQUEST_COMPLETE NdisprotRequestComplete;

PROTOCOL_STATUS_EX NdisprotStatus;

PROTOCOL_RECEIVE_NET_BUFFER_LISTS NdisprotReceiveNetBufferLists;
PROTOCOL_SEND_NET_BUFFER_LISTS_COMPLETE NdisprotSendComplete;

typedef struct _FWD_NDIS_REQUEST FWD_NDIS_REQUEST, *PFWD_NDIS_REQUEST;
VOID
PtCompleteForwardedRequest(
    IN PNDISPROT_OPEN_CONTEXT       pOpenContext,
    IN PFWD_NDIS_REQUEST            pFwdNdisRequest,
    IN NDIS_STATUS                  Status
);

NDIS_STATUS
ForwardOidRequestToVF(
    IN PPARANDIS_ADAPTER            pContext,
    IN PNDIS_OID_REQUEST            Request
);

VOID
SendNetBufferListsToVF(
    IN NDIS_HANDLE      MiniportAdapterContext,
    IN PNET_BUFFER_LIST NetBufferLists,
    IN NDIS_PORT_NUMBER PortNumber,
    IN ULONG            SendFlags
);
#endif //__NDISPROT__H
