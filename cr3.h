auto read_physical(PVOID target_address,
	PVOID buffer,
	SIZE_T size,
	SIZE_T* bytes_read) -> NTSTATUS
{
	MM_COPY_ADDRESS to_read = { 0 };
	to_read.PhysicalAddress.QuadPart = (LONGLONG)target_address;
	return MmCopyMemory(buffer, to_read, size, MM_COPY_MEMORY_PHYSICAL, bytes_read);
}

struct cache {
	uintptr_t Address;
	MMPTE Value;
};
static cache cached_pml4e[512];
static cache cached_pdpte[512];
static cache cached_pde[512];
static cache cached_pte[512];

auto translate_linear(
	UINT64 directory_base,
	UINT64 address) -> UINT64 {
	_virt_addr_t virtual_address{};
	virtual_address.value = PVOID(address);
	SIZE_T Size = 0;

	if (cached_pml4e[virtual_address.pml4_index].Address != directory_base + 8 * virtual_address.pml4_index || !cached_pml4e[virtual_address.pml4_index].Value.u.Hard.Valid) {
		cached_pml4e[virtual_address.pml4_index].Address = directory_base + 8 * virtual_address.pml4_index;
		read_physical(PVOID(cached_pml4e[virtual_address.pml4_index].Address), reinterpret_cast<PVOID>(&cached_pml4e[virtual_address.pml4_index].Value), 8, &Size);
	}
	if (!cached_pml4e[virtual_address.pml4_index].Value.u.Hard.Valid)
		return 0;

	if (cached_pdpte[virtual_address.pdpt_index].Address != (cached_pml4e[virtual_address.pml4_index].Value.u.Hard.PageFrameNumber << 12) + 8 * virtual_address.pdpt_index || !cached_pdpte[virtual_address.pdpt_index].Value.u.Hard.Valid) {
		cached_pdpte[virtual_address.pdpt_index].Address = (cached_pml4e[virtual_address.pml4_index].Value.u.Hard.PageFrameNumber << 12) + 8 * virtual_address.pdpt_index;
		read_physical(PVOID(cached_pdpte[virtual_address.pdpt_index].Address), reinterpret_cast<PVOID>(&cached_pdpte[virtual_address.pdpt_index].Value), 8, &Size);
	}

	if (!cached_pdpte[virtual_address.pdpt_index].Value.u.Hard.Valid)
		return 0;

	if (cached_pde[virtual_address.pd_index].Address != (cached_pdpte[virtual_address.pdpt_index].Value.u.Hard.PageFrameNumber << 12) + 8 * virtual_address.pd_index || !cached_pde[virtual_address.pd_index].Value.u.Hard.Valid) {
		cached_pde[virtual_address.pd_index].Address = (cached_pdpte[virtual_address.pdpt_index].Value.u.Hard.PageFrameNumber << 12) + 8 * virtual_address.pd_index;
		read_physical(PVOID(cached_pde[virtual_address.pd_index].Address), reinterpret_cast<PVOID>(&cached_pde[virtual_address.pd_index].Value), 8, &Size);
	}
	if (!cached_pde[virtual_address.pd_index].Value.u.Hard.Valid)
		return 0;

	if (cached_pte[virtual_address.pt_index].Address != (cached_pde[virtual_address.pd_index].Value.u.Hard.PageFrameNumber << 12) + 8 * virtual_address.pt_index || !cached_pte[virtual_address.pt_index].Value.u.Hard.Valid) {
		cached_pte[virtual_address.pt_index].Address = (cached_pde[virtual_address.pd_index].Value.u.Hard.PageFrameNumber << 12) + 8 * virtual_address.pt_index;
		read_physical(PVOID(cached_pte[virtual_address.pt_index].Address), reinterpret_cast<PVOID>(&cached_pte[virtual_address.pt_index].Value), 8, &Size);
	}

	if (!cached_pte[virtual_address.pt_index].Value.u.Hard.Valid)
		return 0;

	return (cached_pte[virtual_address.pt_index].Value.u.Hard.PageFrameNumber << 12) + virtual_address.offset;
}