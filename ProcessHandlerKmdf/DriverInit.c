#include "DriverInit.h"

PDRIVER_OBJECT gDrvObj = NULL;

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING pRegistryPath)
{
	UNREFERENCED_PARAMETER(pRegistryPath);

	NTSTATUS status = STATUS_SUCCESS;

#ifdef DBG
	PRINT_DEBUG("Loading driver...");

#ifdef DBG_INIT_BREAK
	DbgBreakPoint();
#endif

#endif

#ifdef DBG
	PRINT_DEBUG("Allocating DriverObjectExtension...");
#endif

	PDRIVER_EXTENSION_EX pDriverExt;
	status = IoAllocateDriverObjectExtension(pDriverObject, CLIENT_ID_ADDR, sizeof(PDRIVER_EXTENSION_EX), &pDriverExt);

	if (!NT_SUCCESS(status)) {
#ifdef DBG
		DbgPrint("ERROR!");
		PRINT_ERROR("Failed on allocating DriverObjectExtention.\n");
#endif
		return status;
	}

	RtlInitString(&(pDriverExt->targetName), TARGET_PROCESS_NAME);
	InitializeListHead(&(pDriverExt->targetsList));

#ifdef DBG
	DbgPrint("(target name OK)...");
#endif

	KeInitializeSpinLock(&(pDriverExt->drvExtSpinLock));
	pDriverExt->pendingIrp = NULL;

#ifdef DBG
	DbgPrint("OK");
#endif


#ifdef DBG
	PRINT_DEBUG("Creating device...");
#endif
	PDEVICE_OBJECT pDeviceObj;

	UNICODE_STRING devName;
	RtlInitUnicodeString(&devName, SYS_DEVICE_REG_NAME_W);

	status = IoCreateDevice(pDriverObject, sizeof(DEVICE_EXTENSION), &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &pDeviceObj);
	if (!NT_SUCCESS(status)) {
#ifdef DBG
		DbgPrint("ERROR!");
		PRINT_ERROR("Failed on creating device.\n");
#endif
		return status;
	}

	pDeviceObj->Flags |= DO_DIRECT_IO;

#ifdef DBG
	DbgPrint("OK");
#endif

#ifdef DBG
	PRINT_DEBUG("Creating device symbolic link...");
#endif
	PDEVICE_EXTENSION pDeviceExt = (PDEVICE_EXTENSION)pDeviceObj->DeviceExtension;

	UNICODE_STRING symLinkName;
	RtlInitUnicodeString(&symLinkName, SYM_LINK_REG_NAME_W);

	status = IoCreateSymbolicLink(&symLinkName, &devName);
	if (!NT_SUCCESS(status)) {
#ifdef DBG
		DbgPrint("ERROR!");
		PRINT_ERROR("Can't create symbolic link!");
#endif
		IoDeleteDevice(pDeviceObj);
		return status;
	}

	UNICODE_STRING fileFullName;
	RtlInitUnicodeString(&fileFullName, L"\\" READ_FILE_NAME);

	pDeviceExt->symLink = symLinkName;
	pDeviceExt->isFileOpen = FALSE;
	pDeviceExt->fileName = fileFullName;

#ifdef DBG
	DbgPrint("OK");
#endif

#ifdef DBG
	PRINT_DEBUG("Valid file name to open is:");
	DbgPrint("%wZ", &fileFullName);
#endif

#ifdef DBG
	PRINT_DEBUG("Registering major functions...");
#endif
	pDriverObject->MajorFunction[IRP_MJ_CREATE] = CreateCloseDispatch;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = CreateCloseDispatch;
	pDriverObject->MajorFunction[IRP_MJ_READ] = ReadWriteDispatch;
	pDriverObject->MajorFunction[IRP_MJ_WRITE] = ReadWriteDispatch;
#ifdef DBG
	DbgPrint("OK");
#endif

	pDriverObject->DriverUnload = UnloadDriver;

#ifdef DBG
	PRINT_DEBUG("Registering CreateProcessNotifyRoutine...");
#endif

	status = PsSetCreateProcessNotifyRoutine(CreateProcessNotifyRoutine, FALSE);
	if (!NT_SUCCESS(status))
	{
#ifdef DBG
		DbgPrint("ERROR!");
		PRINT_ERROR("Failed on CreateProcessNotifyRoutine registration.\n");
#endif
		return status;
	}

#ifdef DBG
	DbgPrint("OK");
#endif

#ifdef DBG
	PRINT_DEBUG("Driver loaded.\n");
#endif

	gDrvObj = pDriverObject;	// to access from callback
	return status;
}


