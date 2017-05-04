/*
* Uses code from https://github.com/proteansec/visual-studio-projects/blob/83953e5b6979e1b4a4687b703361cf3d02ca4176/hookssdt/mydriver.c
*/


//#include <wdm.h>
#include <ntddk.h>

/* Function Prototypes */
NTSTATUS MyDriver_UnSupportedFunction(PDEVICE_OBJECT DeviceObject, PIRP Irp);
VOID MyDriver_Unload(PDRIVER_OBJECT  DriverObject);    
NTSTATUS DriverEntry(PDRIVER_OBJECT  pDriverObject, PUNICODE_STRING  pRegistryPath); 

/* Compile directives. */
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, MyDriver_Unload)
#pragma alloc_text(PAGE, MyDriver_UnSupportedFunction)

/* The structure representing the System Service Table. */
typedef struct SystemServiceTable { 
        UINT32* 	ServiceTable; 
        UINT32* 	CounterTable; 
        UINT32		ServiceLimit; 
        UINT32*     ArgumentTable; 
} SST;

/* Declaration of KeServiceDescriptorTable, which is exported by ntoskrnl.exe. */
__declspec(dllimport) SST KeServiceDescriptorTable; 


/* 
 * Required information for hooking ZwQuerySystemInformation.
 */
NTSYSAPI NTSTATUS NTAPI ZwQuerySystemInformation(
	ULONG  SystemInformationClass, 
	PVOID  SystemInformation, 
	ULONG  SystemInformationLength, 
	PULONG ReturnLength 
);

typedef NTSTATUS (*ZwQuerySystemInformationPrototype)(
	ULONG SystemInformationCLass,
	PVOID SystemInformation, 
	ULONG SystemInformationLength, 
	PULONG ReturnLength
);

ZwQuerySystemInformationPrototype oldZwQuerySystemInformation = NULL;

/* 
 * Required information for hooking ZwAllocateVirtualMemory.
 */
NTSYSAPI NTSTATUS ZwAllocateVirtualMemory(
  HANDLE    ProcessHandle,
  PVOID     *BaseAddress,
  LONG_PTR	ZeroBits,
  PSIZE_T   RegionSize,
  ULONG     AllocationType,
  ULONG     Protect
);

typedef NTSTATUS (*ZwAllocateVirtualMemoryPrototype)(
  HANDLE    ProcessHandle,
  PVOID     *BaseAddress,
  LONG_PTR	ZeroBits,
  PSIZE_T   RegionSize,
  ULONG     AllocationType,
  ULONG     Protect
);

ZwAllocateVirtualMemoryPrototype oldZwAllocateVirtualMemory = NULL;

/* 
 * Required information for hooking ZwQuerySystemInformation.
 */
NTSYSAPI NTSTATUS ZwWriteFile(
  HANDLE           FileHandle,
  HANDLE           Event,
  PIO_APC_ROUTINE  ApcRoutine,
  PVOID            ApcContext,
  PIO_STATUS_BLOCK IoStatusBlock,
  PVOID            Buffer,
  ULONG            Length,
  PLARGE_INTEGER   ByteOffset,
  PULONG           Key
);

typedef NTSTATUS (*ZwWriteFilePrototype)(
  HANDLE           FileHandle,
  HANDLE           Event,
  PIO_APC_ROUTINE  ApcRoutine,
  PVOID            ApcContext,
  PIO_STATUS_BLOCK IoStatusBlock,
  PVOID            Buffer,
  ULONG            Length,
  PLARGE_INTEGER   ByteOffset,
  PULONG           Key
);

ZwWriteFilePrototype oldZwWriteFile = NULL;


/*
 * Disable the WP bit in CR0 register.
 */
void DisableWP() {
	__asm {
		push edx;
		mov edx, cr0;
		and edx, 0xFFFEFFFF;
		mov cr0, edx;
		pop edx;
	}
}

/*
 * Enable the WP bit in CR0 register.
 */
