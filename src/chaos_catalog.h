#ifndef CHAOS_CATALOG_H
#define CHAOS_CATALOG_H

#include "chaos_types.h"

// IDs 0..37 stay stable for old binds. Recipes are real combinations of a
// primary action and a secondary modifier, not 400 independent mechanics.
#define CHAOS_BASE_EVENT_COUNT 38
#define CHAOS_SPECIAL_EVENT_COUNT 6
#define CHAOS_THEME_COUNT 24
#define CHAOS_VARIANT_COUNT 16
#define CHAOS_RECIPE_START (CHAOS_BASE_EVENT_COUNT + CHAOS_SPECIAL_EVENT_COUNT)
#define CHAOS_RECIPE_END (CHAOS_RECIPE_START + CHAOS_THEME_COUNT * CHAOS_VARIANT_COUNT)
// Append new events and recipe variants after the old range: existing IDs
// and binds (including the standalone CS meme) remain unchanged.
#define CHAOS_EXTRA_EVENT_COUNT 1
#define CHAOS_EXTENSION_START (CHAOS_RECIPE_END + CHAOS_EXTRA_EVENT_COUNT)
#define CHAOS_EXTRA_VARIANT_COUNT 2
#define CHAOS_EVENT_COUNT (CHAOS_EXTENSION_START + CHAOS_THEME_COUNT * CHAOS_EXTRA_VARIANT_COUNT)

const ChaosEventDef *ChaosGetEventDef(int id);
int ChaosRecipeTheme(int id);
int ChaosRecipeVariant(int id);

#endif
