#include "media_catalog.h"

extern "C" {
#include <fatfs/ff.h>
}

static const unsigned kCatalogMaxEntries = 512u;
static const unsigned kCatalogMaxDepth = 16u;

struct TCatalogRoot
{
	const char *path;
	unsigned char type;
};

struct TRootFingerprint
{
	unsigned short date;
	unsigned short time;
	unsigned char exists;
	unsigned char is_dir;
};

struct TRootCatalogCache
{
	boolean valid;
	TRootFingerprint fp;
	TComboMediaEntry entries[kCatalogMaxEntries];
	unsigned entry_count;
	unsigned rom_count;
	unsigned disk_count;
	unsigned dir_count;
	unsigned file_count;
	unsigned error_count;
	unsigned dropped_count;
};

struct TCatalogStackNode
{
	char path[192];
	unsigned char type;
	unsigned char root_index;
	DIR dir;
};

static const TCatalogRoot kCatalogRoots[] = {
	{"SD:/sms/roms", (unsigned char) ComboMediaTypeRom}
};
static const unsigned kRootCount = (unsigned) (sizeof(kCatalogRoots) / sizeof(kCatalogRoots[0]));

static TComboMediaEntry s_Entries[kCatalogMaxEntries];
static unsigned s_EntryCount = 0u;
static unsigned s_RomCount = 0u;
static unsigned s_DiskCount = 0u;
static unsigned s_DirCount = 0u;
static unsigned s_FileCount = 0u;
static unsigned s_ErrorCount = 0u;
static unsigned s_DroppedCount = 0u;
static unsigned s_RootIndex = 0u;
static unsigned s_StackDepth = 0u;
static boolean s_Scanning = FALSE;
static boolean s_RootNeedsScan[kRootCount];
static TCatalogStackNode s_Stack[kCatalogMaxDepth];
static TRootCatalogCache s_RootCache[kRootCount];
static TRootCatalogCache s_RootScanCache[kRootCount];

static void local_memset(void *dst, unsigned char value, unsigned size)
{
	unsigned char *out = (unsigned char *) dst;
	for (unsigned i = 0u; i < size; ++i)
	{
		out[i] = value;
	}
}

static void local_memcpy(void *dst, const void *src, unsigned size)
{
	unsigned char *out = (unsigned char *) dst;
	const unsigned char *in = (const unsigned char *) src;
	for (unsigned i = 0u; i < size; ++i)
	{
		out[i] = in[i];
	}
}

static unsigned local_strlen(const char *text)
{
	unsigned len = 0u;
	if (text == 0)
	{
		return 0u;
	}
	while (text[len] != '\0')
	{
		++len;
	}
	return len;
}

static char local_tolower(char ch)
{
	if (ch >= 'A' && ch <= 'Z')
	{
		return (char) (ch - 'A' + 'a');
	}
	return ch;
}

static int local_starts_with_dot(const char *name)
{
	return name != 0 && name[0] == '.';
}

static int local_ext_match(const char *name, const char *ext)
{
	const unsigned name_len = local_strlen(name);
	const unsigned ext_len = local_strlen(ext);
	if (name_len < ext_len || ext_len == 0u)
	{
		return 0;
	}

	const char *a = name + (name_len - ext_len);
	for (unsigned i = 0u; i < ext_len; ++i)
	{
		if (local_tolower(a[i]) != local_tolower(ext[i]))
		{
			return 0;
		}
	}
	return 1;
}

static int local_is_supported_file(const char *name, unsigned char type)
{
	if (name == 0 || name[0] == '\0')
	{
		return 0;
	}
	if (name[0] == '.')
	{
		return 0;
	}

	if (type == (unsigned char) ComboMediaTypeRom)
	{
		return local_ext_match(name, ".rom")
			|| local_ext_match(name, ".ccc")
			|| local_ext_match(name, ".mx1")
			|| local_ext_match(name, ".mx2");
	}
	if (type == (unsigned char) ComboMediaTypeDisk)
	{
		return local_ext_match(name, ".dsk")
			|| local_ext_match(name, ".img")
			|| local_ext_match(name, ".di1")
			|| local_ext_match(name, ".di2");
	}
	return 0;
}

