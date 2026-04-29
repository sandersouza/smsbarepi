#ifndef SMSBARE_CIRCLE_SMSBARE_MEDIA_CATALOG_H
#define SMSBARE_CIRCLE_SMSBARE_MEDIA_CATALOG_H

#include <circle/types.h>

enum TComboMediaType
{
	ComboMediaTypeUnknown = 0,
	ComboMediaTypeRom = 1,
	ComboMediaTypeDisk = 2,
	ComboMediaTypeDir = 3
};

struct TComboMediaEntry
{
	char path[192];
	unsigned size_bytes;
	unsigned char type;
};

void combo_media_catalog_reset(void);
void combo_media_catalog_begin_scan(void);
void combo_media_catalog_scan_step(unsigned budget_entries);
boolean combo_media_catalog_is_scanning(void);
unsigned combo_media_catalog_entry_count(void);
unsigned combo_media_catalog_rom_count(void);
unsigned combo_media_catalog_disk_count(void);
unsigned combo_media_catalog_dir_count(void);
unsigned combo_media_catalog_file_count(void);
unsigned combo_media_catalog_error_count(void);
unsigned combo_media_catalog_dropped_count(void);
const TComboMediaEntry *combo_media_catalog_entries(void);
const TComboMediaEntry *combo_media_catalog_entries_by_type(unsigned type, unsigned *count);

#endif
