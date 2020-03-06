/*
 * Copyright (c) 2000  Microsoft Corporation
 * Copyright (c) 2020 Oracle and/or its affiliates.
 */

#include "precomp.h"

#define __FILENUMBER 'PSID'


#ifdef ALLOC_PRAGMA

#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, NdisprotUnload)

#endif // ALLOC_PRAGMA


//
//  Globals:
//
NDISPROT_GLOBALS         Globals = {0};

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT   pDriverObject,
    IN PUNICODE_STRING  pRegistryPath
    )
/*++

Routine Description:

    Called on loading. We create a device object to handle user-mode requests
    on, and register ourselves as a protocol with NDIS.

Arguments:

    pDriverObject - Pointer to driver object created by system.

    pRegistryPath - Pointer to the Unicode name of the registry path
        for this driver.

Return Value:

    NT Status code
    
--*/
{
    NDIS_PROTOCOL_DRIVER_CHARACTERISTICS   protocolChar = {0};
    NTSTATUS                        status = STATUS_SUCCESS;
    NDIS_STRING                     protoName = NDIS_STRING_CONST("NDISPROT");     
    UNICODE_STRING                  ntDeviceName;
    UNICODE_STRING                  win32DeviceName;
    BOOLEAN                         fSymbolicLink = FALSE;
    PDEVICE_OBJECT                  deviceObject = NULL;
    NDIS_HANDLE  ProtocolDriverContext={0};

    UNREFERENCED_PARAMETER(pRegistryPath);
	
    DEBUGP(DL_LOUD, ("DriverEntry\n"));

    Globals.pDriverObject = pDriverObject;
    Globals.EthType = NPROT_ETH_TYPE;
    NPROT_INIT_EVENT(&Globals.BindsComplete);

    do
    {
        //
        // Create our device object using which an application can
        // access NDIS devices.
        //
        RtlInitUnicodeString(&ntDeviceName, NT_DEVICE_NAME);

        status = IoCreateDevice (pDriverObject,
                                 0,
                                 &ntDeviceName,
                                 FILE_DEVICE_NETWORK,
                                 FILE_DEVICE_SECURE_OPEN,
                                 FALSE,
                                 &deviceObject);
    
        if (!NT_SUCCESS (status))
        {
            //
            // Either not enough memory to create a deviceobject or another
            // deviceobject with the same name exits. This could happen
            // if you install another instance of this device.
            //
            break;
        }

        RtlInitUnicodeString(&win32DeviceName, DOS_DEVICE_NAME);

        status = IoCreateSymbolicLink(&win32DeviceName, &ntDeviceName);

        if (!NT_SUCCESS(status))
        {
            break;
        }

        fSymbolicLink = TRUE;
    
        deviceObject->Flags |= DO_DIRECT_IO;
        Globals.ControlDeviceObject = deviceObject;

        NPROT_INIT_LIST_HEAD(&Globals.OpenList);
        NPROT_INIT_LOCK(&Globals.GlobalLock);

        //
        // Initialize the protocol characterstic structure
        //     
#if (NDIS_SUPPORT_NDIS630)
        {C_ASSERT(sizeof(protocolChar) >= NDIS_SIZEOF_PROTOCOL_DRIVER_CHARACTERISTICS_REVISION_2);}
        protocolChar.Header.Type        = NDIS_OBJECT_TYPE_PROTOCOL_DRIVER_CHARACTERISTICS,
        protocolChar.Header.Size        = NDIS_SIZEOF_PROTOCOL_DRIVER_CHARACTERISTICS_REVISION_2;
        protocolChar.Header.Revision    = NDIS_PROTOCOL_DRIVER_CHARACTERISTICS_REVISION_2;
#elif (NDIS_SUPPORT_NDIS6)
        {C_ASSERT(sizeof(protocolChar) >= NDIS_SIZEOF_PROTOCOL_DRIVER_CHARACTERISTICS_REVISION_1);}
        protocolChar.Header.Type        = NDIS_OBJECT_TYPE_PROTOCOL_DRIVER_CHARACTERISTICS,
        protocolChar.Header.Size        = NDIS_SIZEOF_PROTOCOL_DRIVER_CHARACTERISTICS_REVISION_1;
        protocolChar.Header.Revision    = NDIS_PROTOCOL_DRIVER_CHARACTERISTICS_REVISION_1;
#endif // NDIS MINIPORT VERSION

        protocolChar.MajorNdisVersion            = NDIS_PROT_MAJOR_VERSION;
        protocolChar.MinorNdisVersion            = NDIS_PROT_MINOR_VERSION;
        protocolChar.MajorDriverVersion          = MAJOR_DRIVER_VERSION;
        protocolChar.MinorDriverVersion          = MINOR_DRIVER_VERISON;
        protocolChar.Name                        = protoName;
        protocolChar.SetOptionsHandler           = NULL;
        protocolChar.OpenAdapterCompleteHandlerEx  = NdisprotOpenAdapterComplete;
        protocolChar.CloseAdapterCompleteHandlerEx = NdisprotCloseAdapterComplete;
        protocolChar.SendNetBufferListsCompleteHandler = NdisprotSendComplete;
        protocolChar.OidRequestCompleteHandler   = NdisprotRequestComplete;
        protocolChar.StatusHandlerEx             = NdisprotStatus;
        protocolChar.UninstallHandler            = NULL;
        protocolChar.ReceiveNetBufferListsHandler = NdisprotReceiveNetBufferLists;
        protocolChar.NetPnPEventHandler          = NdisprotPnPEventHandler;
        protocolChar.BindAdapterHandlerEx        = NdisprotBindAdapter;
        protocolChar.UnbindAdapterHandlerEx      = NdisprotUnbindAdapter;

        //
        // Register as a protocol driver
        //
    
        status = NdisRegisterProtocolDriver(ProtocolDriverContext,           // driver context
                                            &protocolChar,
                                            &Globals.NdisProtocolHandle);

        if (status != NDIS_STATUS_SUCCESS)
        {
            DEBUGP(DL_WARN, ("Failed to register protocol with NDIS\n"));
            status = STATUS_UNSUCCESSFUL;
            break;
        }

        Globals.PartialCancelId = NdisGeneratePartialCancelId();
        Globals.PartialCancelId <<= ((sizeof(PVOID) - 1) * 8);
        DEBUGP(DL_LOUD, ("DriverEntry: CancelId %lx\n", Globals.PartialCancelId));

        pDriverObject->DriverUnload = NdisprotUnload;

        status = STATUS_SUCCESS;

        
    }
    while (FALSE);
       

    if (!NT_SUCCESS(status))
    {
        if (deviceObject)
        {
            KeEnterCriticalRegion();
            IoDeleteDevice(deviceObject);
            KeLeaveCriticalRegion();
            Globals.ControlDeviceObject = NULL;
        }

        if (fSymbolicLink)
        {
            IoDeleteSymbolicLink(&win32DeviceName);
            fSymbolicLink = FALSE;
        }
        
        if (Globals.NdisProtocolHandle)
        {
            NdisDeregisterProtocolDriver(Globals.NdisProtocolHandle);
            Globals.NdisProtocolHandle = NULL;
        }        
    }
    
    return status;
}