void EnableWP() {
	__asm {
		push edx;
		mov edx, cr0;
		or edx, 0x00010000;
		mov cr0, edx;
		pop edx;
	}
}

/*
 * A function that hooks the 'syscall' function in SSDT.
 */
PULONG HookSSDT(PUCHAR syscall, PUCHAR hookaddr) {
	/* local variables */
	UINT32 index;
	PLONG ssdt;
	PLONG target;
	
	/* disable WP bit in CR0 to enable writing to SSDT */
	DisableWP();
	DbgPrint("The WP flag in CR0 has been disabled.\r\n");
	
	/* identify the address of SSDT table */
	ssdt = KeServiceDescriptorTable.ServiceTable;
	DbgPrint("The system call address is %x.\r\n", syscall);
	DbgPrint("The hook function address is %x.\r\n", hookaddr);
	DbgPrint("The address of the SSDT is: %x.\r\n", ssdt);
	
	/* identify 'syscall' index into the SSDT table */
	index = *((PULONG)(syscall + 0x1));
	DbgPrint("The index into the SSDT table is: %d.\r\n", index);
	
	/* get the address of the service routine in SSDT */
	target = (PLONG)&(ssdt[index]);
	DbgPrint("The address of the SSDT routine to be hooked is: %x.\r\n", target);
	
	/* hook the service routine in SSDT */
	return (PUCHAR)InterlockedExchange(target, hookaddr);
}


/*
 * Hook Function.
 */
NTSTATUS Hook_ZwQuerySystemInformation(ULONG SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength) { 
	/* local variables */
	NTSTATUS status;
	
	/* calling new instructions */
    DbgPrint("ZwQuerySystemInformation hook called.\r\n"); 
	
	/* calling old function */
    status = oldZwQuerySystemInformation(SystemInformationClass, SystemInformation, SystemInformationLength, ReturnLength);
	if(!NT_SUCCESS(status)) {
		DbgPrint("The call to original ZwQuerySystemInformation did not succeed.\r\n");
	}
	return status;
}

/*
 * Hook Function.
 */
 NTSTATUS Hook_ZwAllocateVirtualMemory(HANDLE ProcessHandle, PVOID *BaseAddress, ULONG_PTR ZeroBits, PSIZE_T RegionSize, ULONG AllocationType, ULONG Protect) {
	 /* local variables */
	NTSTATUS status;
	
	/* calling new instructions */
    DbgPrint("ZwAllocateVirtualMemory hook called.\r\n"); 
	
	/* calling old function */
    status = oldZwAllocateVirtualMemory(ProcessHandle, *BaseAddress, ZeroBits, RegionSize, AllocationType, Protect);
	if(!NT_SUCCESS(status)) {
		DbgPrint("The call to original ZwAllocateVirtualMemory did not succeed.\r\n");
	}
	return status;
 }

/*
 * Hook Function.
 */
NTSTATUS Hook_ZwWriteFile(HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock, PVOID Buffer, ULONG Length, PLARGE_INTEGER ByteOffset, PULONG Key) { 
	/* local variables */
	NTSTATUS status;
	
	/* calling new instructions */
    DbgPrint("ZwWriteFile hook called.\r\n"); 
	
	/* calling old function */
    status = oldZwWriteFile(FileHandle, Event, ApcRoutine, ApcContext, IoStatusBlock, Buffer, Length, ByteOffset, Key);
	if(!NT_SUCCESS(status)) {
		DbgPrint("The call to original ZwWriteFile did not succeed.\r\n");
	}
	return status;
}

/*
 * DriverEntry: entry point for drivers.
 */
