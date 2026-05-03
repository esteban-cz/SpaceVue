#include <switch.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>

#ifndef VERSION_MAJOR
#define VERSION_MAJOR 3
#endif

#ifndef VERSION_MINOR
#define VERSION_MINOR 0
#endif

#ifndef VERSION_MICRO
#define VERSION_MICRO 0
#endif

static constexpr int SCREEN_W = 1280;
static constexpr int SCREEN_H = 720;
static constexpr double GIB = 1024.0 * 1024.0 * 1024.0;
static constexpr u64 REFRESH_INTERVAL_NS = 1000000000ULL;

struct StorageStats {
    const char* label;
    bool mounted;
    Result mount_result;
    Result total_result;
    Result free_result;
    s64 total;
    s64 free;
};

struct Fonts {
    TTF_Font* title;
    TTF_Font* section;
    TTF_Font* body;
    TTF_Font* small;
};

static constexpr SDL_Color COLOR_BG = {18, 22, 29, 255};
static constexpr SDL_Color COLOR_PANEL = {30, 36, 46, 255};
static constexpr SDL_Color COLOR_PANEL_2 = {38, 45, 57, 255};
static constexpr SDL_Color COLOR_TEXT = {239, 243, 248, 255};
static constexpr SDL_Color COLOR_MUTED = {151, 162, 178, 255};
static constexpr SDL_Color COLOR_DIM = {94, 105, 120, 255};
static constexpr SDL_Color COLOR_ACCENT = {73, 190, 170, 255};
static constexpr SDL_Color COLOR_WARN = {238, 184, 82, 255};
static constexpr SDL_Color COLOR_DANGER = {231, 91, 91, 255};
static constexpr SDL_Color COLOR_BAR_BG = {52, 61, 75, 255};

