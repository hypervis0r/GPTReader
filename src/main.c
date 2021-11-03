#include "main.h"

DWORD GetGptHeader(struct GPT_TABLE_HEADER* gpt_header)
{
	HANDLE hDisk = NULL;
	DWORD dwBytesRead = 0;
	DISK_GEOMETRY_EX disk_geo = { 0 };
	OVERLAPPED ol = { 0 };

	/*
		Open up C: drive for raw disk i/o
		TODO: allow for disk selection
	*/
	hDisk = CreateFileW(
		L"\\\\.\\PhysicalDrive0",
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		0,
		OPEN_EXISTING,
		FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED,
		0);
	if (hDisk == INVALID_HANDLE_VALUE)
	{
		printf("[-] Failed to open disk: %d\n", GetLastError());
		return -1;
	}

	/*DeviceIoControl(
		hDisk,
		IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
		NULL,
		0,
		&disk_geo,
		sizeof(disk_geo),
		&dwBytesRead,
		NULL);*/

	/*
		Set read offset to the GPT header LBA
	*/
	ol.Offset = GPT_HEADER_OFFSET;

	/*
		Read the GPT header from disk
	*/
	BOOL result = ReadFile(hDisk, gpt_header, sizeof(*gpt_header), &dwBytesRead, &ol);
	if (result != TRUE && GetLastError() != ERROR_IO_PENDING)
	{
		printf("[-] Failed to read GPT header: %d\n", GetLastError());
		return -1;
	}

	WaitForSingleObject(hDisk, -1);

	CloseHandle(hDisk);

	return 0;
}

DWORD GetGptTableEntryArray(struct GPT_TABLE_HEADER* gpt_header, struct GPT_TABLE_ENTRY* gpt_entry_array, UINT32 gpt_entry_array_size)
{
	HANDLE hDisk = NULL;
	DWORD dwBytesRead = 0;
	DISK_GEOMETRY_EX disk_geo = { 0 };
	OVERLAPPED ol = { 0 };

	/*
		fucking disk granularity
		i would also add an "array size sanity check" here, since
		technically with a high enough size, one could have aRbiTraRy dIsK rEaDs, but idgaf
	*/
	if (gpt_entry_array_size % 512 != 0)
		return -1;

	/*
		Open up C: drive for raw disk i/o
		TODO: allow for disk selection
	*/
	hDisk = CreateFileW(
		L"\\\\.\\PhysicalDrive0",
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		0,
		OPEN_EXISTING,
		FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED,
		0);
	if (hDisk == INVALID_HANDLE_VALUE)
	{
		printf("[-] Failed to open disk: %d\n", GetLastError());
		return -1;
	}

	/*
		Set read offset to the start of the GPT entry array
	*/
	ol.Offset = LBA_TO_BYTES(gpt_header->gpt_entry_array_lba);

	/*
		Read the GPT entry array into the provided buffer
	*/
	BOOL result = ReadFile(hDisk, gpt_entry_array, gpt_entry_array_size, &dwBytesRead, &ol);
	if (result != TRUE && GetLastError() != ERROR_IO_PENDING)
	{
		printf("[-] Failed to read GPT entry: %d\n", GetLastError());
		return -1;
	}

	WaitForSingleObject(hDisk, -1);

	CloseHandle(hDisk);

	return 0;
}

int main(void)
{
	struct GPT_TABLE_HEADER gpt_header = { 0 };
	struct GPT_TABLE_ENTRY *gpt_entry_arr = NULL;
	const GUID null_guid = { 0 };
	DWORD result = 0;

	/*
		Get the GPT header from disk
	*/
	result = GetGptHeader(&gpt_header);
	if (result != 0)
	{
		printf("[-] Failed to get GPT header: you probably forgot to run as admin\n");
		return -1;
	}

	printf("---------- GPT Header ----------\n\n");

	printf("magic: %.8s\n", gpt_header.magic);
	printf("revision: %lu\n", gpt_header.revision);
	printf("header size: %lu\n", gpt_header.header_size);
	printf("header crc: 0x%x\n", gpt_header.header_crc);
	printf("header lba: %llu\n", gpt_header.header_lba);
	printf("mirror lba: %llu\n", gpt_header.mirror_header_lba);
	printf("disk guid = {%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}\n",
		gpt_header.disk_guid.Data1, gpt_header.disk_guid.Data2, gpt_header.disk_guid.Data3,
		gpt_header.disk_guid.Data4[0], gpt_header.disk_guid.Data4[1], gpt_header.disk_guid.Data4[2], gpt_header.disk_guid.Data4[3],
		gpt_header.disk_guid.Data4[4], gpt_header.disk_guid.Data4[5], gpt_header.disk_guid.Data4[6], gpt_header.disk_guid.Data4[7]);
	printf("gpt entry array count: %lu\n", gpt_header.gpt_entry_array_count);
	printf("gpt entry item size: %lu\n", gpt_header.gpt_entry_size);
	printf("gpt entry array crc: 0x%x\n\n", gpt_header.gpt_entry_array_crc);

	/*
		calculate the total gpt entry array size from gpt_header shit
	*/
	SIZE_T gpt_entry_arr_size = gpt_header.gpt_entry_array_count * gpt_header.gpt_entry_size;

	/*
		fucking disk sector granularity
		we gotta do this to align the size with the disk sector size
	*/
	gpt_entry_arr_size = ((DISK_SECTOR_SIZE + ((gpt_entry_arr_size + DISK_SECTOR_SIZE) - 1)) & ~(DISK_SECTOR_SIZE - 1));

	/*
		allocate the full gpt entry array
	*/
	gpt_entry_arr = VirtualAlloc(NULL, gpt_entry_arr_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!gpt_entry_arr)
		return -1;

	/*
		get teh GPT entry array
	*/
	result = GetGptTableEntryArray(&gpt_header, gpt_entry_arr, gpt_entry_arr_size);
	if (result != 0)
	{
		printf("[-] Failed to get GPT entry array\n");
		return -1;
	}

	for (UINT32 i = 0; i < gpt_header.gpt_entry_array_count; i++)
	{
		/*
			If partition type GUID is all zeros, it means the entry is unused,
			and we can safely skip it
		*/
		if (memcmp(&gpt_entry_arr[i].partition_type, &null_guid, sizeof(GUID)) == 0)
			continue;

		printf("---------- GPT Entry %lu ----------\n\n", i);

		wprintf(L"partition name: %s\n", gpt_entry_arr[i].partition_name);
		printf("partition guid = {%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}\n",
			gpt_entry_arr[i].partition_uuid.Data1, gpt_entry_arr[i].partition_uuid.Data2, gpt_entry_arr[i].partition_uuid.Data3,
			gpt_entry_arr[i].partition_uuid.Data4[0], gpt_entry_arr[i].partition_uuid.Data4[1], gpt_entry_arr[i].partition_uuid.Data4[2], gpt_entry_arr[i].partition_uuid.Data4[3],
			gpt_entry_arr[i].partition_uuid.Data4[4], gpt_entry_arr[i].partition_uuid.Data4[5], gpt_entry_arr[i].partition_uuid.Data4[6], gpt_entry_arr[i].partition_uuid.Data4[7]);
		printf("attributes: 0x%llX\n", gpt_entry_arr[i].attributes);
		printf("size in bytes: %llu\n\n", LBA_TO_BYTES(gpt_entry_arr[i].end_lba - gpt_entry_arr[i].start_lba));
	}

	VirtualFree(gpt_entry_arr, 0, MEM_RELEASE);
}