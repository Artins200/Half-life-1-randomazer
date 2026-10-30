#include "chaos_catalog.h"
#include <stdio.h>
#include <string.h>

static const ChaosEventDef g_baseEventDefs[CHAOS_BASE_EVENT_COUNT] = {
    // === MONSTERS & NPCS (0..11) ===
    {  0, "HEADCRAB AMBUSH!",        "Засада хедкрабов!",         "Вокруг может появиться до 3 хедкрабов!",              CHAOS_CAT_MONSTERS, CHAOS_TONE_DANGER,   0.0f, false, true },
    {  1, "BABY CRAB SWARM!",        "Рой мини-хедкрабов!",       "До трёх быстрых детёнышей хедкраба!",        CHAOS_CAT_MONSTERS, CHAOS_TONE_DANGER,   0.0f, false, true },
    {  2, "HOUNDEYE SONIC PACK!",    "Стая хаундаев!",            "Два звуковых пса Xen готовят ультразвуковую волну!", CHAOS_CAT_MONSTERS, CHAOS_TONE_DANGER,   0.0f, false, true },
    {  3, "BULLSQUID SURPRISE!",     "Кислотный буллсквид!",      "Рядом телепортировался голодный буллсквид!",         CHAOS_CAT_MONSTERS, CHAOS_TONE_DANGER,   0.0f, false, true },
    {  4, "ZOMBIE OUTBREAK!",        "Нашествие зомби!",          "Два зомбированных учёных вышли на охоту!",           CHAOS_CAT_MONSTERS, CHAOS_TONE_DANGER,   0.0f, false, true },
    {  5, "XEN VORTIGAUNT RAID!",    "Вторжение вортигонта!",     "Портал из Xen перенёс боевого вортигонта!",          CHAOS_CAT_MONSTERS, CHAOS_TONE_DANGER,   0.0f, false, true },
    {  6, "HECU MARINE DROP!",       "Десант спецназа HECU!",     "Пехотинец HECU высадился прямо поблизости!",         CHAOS_CAT_MONSTERS, CHAOS_TONE_DANGER,   0.0f, true, true },
    {  7, "ALIEN GRUNT HORNETS!",    "Пехотинец пришельцев!",     "Мощный Alien Grunt с мухомётом вышел из портала!",   CHAOS_CAT_MONSTERS, CHAOS_TONE_DANGER,   0.0f, true, true },
    {  8, "SNARK PARTY!",            "Вечеринка снарков!",        "До трёх кусачих снарков бегут грызть всех!",      CHAOS_CAT_MONSTERS, CHAOS_TONE_DANGER,   0.0f, false, true },
    {  9, "ROACH INFESTATION!",      "Нашествие тараканов!",      "До трёх шуршащих тараканов Чёрной Мезы!",             CHAOS_CAT_MONSTERS, CHAOS_TONE_WEIRD,    0.0f, false, true },
    { 10, "BARNEY REINFORCEMENT!",   "Подмога от Барни!",         "Дружелюбный охранник Барни пришёл прикрыть спину!",  CHAOS_CAT_MONSTERS, CHAOS_TONE_GOOD,     0.0f, false, true },
    { 11, "PANICKED SCIENTISTS!",    "Паника научного отдела!",   "Трое учёных появились рядом в полной панике!",       CHAOS_CAT_MONSTERS, CHAOS_TONE_WEIRD,    0.0f, false, true },

    // === PHYSICS & MOVEMENT (12..21) ===
    { 12, "MOON GRAVITY (10S)",      "Лунная гравитация!",        "Притяжение снижено в 5 раз на 10 секунд!",           CHAOS_CAT_PHYSICS,  CHAOS_TONE_NEUTRAL, 10.0f, false, true },
    { 13, "JUPITER GRAVITY (8S)",    "Гравитация Юпитера!",       "Тяжёлое притяжение x2 на 8 секунд!",                 CHAOS_CAT_PHYSICS,  CHAOS_TONE_DANGER,   8.0f, false, true },
    { 14, "TURBO FREEMAN (10S)",     "Турбо-ускорение Гордона!",  "Сверхзвуковой бег Гордона Фримена на 10 секунд!",    CHAOS_CAT_PHYSICS,  CHAOS_TONE_GOOD,    10.0f, false, true },
    { 15, "SNAIL PACE (7S)",         "Черепаший шаг!",            "Ноги налились свинцом — замедление на 7 секунд!",    CHAOS_CAT_PHYSICS,  CHAOS_TONE_DANGER,   7.0f, false, true },
    { 16, "ICE SKATING RINK (10S)",  "Гололёд в Чёрной Мезе!",    "Пол стал ледяным: скольжение как на коньках!",       CHAOS_CAT_PHYSICS,  CHAOS_TONE_NEUTRAL, 10.0f, false, true },
    { 17, "SUPER JUMP BOOTS (10S)",  "Пружинные ботинки!",        "Каждый прыжок подбрасывает к потолку (10 сек)!",     CHAOS_CAT_PHYSICS,  CHAOS_TONE_GOOD,    10.0f, false, true },
    { 18, "POGO STICK MODE (9S)",    "Режим кузнечика!",          "Гордон неудержимо прыгает каждые 1.1 секунды!",      CHAOS_CAT_PHYSICS,  CHAOS_TONE_WEIRD,    9.0f, false, true },
    { 19, "SKY LAUNCHPAD!",          "Катапульта в воздух!",      "Резкий подброс вверх с защитой от урона падения!",   CHAOS_CAT_PHYSICS,  CHAOS_TONE_NEUTRAL,  0.0f, false, true },
    { 20, "DEJA VU (REWIND 3.5S)",   "Дежавю!",                   "Возврат Гордона на позицию 3.5 секунды назад!",      CHAOS_CAT_PHYSICS,  CHAOS_TONE_WEIRD,    0.0f, false, true },
    { 21, "BLINK DASH FORWARD!",     "Телепорт-рывок вперёд!",    "Мгновенный блинк вперёд по направлению взгляда!",    CHAOS_CAT_PHYSICS,  CHAOS_TONE_NEUTRAL,  0.0f, false, true },

    // === WEAPONS & INVENTORY (22..27) ===
    { 22, "RANDOM WEAPON GIFT!",     "Случайная пушка!",          "Выдано случайное мощное оружие с боезапасом!",       CHAOS_CAT_WEAPONS,  CHAOS_TONE_GOOD,     0.0f, true, true },
    { 23, "FULL ARSENAL JACKPOT!",   "ДЖЕКПОТ: Полный арсенал!",  "Выданы все пушки и запас патронов!",                 CHAOS_CAT_WEAPONS,  CHAOS_TONE_GOOD,     0.0f, true, true },
    { 24, "CROWBAR ONLY!",           "Только монтировка!",        "Пушки отобраны — в руках только верная монтировка!", CHAOS_CAT_WEAPONS,  CHAOS_TONE_DANGER,   0.0f, true, true },
    { 25, "DEMOMAN LOADOUT!",        "Набор подрывника!",         "Только взрывчатка: RPG, гранаты, ранцы и мины!",     CHAOS_CAT_WEAPONS,  CHAOS_TONE_WEIRD,    0.0f, true, true },
    { 26, "SUPPLY DROP SHOWER!",     "Дождь припасов!",           "Аптечки, батареи HEV и патроны прямо в руки!",       CHAOS_CAT_WEAPONS,  CHAOS_TONE_GOOD,     0.0f, true, true },
    { 27, "ANOMALY BOMB (3S FUSE)!", "Аномальная бомба (3 сек)!", "Отойди с точки! Через 3 сек ударная волна!",         CHAOS_CAT_WEAPONS,  CHAOS_TONE_DANGER,   3.0f, false, true },

    // === HEALTH, ARMOR & SURVIVAL (28..32) ===
    { 28, "HEV OVERCHARGE!",         "Перегрузка костюма HEV!",   "100 HP и 150 единиц заряда брони костюма!",          CHAOS_CAT_SURVIVAL, CHAOS_TONE_GOOD,     0.0f, false, true },
    { 29, "GLASS CANNON (15/100)!",  "Хрустальная пушка!",        "15 здоровья, но 100 брони HEV — береги себя!",       CHAOS_CAT_SURVIVAL, CHAOS_TONE_DANGER,   0.0f, false, true },
    { 30, "NANO REGENERATION (10S)", "Нано-регенерация!",         "+7 HP и +7 брони каждую секунду на 10 секунд!",      CHAOS_CAT_SURVIVAL, CHAOS_TONE_GOOD,    10.0f, false, true },
    { 31, "HP <-> ARMOR SWAP!",      "Обмен ХП и брони местами!", "Здоровье и броня поменялись значениями!",            CHAOS_CAT_SURVIVAL, CHAOS_TONE_WEIRD,    0.0f, false, true },
    { 32, "GODMODE SHIELD (8S)!",    "Золотой щит неуязвимости!", "Полное бессмертие на 8 секунд — врывайся в бой!",    CHAOS_CAT_SURVIVAL, CHAOS_TONE_GOOD,     8.0f, false, true },

    // === VISUAL, CAMERA & WORLD CHAOS (33..37) ===
    { 33, "QUAKE PRO FOV (130)",     "Рыбий глаз Quake Pro!",     "Сверхширокий угол обзора FOV 130 на 10 секунд!",     CHAOS_CAT_VISUAL,   CHAOS_TONE_WEIRD,   10.0f, false, false },
    { 34, "SNIPER ZOOM (FOV 38)",    "Туннельное зрение!",        "Экстремальный зум камеры FOV 38 на 8 секунд!",       CHAOS_CAT_VISUAL,   CHAOS_TONE_WEIRD,    8.0f, false, false },
    { 35, "BARNEY'S BEER (DRUNK)",   "Пиво от Барни!",            "Камера пьяно раскачивается 10 секунд!",              CHAOS_CAT_VISUAL,   CHAOS_TONE_WEIRD,   10.0f, false, false },
    { 36, "RESONANCE EARTHQUAKE!",   "Каскадный резонанс!",       "Мощное землетрясение и подземные толчки 8 сек!",     CHAOS_CAT_VISUAL,   CHAOS_TONE_DANGER,   8.0f, false, true },
    { 37, "BLACK MESA DISCO RAVE!",  "Дискотека в Чёрной Мезе!",  "Неоновый рейв + все враги вокруг взлетают вверх!",   CHAOS_CAT_VISUAL,   CHAOS_TONE_WEIRD,   10.0f, false, true }
};