static int local_join_path(char *dst, unsigned dst_size, const char *base, const char *name)
{
	if (dst == 0 || dst_size == 0u || base == 0 || name == 0)
	{
		return 0;
	}

	const unsigned base_len = local_strlen(base);
	const unsigned name_len = local_strlen(name);
	const unsigned needs_sep = (base_len > 0u && base[base_len - 1u] != '/') ? 1u : 0u;
	const unsigned total = base_len + needs_sep + name_len + 1u;
	if (total > dst_size)
	{
		return 0;
	}

	local_memcpy(dst, base, base_len);
	unsigned cursor = base_len;
	if (needs_sep)
	{
		dst[cursor++] = '/';
	}
	local_memcpy(dst + cursor, name, name_len);
	cursor += name_len;
	dst[cursor] = '\0';
	return 1;
}

static int local_fingerprint_equals(const TRootFingerprint *a, const TRootFingerprint *b)
{
	if (a == 0 || b == 0)
	{
		return 0;
	}
	return a->exists == b->exists
		&& a->is_dir == b->is_dir
		&& a->date == b->date
		&& a->time == b->time;
}

static TRootFingerprint local_capture_root_fingerprint(const char *path)
{
	TRootFingerprint fp;
	fp.date = 0u;
	fp.time = 0u;
	fp.exists = 0u;
	fp.is_dir = 0u;
	if (path == 0 || path[0] == '\0')
	{
		return fp;
	}

	FILINFO info;
	local_memset(&info, 0, sizeof(info));
	const FRESULT fr = f_stat(path, &info);
	if (fr == FR_OK)
	{
		fp.exists = 1u;
		fp.is_dir = ((info.fattrib & AM_DIR) != 0u) ? 1u : 0u;
		fp.date = info.fdate;
		fp.time = info.ftime;
	}
	return fp;
}

static void local_runtime_reset(void)
{
	s_EntryCount = 0u;
	s_RomCount = 0u;
	s_DiskCount = 0u;
	s_DirCount = 0u;
	s_FileCount = 0u;
	s_ErrorCount = 0u;
	s_DroppedCount = 0u;
	s_RootIndex = 0u;
	s_StackDepth = 0u;
	s_Scanning = FALSE;
	local_memset(s_Entries, 0, sizeof(s_Entries));
	local_memset(s_Stack, 0, sizeof(s_Stack));
	local_memset(s_RootNeedsScan, 0, sizeof(s_RootNeedsScan));
	local_memset(s_RootScanCache, 0, sizeof(s_RootScanCache));
}

static void local_clear_all_caches(void)
{
	local_memset(s_RootCache, 0, sizeof(s_RootCache));
	local_memset(s_RootScanCache, 0, sizeof(s_RootScanCache));
}

static void local_add_root_error(unsigned root_index)
{
	++s_ErrorCount;
	if (root_index < kRootCount)
	{
		++s_RootScanCache[root_index].error_count;
	}
}

static void local_add_root_dir(unsigned root_index)
{
	++s_DirCount;
	if (root_index < kRootCount)
	{
		++s_RootScanCache[root_index].dir_count;
	}
}

static void local_add_root_file(unsigned root_index)
{
	++s_FileCount;
	if (root_index < kRootCount)
	{
		++s_RootScanCache[root_index].file_count;
	}
}

static void local_count_media_type(unsigned char type, unsigned *rom, unsigned *disk)
{
	if (type == (unsigned char) ComboMediaTypeRom)
	{
		++(*rom);
	}
	else if (type == (unsigned char) ComboMediaTypeDisk)
	{
		++(*disk);
	}
}

static int local_append_entry(TComboMediaEntry *dst, unsigned *count, unsigned max_entries, const char *path, unsigned size_bytes, unsigned char type)
{
	if (dst == 0 || count == 0 || path == 0 || path[0] == '\0' || *count >= max_entries)
	{
		return 0;
	}

	TComboMediaEntry *entry = &dst[*count];
	local_memset(entry, 0, sizeof(*entry));
	const unsigned len = local_strlen(path);
	const unsigned copy_len = len < sizeof(entry->path) - 1u ? len : (sizeof(entry->path) - 1u);
	local_memcpy(entry->path, path, copy_len);
	entry->path[copy_len] = '\0';
	entry->size_bytes = size_bytes;
	entry->type = type;
	++(*count);
	return 1;
}

