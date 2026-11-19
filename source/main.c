#include <switch.h>
#include <stdio.h>
#include <string.h>

enum {
    SCREEN_WIDTH = 1280,
    SCREEN_HEIGHT = 720,
    STATUS_X = 100,
    STATUS_Y = 515,
    STATUS_WIDTH = 511,
    STATUS_HEIGHT = 41,
    PIXEL_BYTES = 4,
};

extern const u8 ui_home[];
extern const u8 ui_opening[];
extern const u8 ui_failed[];

static bool present(Framebuffer *fb, const u8 *status) {
    u32 stride;
    u8 *pixels = framebufferBegin(fb, &stride);
    if (!pixels) return false;

    for (size_t y = 0; y < SCREEN_HEIGHT; ++y) {
        memcpy(pixels + y * stride, ui_home + y * SCREEN_WIDTH * PIXEL_BYTES,
               SCREEN_WIDTH * PIXEL_BYTES);
    }
    if (status) {
        for (size_t y = 0; y < STATUS_HEIGHT; ++y) {
            memcpy(pixels + (STATUS_Y + y) * stride + STATUS_X * PIXEL_BYTES,
                   status + y * STATUS_WIDTH * PIXEL_BYTES, STATUS_WIDTH * PIXEL_BYTES);
        }
    }
    framebufferEnd(fb);
    return true;
}

static Result open_apple_music(void) {
    static const char url[] = "https://music.apple.com/es/";
    static const char allowed[] =
        "^https://([A-Za-z0-9-]+[.])*apple[.]com(/|$)\n"
        "^https://([A-Za-z0-9-]+[.])*apple-cloudkit[.]com(/|$)\n"
        "^https://([A-Za-z0-9-]+[.])*mzstatic[.]com(/|$)\n";
    WebCommonConfig config;
    Result result = webPageCreate(&config, url);
    if (R_SUCCEEDED(result)) result = webConfigSetWhitelist(&config, allowed);
    if (R_SUCCEEDED(result)) {
        WebCommonReply reply;
        result = webConfigShow(&config, &reply);
    }
    return result;
}

static void show_error(const char *description, Result result) {
    char message[128];
    snprintf(message, sizeof message, "%s (0x%08X)", description, (unsigned)result);
    ErrorSystemConfig dialog;
    if (R_SUCCEEDED(errorSystemCreate(&dialog, message, NULL))) {
        errorSystemShow(&dialog);
    }
}

int main(void) {
    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    Framebuffer fb = {0};
    Result rc = framebufferCreate(&fb, nwindowGetDefault(), SCREEN_WIDTH, SCREEN_HEIGHT,
                                  PIXEL_FORMAT_RGBA_8888, 2);
    if (R_FAILED(rc)) {
        show_error("Could not initialize the display", rc);
        return 1;
    }
    rc = framebufferMakeLinear(&fb);
    if (R_FAILED(rc)) {
        framebufferClose(&fb);
        show_error("Could not prepare the display", rc);
        return 1;
    }

    bool visible = present(&fb, NULL);
    while (visible && appletMainLoop()) {
        padUpdate(&pad);
        u64 down = padGetButtonsDown(&pad);
        if (down & (HidNpadButton_Plus | HidNpadButton_B)) break;
        if (down & HidNpadButton_A) {
            visible = present(&fb, ui_opening);
            if (!visible) break;
            rc = open_apple_music();
            if (!appletMainLoop()) break;
            if (R_FAILED(rc)) show_error("Could not open the web applet", rc);
            visible = present(&fb, R_FAILED(rc) ? ui_failed : NULL);
        }
        svcSleepThread(15000000);
    }
    framebufferClose(&fb);
    return visible ? 0 : 1;
}