// These are original-game effects inspired by the references, not imported
// Minecraft, Counter-Strike, Opposing Force, or Portal assets.
static const ChaosEventDef g_specialDefs[CHAOS_SPECIAL_EVENT_COUNT] = {
    {38, "TOTEM OF SECOND CHANCES", "Тотем второго шанса", "Щит сработает при низком здоровье, пока Гордон жив.", CHAOS_CAT_SURVIVAL, CHAOS_TONE_GOOD, 0, false, true},
    {39, "APERTURE PORTAL DEVICE", "Портальное устройство", "Две точки и безопасный переход между ними на одной карте.", CHAOS_CAT_PHYSICS, CHAOS_TONE_GOOD, 0, false, true},
    {40, "SPEEDRUN STRAT", "Спидран-страта", "Короткое ускорение и прыжок — без изменения времени игры.", CHAOS_CAT_PHYSICS, CHAOS_TONE_GOOD, 8, false, true},
    {41, "NEW GAME? JUST KIDDING!", "Новая игра? Троллинг!", "Фальшивый экран перезапуска: настоящие сохранения не трогаем.", CHAOS_CAT_VISUAL, CHAOS_TONE_WEIRD, 0, false, false},
    {42, "OPFOR INSPIRED LOADOUT", "Набор в духе Opposing Force", "Выдаёт доступное штатное оружие HL1, без файлов Gearbox.", CHAOS_CAT_WEAPONS, CHAOS_TONE_GOOD, 0, true, true},
    {43, "CREEPER? AW MAN!", "Крипер? О нет!", "Трёхсекундная несмертельная аномальная волна.", CHAOS_CAT_VISUAL, CHAOS_TONE_DANGER, 3, false, true}
};