static void local_record_entry(unsigned root_index, const char *path, unsigned size_bytes, unsigned char type)
{
	const int appended_global = local_append_entry(s_Entries, &s_EntryCount, kCatalogMaxEntries, path, size_bytes, type);
	if (!appended_global)
	{
		++s_DroppedCount;
		if (root_index < kRootCount)
		{
			++s_RootScanCache[root_index].dropped_count;
		}
	}
	else
	{
		local_count_media_type(type, &s_RomCount, &s_DiskCount);
	}

	if (root_index < kRootCount)
	{
		if (!local_append_entry(s_RootScanCache[root_index].entries,
							   &s_RootScanCache[root_index].entry_count,
							   kCatalogMaxEntries,
							   path,
							   size_bytes,
							   type))
		{
			++s_RootScanCache[root_index].dropped_count;
		}
		local_count_media_type(type, &s_RootScanCache[root_index].rom_count, &s_RootScanCache[root_index].disk_count);
	}
}

static void local_replay_cached_root(unsigned root_index)
{
	if (root_index >= kRootCount || !s_RootCache[root_index].valid)
	{
		return;
	}

	const TRootCatalogCache *cache = &s_RootCache[root_index];
	for (unsigned i = 0u; i < cache->entry_count; ++i)
	{
		if (!local_append_entry(s_Entries, &s_EntryCount, kCatalogMaxEntries,
								cache->entries[i].path,
								cache->entries[i].size_bytes,
								cache->entries[i].type))
		{
			++s_DroppedCount;
		}
	}
	s_RomCount += cache->rom_count;
	s_DiskCount += cache->disk_count;
	s_DirCount += cache->dir_count;
	s_FileCount += cache->file_count;
	s_ErrorCount += cache->error_count;
	s_DroppedCount += cache->dropped_count;
}

static void local_pop_stack(void)
{
	if (s_StackDepth == 0u)
	{
		return;
	}
	f_closedir(&s_Stack[s_StackDepth - 1u].dir);
	--s_StackDepth;
}

static void local_finalize_scan_cache(void)
{
	for (unsigned i = 0u; i < kRootCount; ++i)
	{
		if (s_RootNeedsScan[i])
		{
			if (s_RootScanCache[i].valid)
			{
				s_RootCache[i] = s_RootScanCache[i];
				s_RootCache[i].valid = TRUE;
			}
			else
			{
				s_RootCache[i].valid = FALSE;
			}
		}
	}
}

static void local_open_next_root(void)
{
	while (s_RootIndex < kRootCount)
	{
		const unsigned root_index = s_RootIndex++;
		if (!s_RootNeedsScan[root_index])
		{
			continue;
		}

		const TCatalogRoot *root = &kCatalogRoots[root_index];
		if (s_StackDepth >= kCatalogMaxDepth)
		{
			local_add_root_error(root_index);
			s_Scanning = FALSE;
			local_finalize_scan_cache();
			return;
		}

		TCatalogStackNode *node = &s_Stack[s_StackDepth];
		local_memset(node, 0, sizeof(*node));
		const unsigned root_len = local_strlen(root->path);
		if (root_len >= sizeof(node->path))
		{
			local_add_root_error(root_index);
			continue;
		}
		local_memcpy(node->path, root->path, root_len + 1u);
		node->type = root->type;
		node->root_index = (unsigned char) root_index;

		const FRESULT fr = f_opendir(&node->dir, node->path);
		if (fr == FR_OK)
		{
			s_RootScanCache[root_index].valid = TRUE;
			++s_StackDepth;
			return;
		}

		if (fr != FR_NO_PATH && fr != FR_NO_FILE)
		{
			local_add_root_error(root_index);
		}
	}

	s_Scanning = FALSE;
	local_finalize_scan_cache();
}

static void local_ensure_root_dir(unsigned root_index, const char *path)
{
	if (path == 0 || path[0] == '\0')
	{
		return;
	}

	FILINFO info;
	local_memset(&info, 0, sizeof(info));
	FRESULT fr = f_stat(path, &info);
	if (fr == FR_OK)
	{
		if ((info.fattrib & AM_DIR) != 0u)
		{
			return;
		}
		local_add_root_error(root_index);
		return;
	}

	if (fr != FR_NO_FILE && fr != FR_NO_PATH)
	{
		local_add_root_error(root_index);
		return;
	}

	fr = f_mkdir(path);
	if (fr == FR_OK || fr == FR_EXIST)
	{
		return;
	}

	local_add_root_error(root_index);
}

void combo_media_catalog_reset(void)
{
	local_runtime_reset();
	local_clear_all_caches();
}