VOID UnloadDriver(_In_ PDRIVER_OBJECT pDriverObject)
{
	UNREFERENCED_PARAMETER(pDriverObject);

#ifdef DBG
	PRINT_DEBUG("Unloading driver...");
#endif


#ifdef DBG
	PRINT_DEBUG("Unregistering CreateProcessNotifyRoutine...");
#endif

	PsSetCreateProcessNotifyRoutine(CreateProcessNotifyRoutine, TRUE);

#ifdef DBG
	DbgPrint("READY");
#endif


	PDRIVER_EXTENSION_EX pDrvExt = IoGetDriverObjectExtension(pDriverObject, CLIENT_ID_ADDR);

	// !!! Only AFTER unregistrting CALLBACK !!!
#ifdef DBG
	PRINT_DEBUG("Cleaning targets list...");
#endif

		PLIST_ENTRY targetsList = &(pDrvExt->targetsList);
		while (!IsListEmpty(targetsList))
		{
			PLIST_ENTRY nextTarget = RemoveHeadList(targetsList);
			ExFreePoolWithTag(nextTarget, PH_POOL_TAG);
		}
		
#ifdef DBG
	DbgPrint("READY");
#endif


#ifdef DBG
	PRINT_DEBUG("Deleting devices...");
#endif
	PDEVICE_OBJECT pCurrentDevice = pDriverObject->DeviceObject;
	while(pCurrentDevice != NULL) {
		PDEVICE_OBJECT pNextObject = pCurrentDevice->NextDevice;

		PDEVICE_EXTENSION pDeviceExt = (PDEVICE_EXTENSION)pCurrentDevice->DeviceExtension;
		PUNICODE_STRING pSymbolicLink = &(pDeviceExt->symLink);

		IoDeleteSymbolicLink(pSymbolicLink);
		IoDeleteDevice(pCurrentDevice);

		pCurrentDevice = pNextObject;
	}

#ifdef DBG
	DbgPrint("READY");
#endif


#ifdef DBG
	PRINT_DEBUG("Driver unloaded.\n");
#endif
}


VOID CreateProcessNotifyRoutine(_In_ HANDLE ParentId, _In_ HANDLE ProcessId, _In_ BOOLEAN isCreate)
{
	UNREFERENCED_PARAMETER(ParentId);

	PDEVICE_EXTENSION pDeviceExt = gDrvObj->DeviceObject->DeviceExtension;
	PDRIVER_EXTENSION_EX pDrvExt = IoGetDriverObjectExtension(gDrvObj, CLIENT_ID_ADDR);

	PKSPIN_LOCK pSpinLock = &(pDrvExt->drvExtSpinLock);
	KIRQL oldIrql;

	KeAcquireSpinLock(pSpinLock, &oldIrql);
	{
		if (!(pDeviceExt->isFileOpen))
		{
			KeReleaseSpinLock(pSpinLock, oldIrql);
			return;
		}
	}
	KeReleaseSpinLock(pSpinLock, oldIrql);

	PEPROCESS pProcStruct;
	NTSTATUS status = PsLookupProcessByProcessId(ProcessId, &pProcStruct);
	if (!NT_SUCCESS(status))
	{
#ifdef DBG
		PRINT_ERROR("Can't obtain process information\n");
#endif
		return;
	}
	
	STRING procName;
	LPCSTR procNameStr = PsGetProcessImageFileName(pProcStruct);

	RtlInitString(&procName, procNameStr);
	
	BOOLEAN isTarget = RtlEqualString(&(pDrvExt->targetName), &procName, TRUE);
	if (isTarget)
	{
		READ_BUFFER_TYPE newTarget;
		newTarget.pid = ProcessId;
		newTarget.isCreate = isCreate;

#ifdef DBG
		PRINT_DEBUG("Target process hit!");
		DbgPrint(" PID: %d Action: %s\n", ProcessId, isCreate? "Created" : "Terminated");
#endif
		
		KeAcquireSpinLock(pSpinLock, &oldIrql);
		{
			if (pDrvExt->pendingIrp != NULL)
			{
#ifdef DBG
				PRINT_DEBUG("Process pending IRP\n");
#endif
				CompleteReadIrp(pDrvExt->pendingIrp, newTarget);
				pDrvExt->pendingIrp = NULL;
			}
			else
			{
				PTARGETS_LIST_ENTRY newEntry = ExAllocatePoolWithTag(NonPagedPool, sizeof(TARGETS_LIST_ENTRY), PH_POOL_TAG);
				newEntry->data = newTarget;

#ifdef DBG
				PRINT_DEBUG("Inserting new target\n");
#endif

				InsertTailList(&(pDrvExt->targetsList), &(newEntry->listEntry));
			}
		}
		KeReleaseSpinLock(pSpinLock, oldIrql);

	}
}