// New standalone events go after the recipe range to preserve every old ID.
static const ChaosEventDef g_extraDefs[CHAOS_EXTRA_EVENT_COUNT] = {
    {CHAOS_RECIPE_END, "CS 1.6 ANIME UPDATE?!", "Аниме-обновление CS 1.6?!",
     "Короткая шуточная анимация в HUD, без установки сторонних моделей.",
     CHAOS_CAT_VISUAL, CHAOS_TONE_WEIRD, 3, false, false}
};

struct Theme {
    const char *en;
    const char *ru;
    const char *description;
    ChaosCategory category;
    ChaosTone tone;
};
static const Theme g_themes[CHAOS_THEME_COUNT] = {
    {"TOTEM",          "Тотем",              "Заряжает защиту при низком HP", CHAOS_CAT_SURVIVAL, CHAOS_TONE_GOOD},
    {"DIAMOND PICKAXE", "Алмазная кирка",     "Монтировка вместо кирки и немного брони", CHAOS_CAT_WEAPONS, CHAOS_TONE_GOOD},
    {"CREEPER",        "Крипер",              "Аномальная волна через три секунды", CHAOS_CAT_VISUAL, CHAOS_TONE_DANGER},
    {"ENDER PEARL",    "Жемчуг Эндера",       "Проверенный рывок вперёд", CHAOS_CAT_PHYSICS, CHAOS_TONE_NEUTRAL},
    {"NETHER PORTAL",  "Портал Нижнего мира", "Фиолетовая вспышка и короткая защита", CHAOS_CAT_VISUAL, CHAOS_TONE_WEIRD},
    {"GOLDEN APPLE",   "Золотое яблоко",      "Восстанавливает здоровье и броню", CHAOS_CAT_SURVIVAL, CHAOS_TONE_GOOD},
    {"REDSTONE RUSH",  "Редстоун-ускорение",  "Ненадолго ускоряет игрока", CHAOS_CAT_PHYSICS, CHAOS_TONE_GOOD},
    {"BEDROCK",        "Бедрок",              "Повышает гравитацию", CHAOS_CAT_PHYSICS, CHAOS_TONE_DANGER},
    {"SKELETON LOOT",  "Лут скелета",         "Доступный арбалет и болты HL1", CHAOS_CAT_WEAPONS, CHAOS_TONE_GOOD},
    {"OVERWORLD",      "Верхний мир",         "Небольшое лечение и солнечная вспышка", CHAOS_CAT_SURVIVAL, CHAOS_TONE_GOOD},
    {"RUSH B",         "Раш B",               "Быстрый рывок без остановки", CHAOS_CAT_PHYSICS, CHAOS_TONE_GOOD},
    {"CS BUNNYHOP",    "Баннихоп",            "Пружинные ботинки и ускорение", CHAOS_CAT_PHYSICS, CHAOS_TONE_GOOD},
    {"ECO ROUND",      "Эко-раунд",           "Монтировка и небольшое лечение, оружие не отнимается", CHAOS_CAT_WEAPONS, CHAOS_TONE_NEUTRAL},
    {"FLASHBANG",      "Флешка",              "Короткая белая вспышка", CHAOS_CAT_VISUAL, CHAOS_TONE_WEIRD},
    {"BOMB PLANTED",   "Бомба заложена",      "Обратный отсчёт и безопасная волна", CHAOS_CAT_VISUAL, CHAOS_TONE_DANGER},
    {"AWP ZOOM",       "Прицел AWP",          "Временное приближение камеры", CHAOS_CAT_VISUAL, CHAOS_TONE_WEIRD},
    {"KNIFE ROUND",    "Раунд на ножах",      "Монтировка вместо ножа и короткий спринт", CHAOS_CAT_WEAPONS, CHAOS_TONE_NEUTRAL},
    {"SPEEDRUN",       "Спидран",             "Ускорение и прыжок", CHAOS_CAT_PHYSICS, CHAOS_TONE_GOOD},
    {"LONG JUMP",      "Длинный прыжок",      "Пружинные ботинки", CHAOS_CAT_PHYSICS, CHAOS_TONE_GOOD},
    {"XEN HOP",        "Прыжок Xen",          "Пониженная гравитация", CHAOS_CAT_PHYSICS, CHAOS_TONE_NEUTRAL},
    {"MEDIC",          "Медик",               "Регенерация здоровья", CHAOS_CAT_SURVIVAL, CHAOS_TONE_GOOD},
    {"APERTURE BLUE",  "Синий портал",        "Пытается установить синюю точку на стене перед игроком", CHAOS_CAT_PHYSICS, CHAOS_TONE_GOOD},
    {"APERTURE ORANGE","Оранжевый портал",   "Пытается установить оранжевую точку на стене перед игроком", CHAOS_CAT_PHYSICS, CHAOS_TONE_GOOD},
    {"OPFOR KIT",      "Набор Opposing Force", "Выдаёт доступные аналоги оружия HL1", CHAOS_CAT_WEAPONS, CHAOS_TONE_GOOD}
};