VOID
NdisprotUnload(
    IN PDRIVER_OBJECT DriverObject
    )
/*++

Routine Description:

    Free all the allocated resources, etc.

Arguments:

    DriverObject - pointer to a driver object.

Return Value:

    VOID.

--*/
{

    UNICODE_STRING     win32DeviceName;
    PAGED_CODE();
    UNREFERENCED_PARAMETER(DriverObject);
	
    DEBUGP(DL_LOUD, ("Unload Enter\n"));

    //
    // First delete the Control deviceobject and the corresponding
    // symbolicLink
    //
    RtlInitUnicodeString(&win32DeviceName, DOS_DEVICE_NAME);

    IoDeleteSymbolicLink(&win32DeviceName);           


    if (Globals.ControlDeviceObject)
    {
        IoDeleteDevice(Globals.ControlDeviceObject);
        Globals.ControlDeviceObject = NULL;
    }


    ndisprotDoProtocolUnload();

#if DBG
    ndisprotAuditShutdown();
#endif

    DEBUGP(DL_LOUD, ("Unload Exit\n"));
}

VOID
ndisprotRefOpen(
    IN PNDISPROT_OPEN_CONTEXT        pOpenContext
    )
/*++

Routine Description:

    Reference the given open context.

    NOTE: Can be called with or without holding the opencontext lock.

Arguments:

    pOpenContext - pointer to open context

Return Value:

    None

--*/
{
    NdisInterlockedIncrement((PLONG)&pOpenContext->RefCount);
}


VOID
ndisprotDerefOpen(
    IN PNDISPROT_OPEN_CONTEXT        pOpenContext
    )
/*++

Routine Description:

    Dereference the given open context. If the ref count goes to zero,
    free it.

    NOTE: called without holding the opencontext lock

Arguments:

    pOpenContext - pointer to open context

Return Value:

    None

--*/
{
    if (NdisInterlockedDecrement((PLONG)&pOpenContext->RefCount) == 0)
    {
        DEBUGP(DL_INFO, ("DerefOpen: Open %p, Flags %x, ref count is zero!\n",
            pOpenContext, pOpenContext->Flags));
        
        NPROT_ASSERT(pOpenContext->BindingHandle == NULL);
        NPROT_ASSERT(pOpenContext->RefCount == 0);
        NPROT_ASSERT(pOpenContext->pFileObject == NULL);

        pOpenContext->oc_sig++;

        //
        //  Free it.
        //
        NPROT_FREE_MEM(pOpenContext);
    }
}
