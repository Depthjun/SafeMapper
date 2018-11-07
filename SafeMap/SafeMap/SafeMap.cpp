// SafeMap.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "pch.h"
#include <iostream>
#include <assert.h>
#include "LockedMemory.h"
#include "KernelRoutines.h"
#include "CapcomLoader.h"
#include "KernelHelper.h"

// Needs relocating
#undef WIN32_NO_STATUS
#include <ntstatus.h>
#include <unordered_map>

typedef struct _RTL_PROCESS_MODULE_INFORMATION
{
	HANDLE Section;
	PVOID MappedBase;
	PVOID ImageBase;
	ULONG ImageSize;
	ULONG Flags;
	USHORT LoadOrderIndex;
	USHORT InitOrderIndex;
	USHORT LoadCount;
	USHORT OffsetToFileName;
	UCHAR FullPathName[256];
} RTL_PROCESS_MODULE_INFORMATION, *PRTL_PROCESS_MODULE_INFORMATION;

typedef struct _RTL_PROCESS_MODULES
{
	ULONG NumberOfModules;
	RTL_PROCESS_MODULE_INFORMATION Modules[1];
} RTL_PROCESS_MODULES, *PRTL_PROCESS_MODULES;

std::unordered_map<std::wstring, uintptr_t> m_functions;

uintptr_t get_system_routine_internal(const std::wstring& name)
{
	uintptr_t address = { 0 };

	UNICODE_STRING unicode_name = { 0 };
	unicode_name.Buffer = (wchar_t*)name.c_str();
	unicode_name.Length = (name.size()) * 2;
	unicode_name.MaximumLength = (name.size() + 1) * 2;

	//const auto fetch = [&unicode_name, &address](kernel::MmGetSystemRoutineAddressFn mm_get_system_routine)
	//{
	//	address = (uintptr_t)mm_get_system_routine(&unicode_name);
	//};

	//run(fetch);

	return address;
}

uintptr_t get_system_routine(const std::wstring& name)
{
	const auto iter = m_functions.find(name);

	if (iter == m_functions.end())
	{
		const auto address = get_system_routine_internal(name);
		if (address != 0)
			m_functions.insert_or_assign(name, address);

		return address;
	}

	return iter->second;
}

uintptr_t get_kernel_module(const std::string_view kmodule)
{
	NTSTATUS status = 0x0;
	ULONG bytes = 0;
	std::vector<uint8_t> data;
	unsigned long required = 0;


	while ((status = NtQuerySystemInformation(SystemModuleInformation, data.data(), (ULONG)data.size(), &required)) == STATUS_INFO_LENGTH_MISMATCH) {
		data.resize(required);
	}

	if (status == STATUS_SUCCESS)
	{
		return 0;
	}

	const auto modules = reinterpret_cast<PRTL_PROCESS_MODULES>(data.data());
	for (unsigned i = 0; i < modules->NumberOfModules; ++i)
	{
		const auto& driver = modules->Modules[i];
		const auto image_base = reinterpret_cast<uintptr_t>(driver.ImageBase);
		std::string base_name = reinterpret_cast<char*>((uintptr_t)driver.FullPathName + driver.OffsetToFileName);
		const auto offset = base_name.find_last_of(".");

		if (kmodule == base_name)
			return reinterpret_cast<uintptr_t>(driver.ImageBase);

		if (offset != base_name.npos)
			base_name = base_name.erase(offset, base_name.size() - offset);

		if (kmodule == base_name)
			return reinterpret_cast<uintptr_t>(driver.ImageBase);
	}
}

int main()
{
	if (!Np_LockSections()) // Failed to setup locked sections, fail!
		return 0; 

	KernelContext* KrCtx = Kr_InitContext();
	CapcomContext* CpCtx = Cl_InitContext();

	if (!KrCtx || !CpCtx)
		return -1;

	Khu_Init(CpCtx, KrCtx);

// Good old cleanup ;)
	Cl_FreeContext(CpCtx);
	Kr_FreeContext(KrCtx);

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