struct Variant { const char *en; const char *ru; };
static const Variant g_variants[CHAOS_VARIANT_COUNT] = {
    {"MOON", "лунная гравитация"}, {"HEAVY", "тяжёлая гравитация"},
    {"TURBO", "ускорение"}, {"SNAIL", "замедление"},
    {"ICE", "скользкий пол"}, {"BOUNCE", "высокий прыжок"},
    {"REGEN", "регенерация"}, {"SHIELD", "временный щит"},
    {"ZOOM", "приближение"}, {"WIDE FOV", "широкий угол обзора"},
    {"DRUNK", "покачивание камеры"}, {"QUAKE", "землетрясение"},
    {"DISCO", "неоновая дискотека"}, {"HEAL", "лечение"},
    {"ARMOR", "заряд брони"}, {"SUPPLIES", "припасы"}
};

static const Variant g_extraVariants[CHAOS_EXTRA_VARIANT_COUNT] = {
    {"B-HOP", "баннихоп и ускорение"},
    {"LAST STAND", "последний шанс: лечение и щит"}
};

static ChaosEventDef g_recipes[CHAOS_THEME_COUNT * CHAOS_VARIANT_COUNT];
static char g_titlesEn[CHAOS_THEME_COUNT * CHAOS_VARIANT_COUNT][80];
static char g_titlesRu[CHAOS_THEME_COUNT * CHAOS_VARIANT_COUNT][128];
static char g_descriptions[CHAOS_THEME_COUNT * CHAOS_VARIANT_COUNT][256];
static ChaosEventDef g_extraRecipes[CHAOS_THEME_COUNT * CHAOS_EXTRA_VARIANT_COUNT];
static char g_extraTitlesEn[CHAOS_THEME_COUNT * CHAOS_EXTRA_VARIANT_COUNT][80];
static char g_extraTitlesRu[CHAOS_THEME_COUNT * CHAOS_EXTRA_VARIANT_COUNT][128];
static char g_extraDescriptions[CHAOS_THEME_COUNT * CHAOS_EXTRA_VARIANT_COUNT][256];
static bool g_initialized = false;