NTSTATUS DriverEntry(PDRIVER_OBJECT  pDriverObject, PUNICODE_STRING  pRegistryPath) {
    NTSTATUS NtStatus = STATUS_SUCCESS;
    unsigned int uiIndex = 0;
    PDEVICE_OBJECT pDeviceObject = NULL;
    UNICODE_STRING usDriverName, usDosDeviceName;

    DbgPrint("DriverEntry Called \r\n");

    RtlInitUnicodeString(&usDriverName, L"\\Device\\MyDriver");
    RtlInitUnicodeString(&usDosDeviceName, L"\\DosDevices\\MyDriver"); 

    NtStatus = IoCreateDevice(pDriverObject, 0, &usDriverName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &pDeviceObject);

    if(NtStatus == STATUS_SUCCESS) {
        /* MajorFunction: is a list of function pointers for entry points into the driver. */
        for(uiIndex = 0; uiIndex < IRP_MJ_MAXIMUM_FUNCTION; uiIndex++)
             pDriverObject->MajorFunction[uiIndex] = MyDriver_UnSupportedFunction;

			 
		/* DriverUnload is required to be able to dynamically unload the driver. */
        pDriverObject->DriverUnload =  MyDriver_Unload; 
        pDeviceObject->Flags |= 0;
		pDeviceObject->Flags &= (~DO_DEVICE_INITIALIZING);
		
        /* Create a Symbolic Link to the device. MyDriver -> \Device\MyDriver */
        IoCreateSymbolicLink(&usDosDeviceName, &usDriverName);
		
		/* hook SSDT */
		oldZwQuerySystemInformation = (ZwQuerySystemInformationPrototype)HookSSDT((PULONG)ZwQuerySystemInformation, (PULONG)Hook_ZwQuerySystemInformation);
		
		//oldZwAllocateVirtualMemory = (ZwAllocateVirtualMemoryPrototype)HookSSDT((PULONG)ZwAllocateVirtualMemory, (PULONG)Hook_ZwAllocateVirtualMemory);
		
		oldZwWriteFile = (ZwWriteFilePrototype)HookSSDT((PULONG)ZwWriteFile, (PULONG)Hook_ZwWriteFile);
    }

    return NtStatus;
}


 /*
  * MyDriver_Unload: called when the driver is unloaded.
  */
VOID MyDriver_Unload(PDRIVER_OBJECT  DriverObject) {
	/* local variables */
	UNICODE_STRING usDosDeviceName;

	/* restore the hook */
	if(oldZwQuerySystemInformation != NULL) {
		oldZwQuerySystemInformation = (ZwQuerySystemInformationPrototype)HookSSDT((PULONG)ZwQuerySystemInformation, (PULONG)oldZwQuerySystemInformation);
		EnableWP();
		DbgPrint("The original ZwQuerySystemInformation function restored.\r\n");
	}

	/* restore the hook */
	/*
	if(oldZwAllocateVirtualMemory != NULL) {
		oldZwAllocateVirtualMemory = (ZwAllocateVirtualMemoryPrototype)HookSSDT((PULONG)ZwAllocateVirtualMemory, (PULONG)oldZwAllocateVirtualMemory);
		EnableWP();
		DbgPrint("The original ZwAllocateVirtualMemory function restored.\r\n");
	}
	*/
	
	/* restore the hook */
	if(oldZwWriteFile != NULL) {
		oldZwWriteFile = (ZwWriteFilePrototype)HookSSDT((PULONG)ZwWriteFile, (PULONG)oldZwWriteFile);
		EnableWP();
		DbgPrint("The original ZwWriteFile function restored.\r\n");
	}

	/* delete the driver */
    DbgPrint("MyDriver_Unload Called \r\n");
    RtlInitUnicodeString(&usDosDeviceName, L"\\DosDevices\\MyDriver");
    IoDeleteSymbolicLink(&usDosDeviceName);
    IoDeleteDevice(DriverObject->DeviceObject);
}


/*
 * MyDriver_UnSupportedFunction: called when a major function is issued that isn't supported.
 */
NTSTATUS MyDriver_UnSupportedFunction(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    NTSTATUS NtStatus = STATUS_NOT_SUPPORTED;
    DbgPrint("MyDriver_UnSupportedFunction Called \r\n");

    return NtStatus;
}
