#ifndef CHAOS_FEATURES_H
#define CHAOS_FEATURES_H

#include <stddef.h>

// GoldSrc may age an existing save into <slot>01.sav even when creating a
// different named slot. Check both names before requesting an engine save.
enum ChaosSaveSlotResult {
    CHAOS_SAVE_SLOT_OK,
    CHAOS_SAVE_SLOT_UNAVAILABLE,
    CHAOS_SAVE_SLOT_CHECK_FAILED
};
ChaosSaveSlotResult ChaosFindUnusedSaveSlot(const char *gameDir, unsigned long timestamp,
                                           char *name, size_t nameSize);

#endif
