#include "DriverInit.h"

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING pRegistryPath)
{
	NTSTATUS status = STATUS_SUCCESS;

	UNREFERENCED_PARAMETER(pRegistryPath);

	// Register major functions

	// Register PCREATE_PROCESS_NOTIFY_ROUTINE

	pDriverObject->DriverUnload = UnloadDriver;

	return status;
}


VOID UnloadDriver(_In_ PDRIVER_OBJECT pDriverObject)
{
	UNREFERENCED_PARAMETER(pDriverObject);

	// TODO: Clear driver resources
}


VOID SetCreateProcessNotifyRoutine(_In_ HANDLE ParentId, _In_ HANDLE ProcessId, _In_ BOOLEAN isCreate)
{
	UNREFERENCED_PARAMETER(ParentId);
	UNREFERENCED_PARAMETER(ProcessId);
	UNREFERENCED_PARAMETER(isCreate);

	// TODO
}
