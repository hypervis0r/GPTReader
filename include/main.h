#include <Windows.h>
#include <stdint.h>
#include <stdio.h>
#include <wchar.h>

/*
	TODO: Disk sectors can be any size, but 512 is the standard.
*/
#define DISK_SECTOR_SIZE 512

#define LBA_TO_BYTES(x) ((x) * DISK_SECTOR_SIZE)

#define GPT_HEADER_OFFSET (LBA_TO_BYTES(1))

#define INDEX_TO_GPT_ENTRY(start_entry, gpt_header, index) ( start_entry + (gpt_header->gpt_entry_size * index) )

struct GPT_TABLE_HEADER
{
	UCHAR magic[8];
	UINT32 revision;
	UINT32 header_size;
	UINT32 header_crc;
	UINT32 reserved_1;
	UINT64 header_lba;
	UINT64 mirror_header_lba;
	UINT64 first_usable_block;
	UINT64 last_usable_block;
	GUID disk_guid;
	UINT64 gpt_entry_array_lba;
	UINT32 gpt_entry_array_count;
	UINT32 gpt_entry_size;
	UINT32 gpt_entry_array_crc;
	CHAR pad[DISK_SECTOR_SIZE - 0x5C];
};

struct GPT_TABLE_ENTRY
{
	GUID partition_type;
	GUID partition_uuid;
	UINT64 start_lba;
	UINT64 end_lba;
	UINT64 attributes;
	WCHAR partition_name[36];
};