static bool ThemeUsesInventory(int theme) {
    return theme == 1 || theme == 8 || theme == 12 || theme == 16 || theme == 23;
}

static bool IsTramVisualRecipe(int theme, int variant) {
    // Flash/zoom plus a camera or purely visual effect: none moves the player,
    // spawns entities, or interferes with the tram's scripts.
    const bool visualTheme = theme == 13 || theme == 15;
    const bool visualVariant = variant == 8 || variant == 9 || variant == 10 || variant == 12;
    return visualTheme && visualVariant;
}

static void InitRecipes() {
    if (g_initialized) return;
    for (int t = 0; t < CHAOS_THEME_COUNT; ++t) {
        for (int v = 0; v < CHAOS_VARIANT_COUNT; ++v) {
            int i = t * CHAOS_VARIANT_COUNT + v;
            snprintf(g_titlesEn[i], sizeof(g_titlesEn[i]), "%s + %s", g_themes[t].en, g_variants[v].en);
            snprintf(g_titlesRu[i], sizeof(g_titlesRu[i]), "%s + %s", g_themes[t].ru, g_variants[v].ru);
            snprintf(g_descriptions[i], sizeof(g_descriptions[i]), "%s; дополнительно: %s.",
                     g_themes[t].description, g_variants[v].ru);
            g_recipes[i] = {CHAOS_RECIPE_START + i, g_titlesEn[i], g_titlesRu[i], g_descriptions[i],
                            g_themes[t].category, g_themes[t].tone, 8.0f,
                            ThemeUsesInventory(t) || v == 15, !IsTramVisualRecipe(t, v)};
        }
        for (int v = 0; v < CHAOS_EXTRA_VARIANT_COUNT; ++v) {
            int i = t * CHAOS_EXTRA_VARIANT_COUNT + v;
            snprintf(g_extraTitlesEn[i], sizeof(g_extraTitlesEn[i]), "%s + %s", g_themes[t].en, g_extraVariants[v].en);
            snprintf(g_extraTitlesRu[i], sizeof(g_extraTitlesRu[i]), "%s + %s", g_themes[t].ru, g_extraVariants[v].ru);
            snprintf(g_extraDescriptions[i], sizeof(g_extraDescriptions[i]), "%s; дополнительно: %s.",
                     g_themes[t].description, g_extraVariants[v].ru);
            g_extraRecipes[i] = {CHAOS_EXTENSION_START + i, g_extraTitlesEn[i], g_extraTitlesRu[i], g_extraDescriptions[i],
                                 g_themes[t].category, g_themes[t].tone, 8.0f,
                                 ThemeUsesInventory(t), true};
        }
    }
    g_initialized = true;
}