void combo_media_catalog_begin_scan(void)
{
	local_runtime_reset();

	for (unsigned i = 0u; i < kRootCount; ++i)
	{
		local_ensure_root_dir(i, kCatalogRoots[i].path);
	}

	for (unsigned i = 0u; i < kRootCount; ++i)
	{
		/* Always rescan roots to avoid stale FAT timestamp cache behavior on removable media. */
		const TRootFingerprint fp = local_capture_root_fingerprint(kCatalogRoots[i].path);
		s_RootScanCache[i].fp = fp;
		s_RootNeedsScan[i] = TRUE;
		s_RootScanCache[i].valid = FALSE;
	}

	boolean has_scan_work = FALSE;
	for (unsigned i = 0u; i < kRootCount; ++i)
	{
		if (s_RootNeedsScan[i])
		{
			has_scan_work = TRUE;
			break;
		}
	}

	if (!has_scan_work)
	{
		s_Scanning = FALSE;
		return;
	}

	s_Scanning = TRUE;
	local_open_next_root();
}

void combo_media_catalog_scan_step(unsigned budget_entries)
{
	if (!s_Scanning || budget_entries == 0u)
	{
		return;
	}

	while (budget_entries > 0u && s_Scanning)
	{
		if (s_StackDepth == 0u)
		{
			local_open_next_root();
			if (!s_Scanning)
			{
				break;
			}
		}

		TCatalogStackNode *node = &s_Stack[s_StackDepth - 1u];
		const unsigned root_index = node->root_index;
		FILINFO fno;
		local_memset(&fno, 0, sizeof(fno));
		const FRESULT fr = f_readdir(&node->dir, &fno);
		--budget_entries;

		if (fr != FR_OK)
		{
			local_add_root_error(root_index);
			local_pop_stack();
			continue;
		}

		if (fno.fname[0] == '\0')
		{
			local_pop_stack();
			continue;
		}

		if ((fno.fattrib & AM_DIR) != 0u)
		{
			if (local_starts_with_dot(fno.fname))
			{
				continue;
			}
			local_add_root_dir(root_index);
			if (s_StackDepth >= kCatalogMaxDepth)
			{
				local_add_root_error(root_index);
				continue;
			}

			TCatalogStackNode *child = &s_Stack[s_StackDepth];
			local_memset(child, 0, sizeof(*child));
			if (!local_join_path(child->path, sizeof(child->path), node->path, fno.fname))
			{
				local_add_root_error(root_index);
				continue;
			}
			child->type = node->type;
			child->root_index = (unsigned char) root_index;

			const FRESULT child_fr = f_opendir(&child->dir, child->path);
			if (child_fr == FR_OK)
			{
				local_record_entry(root_index, child->path, 0u, (unsigned char) ComboMediaTypeDir);
				++s_StackDepth;
			}
			else if (child_fr != FR_NO_PATH && child_fr != FR_NO_FILE)
			{
				local_add_root_error(root_index);
			}
			continue;
		}

		local_add_root_file(root_index);
		if (!local_is_supported_file(fno.fname, node->type))
		{
			continue;
		}

		char full_path[192];
		if (!local_join_path(full_path, sizeof(full_path), node->path, fno.fname))
		{
			local_add_root_error(root_index);
			continue;
		}

		local_record_entry(root_index, full_path, (unsigned) fno.fsize, node->type);
	}
}

boolean combo_media_catalog_is_scanning(void)
{
	return s_Scanning;
}

unsigned combo_media_catalog_entry_count(void)
{
	return s_EntryCount;
}

unsigned combo_media_catalog_rom_count(void)
{
	return s_RomCount;
}

unsigned combo_media_catalog_disk_count(void)
{
	return s_DiskCount;
}

unsigned combo_media_catalog_dir_count(void)
{
	return s_DirCount;
}

unsigned combo_media_catalog_file_count(void)
{
	return s_FileCount;
}

unsigned combo_media_catalog_error_count(void)
{
	return s_ErrorCount;
}

unsigned combo_media_catalog_dropped_count(void)
{
	return s_DroppedCount;
}

const TComboMediaEntry *combo_media_catalog_entries(void)
{
	return s_Entries;
}

const TComboMediaEntry *combo_media_catalog_entries_by_type(unsigned type, unsigned *count)
{
	if (count != 0)
	{
		*count = 0u;
	}

	for (unsigned i = 0u; i < kRootCount; ++i)
	{
		if (kCatalogRoots[i].type != (unsigned char) type)
		{
			continue;
		}
		if (!s_RootCache[i].valid)
		{
			return 0;
		}
		if (count != 0)
		{
			*count = s_RootCache[i].entry_count;
		}
		return s_RootCache[i].entries;
	}
	return 0;
}