static void set_color(SDL_Renderer* renderer, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

static double bytes_to_gib(s64 bytes) {
    return (double)bytes / GIB;
}

static s64 used_bytes(const StorageStats& stats) {
    if (stats.total <= 0 || stats.free < 0 || stats.free > stats.total) {
        return 0;
    }

    return stats.total - stats.free;
}

static int used_percent(const StorageStats& stats) {
    if (stats.total <= 0) {
        return 0;
    }

    return (int)(((double)used_bytes(stats) * 100.0 / (double)stats.total) + 0.5);
}

static const char* health_label(const StorageStats& stats) {
    if (!stats.mounted || R_FAILED(stats.total_result) || R_FAILED(stats.free_result) || stats.total <= 0) {
        return "UNAVAILABLE";
    }

    const int free_pct = 100 - used_percent(stats);
    if (free_pct <= 5) {
        return "CRITICAL";
    }
    if (free_pct <= 15) {
        return "LOW";
    }

    return "OK";
}

static SDL_Color health_color(const StorageStats& stats) {
    const char* label = health_label(stats);

    if (label[0] == 'O') {
        return COLOR_ACCENT;
    }
    if (label[0] == 'L') {
        return COLOR_WARN;
    }
    if (label[0] == 'C') {
        return COLOR_DANGER;
    }

    return COLOR_DIM;
}

static Result read_space(FsFileSystem* fs, StorageStats* stats) {
    stats->total_result = fsFsGetTotalSpace(fs, "/", &stats->total);
    if (R_FAILED(stats->total_result)) {
        return stats->total_result;
    }

    stats->free_result = fsFsGetFreeSpace(fs, "/", &stats->free);
    return stats->free_result;
}

static StorageStats read_internal_storage() {
    StorageStats stats = {
        "Internal NAND",
        false,
        0,
        0,
        0,
        0,
        0,
    };

    FsFileSystem fs;
    stats.mount_result = fsOpenBisFileSystem(&fs, FsBisPartitionId_User, "");
    if (R_SUCCEEDED(stats.mount_result)) {
        stats.mounted = true;
        read_space(&fs, &stats);
        fsFsClose(&fs);
    }

    return stats;
}

static StorageStats read_sd_storage() {
    StorageStats stats = {
        "microSD",
        false,
        0,
        0,
        0,
        0,
        0,
    };

    FsFileSystem fs;
    stats.mount_result = fsOpenSdCardFileSystem(&fs);
    if (R_SUCCEEDED(stats.mount_result)) {
        stats.mounted = true;
        read_space(&fs, &stats);
        fsFsClose(&fs);
    }

    return stats;
}

static void draw_text(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color color) {
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) {
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect dst = {x, y, surface->w, surface->h};
    SDL_FreeSurface(surface);

    if (!texture) {
        return;
    }

    SDL_RenderCopy(renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
}

static void draw_text_right(SDL_Renderer* renderer, TTF_Font* font, const char* text, int right, int y, SDL_Color color) {
    int w = 0;
    int h = 0;
    TTF_SizeUTF8(font, text, &w, &h);
    draw_text(renderer, font, text, right - w, y, color);
}

static void fill_rect(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color) {
    set_color(renderer, color);
    SDL_RenderFillRect(renderer, &rect);
}

static void fill_rounded_rect(SDL_Renderer* renderer, SDL_Rect rect, int radius, SDL_Color color) {
    set_color(renderer, color);

    SDL_Rect center = {rect.x + radius, rect.y, rect.w - radius * 2, rect.h};
    SDL_Rect left = {rect.x, rect.y + radius, radius, rect.h - radius * 2};
    SDL_Rect right = {rect.x + rect.w - radius, rect.y + radius, radius, rect.h - radius * 2};

    SDL_RenderFillRect(renderer, &center);
    SDL_RenderFillRect(renderer, &left);
    SDL_RenderFillRect(renderer, &right);

    for (int dy = 0; dy < radius; dy++) {
        for (int dx = 0; dx < radius; dx++) {
            const int dist = dx * dx + dy * dy;
            if (dist <= radius * radius) {
                SDL_RenderDrawPoint(renderer, rect.x + radius - dx, rect.y + radius - dy);
                SDL_RenderDrawPoint(renderer, rect.x + rect.w - radius + dx - 1, rect.y + radius - dy);
                SDL_RenderDrawPoint(renderer, rect.x + radius - dx, rect.y + rect.h - radius + dy - 1);
                SDL_RenderDrawPoint(renderer, rect.x + rect.w - radius + dx - 1, rect.y + rect.h - radius + dy - 1);
            }
        }
    }
}

static void draw_badge(SDL_Renderer* renderer, Fonts* fonts, const StorageStats& stats, int x, int y) {
    const char* label = health_label(stats);
    SDL_Color color = health_color(stats);
    int text_w = 0;
    int text_h = 0;
    TTF_SizeUTF8(fonts->small, label, &text_w, &text_h);

    SDL_Rect badge = {x, y, text_w + 34, 34};
    fill_rounded_rect(renderer, badge, 8, color);
    draw_text(renderer, fonts->small, label, x + 17, y + 7, COLOR_BG);
}

static void draw_progress(SDL_Renderer* renderer, int x, int y, int width, int percent, SDL_Color color) {
    SDL_Rect track = {x, y, width, 22};
    fill_rounded_rect(renderer, track, 8, COLOR_BAR_BG);

    int fill_w = (width * percent) / 100;
    if (fill_w < 16) {
        fill_w = percent > 0 ? 16 : 0;
    }
    if (fill_w > width) {
        fill_w = width;
    }

    if (fill_w > 0) {
        SDL_Rect fill = {x, y, fill_w, 22};
        fill_rounded_rect(renderer, fill, 8, color);
    }
}

static void draw_storage_panel(SDL_Renderer* renderer, Fonts* fonts, const StorageStats& stats, SDL_Rect panel) {
    fill_rounded_rect(renderer, panel, 8, COLOR_PANEL);

    SDL_Rect top = {panel.x, panel.y, panel.w, 74};
    fill_rounded_rect(renderer, top, 8, COLOR_PANEL_2);

    draw_text(renderer, fonts->section, stats.label, panel.x + 32, panel.y + 24, COLOR_TEXT);
    draw_badge(renderer, fonts, stats, panel.x + panel.w - 170, panel.y + 22);

    if (!stats.mounted) {
        char error[64];
        snprintf(error, sizeof(error), "Mount error 0x%08x", (unsigned int)stats.mount_result);
        draw_text(renderer, fonts->body, "Storage is not available.", panel.x + 32, panel.y + 124, COLOR_TEXT);
        draw_text(renderer, fonts->small, error, panel.x + 32, panel.y + 176, COLOR_MUTED);
        return;
    }

    if (R_FAILED(stats.total_result) || R_FAILED(stats.free_result) || stats.total <= 0) {
        char error[96];
        snprintf(error, sizeof(error), "Read error 0x%08x / 0x%08x",
                 (unsigned int)stats.total_result,
                 (unsigned int)stats.free_result);
        draw_text(renderer, fonts->body, "Could not read storage size.", panel.x + 32, panel.y + 124, COLOR_TEXT);
        draw_text(renderer, fonts->small, error, panel.x + 32, panel.y + 176, COLOR_MUTED);
        return;
    }

    char free_text[64];
    char used_text[64];
    char total_text[64];
    char percent_text[32];
    const int percent = used_percent(stats);

    snprintf(free_text, sizeof(free_text), "%.2f GiB", bytes_to_gib(stats.free));
    snprintf(used_text, sizeof(used_text), "%.2f GiB used", bytes_to_gib(used_bytes(stats)));
    snprintf(total_text, sizeof(total_text), "of %.2f GiB total", bytes_to_gib(stats.total));
    snprintf(percent_text, sizeof(percent_text), "%d%%", percent);

    draw_text(renderer, fonts->small, "FREE SPACE", panel.x + 32, panel.y + 114, COLOR_MUTED);
    draw_text(renderer, fonts->title, free_text, panel.x + 32, panel.y + 142, COLOR_TEXT);
    draw_text(renderer, fonts->body, total_text, panel.x + 34, panel.y + 204, COLOR_MUTED);

    draw_text_right(renderer, fonts->section, percent_text, panel.x + panel.w - 32, panel.y + 144, health_color(stats));
    draw_text_right(renderer, fonts->small, used_text, panel.x + panel.w - 32, panel.y + 204, COLOR_MUTED);

    draw_progress(renderer, panel.x + 32, panel.y + 260, panel.w - 64, percent, health_color(stats));
}

static void render_dashboard(SDL_Renderer* renderer, Fonts* fonts, int refreshes, const StorageStats& internal, const StorageStats& sd) {
    set_color(renderer, COLOR_BG);
    SDL_RenderClear(renderer);

    fill_rect(renderer, {0, 0, SCREEN_W, 96}, {22, 28, 37, 255});
    fill_rect(renderer, {0, 96, SCREEN_W, 2}, {61, 72, 88, 255});

    char version[64];
    char refresh[48];
    snprintf(version, sizeof(version), "SpaceVue v%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO);
    snprintf(refresh, sizeof(refresh), "Refresh #%d", refreshes);

    draw_text(renderer, fonts->title, version, 58, 24, COLOR_TEXT);
    draw_text(renderer, fonts->small, "made by estyxq", 60, 62, COLOR_MUTED);
    draw_text_right(renderer, fonts->body, "A Refresh    + Exit", SCREEN_W - 58, 24, COLOR_MUTED);

    SDL_Rect internal_panel = {58, 134, 558, 350};
    SDL_Rect sd_panel = {664, 134, 558, 350};
    draw_storage_panel(renderer, fonts, internal, internal_panel);
    draw_storage_panel(renderer, fonts, sd, sd_panel);

    SDL_Rect footer = {58, 526, 1164, 104};
    fill_rounded_rect(renderer, footer, 8, {25, 31, 40, 255});
    draw_text(renderer, fonts->section, "Overview", 90, 556, COLOR_TEXT);
    draw_text(renderer, fonts->body, "Live storage stats with a lightweight SDL2 interface.", 90, 596, COLOR_MUTED);
    draw_text_right(renderer, fonts->body, refresh, 1190, 556, COLOR_ACCENT);
    draw_text_right(renderer, fonts->small, "Values use GiB", 1190, 602, COLOR_DIM);

    SDL_RenderPresent(renderer);
}

static bool load_fonts(Fonts* fonts) {
    const char* font_path = "romfs:/data/LeroyLetteringLightBeta01.ttf";
    fonts->title = TTF_OpenFont(font_path, 34);
    fonts->section = TTF_OpenFont(font_path, 24);
    fonts->body = TTF_OpenFont(font_path, 20);
    fonts->small = TTF_OpenFont(font_path, 16);

    return fonts->title && fonts->section && fonts->body && fonts->small;
}

static void close_fonts(Fonts* fonts) {
    if (fonts->title) {
        TTF_CloseFont(fonts->title);
    }
    if (fonts->section) {
        TTF_CloseFont(fonts->section);
    }
    if (fonts->body) {
        TTF_CloseFont(fonts->body);
    }
    if (fonts->small) {
        TTF_CloseFont(fonts->small);
    }
}

int main(int argc, char* argv[]) {
    Result rc = romfsInit();
    if (R_FAILED(rc)) {
        return -1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        romfsExit();
        return -1;
    }

    if (TTF_Init() < 0) {
        SDL_Quit();
        romfsExit();
        return -1;
    }

    SDL_Window* window = SDL_CreateWindow("SpaceVue", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN);
    if (!window) {
        TTF_Quit();
        SDL_Quit();
        romfsExit();
        return -1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        romfsExit();
        return -1;
    }

    Fonts fonts = {};
    if (!load_fonts(&fonts)) {
        close_fonts(&fonts);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        romfsExit();
        return -1;
    }

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    StorageStats internal = read_internal_storage();
    StorageStats sd = read_sd_storage();
    int refreshes = 1;
    u64 last_refresh = armGetSystemTick();

    render_dashboard(renderer, &fonts, refreshes, internal, sd);

    while (appletMainLoop()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                close_fonts(&fonts);
                SDL_DestroyRenderer(renderer);
                SDL_DestroyWindow(window);
                TTF_Quit();
                SDL_Quit();
                romfsExit();
                return 0;
            }
        }

        padUpdate(&pad);
        const u64 down = padGetButtonsDown(&pad);

        if (down & HidNpadButton_Plus) {
            break;
        }

        const u64 now = armGetSystemTick();
        const bool manual_refresh = (down & HidNpadButton_A) != 0;
        const bool auto_refresh = armTicksToNs(now - last_refresh) >= REFRESH_INTERVAL_NS;

        if (manual_refresh || auto_refresh) {
            internal = read_internal_storage();
            sd = read_sd_storage();
            last_refresh = now;
            refreshes++;
        }

        render_dashboard(renderer, &fonts, refreshes, internal, sd);
    }

    close_fonts(&fonts);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    romfsExit();
    return 0;
}
