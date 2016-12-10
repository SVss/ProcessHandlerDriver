#pragma once

#include <ntddk.h>

#define __DRIVER_PART__	// to use PRINT_DEBUG macro

#include "..\Common\CommonDefinitions.h"
#include "Extensions.h"

// Major driver functions to register in DriverEntry

// CreateFile dispatcher
DRIVER_DISPATCH DispatchReadWrite;

// CloseHandle dispatcher
DRIVER_DISPATCH DispatchCreateClose;

// DeviceIoControl dispatcher
DRIVER_DISPATCH DeviceControlRoutine;