const ChaosEventDef *ChaosGetEventDef(int id) {
    if (id < 0 || id >= CHAOS_EVENT_COUNT) return nullptr;
    if (id < CHAOS_BASE_EVENT_COUNT) return &g_baseEventDefs[id];
    if (id < CHAOS_RECIPE_START) return &g_specialDefs[id - CHAOS_BASE_EVENT_COUNT];
    if (id < CHAOS_RECIPE_END) {
        InitRecipes();
        return &g_recipes[id - CHAOS_RECIPE_START];
    }
    if (id < CHAOS_EXTENSION_START) return &g_extraDefs[id - CHAOS_RECIPE_END];
    InitRecipes();
    return &g_extraRecipes[id - CHAOS_EXTENSION_START];
}
int ChaosRecipeTheme(int id) {
    if (id >= CHAOS_RECIPE_START && id < CHAOS_RECIPE_END)
        return (id - CHAOS_RECIPE_START) / CHAOS_VARIANT_COUNT;
    if (id >= CHAOS_EXTENSION_START && id < CHAOS_EVENT_COUNT)
        return (id - CHAOS_EXTENSION_START) / CHAOS_EXTRA_VARIANT_COUNT;
    return -1;
}
int ChaosRecipeVariant(int id) {
    if (id >= CHAOS_RECIPE_START && id < CHAOS_RECIPE_END)
        return (id - CHAOS_RECIPE_START) % CHAOS_VARIANT_COUNT;
    if (id >= CHAOS_EXTENSION_START && id < CHAOS_EVENT_COUNT)
        return CHAOS_VARIANT_COUNT + (id - CHAOS_EXTENSION_START) % CHAOS_EXTRA_VARIANT_COUNT;
    return -1;
}
