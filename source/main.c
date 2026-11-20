#include <switch.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

enum {
    SCREEN_WIDTH = 1280,
    SCREEN_HEIGHT = 720,
    STATUS_X = 100,
    STATUS_Y = 515,
    STATUS_WIDTH = 511,
    STATUS_HEIGHT = 61,
    ERROR_GLYPH_WIDTH = 12,
    ERROR_GLYPH_HEIGHT = 18,
    PIXEL_BYTES = 4,
    SWITCH2_OPENWEB_APPLET_ID = 0x01000013,
    SWITCH2_ERROR_APPLET_ID = 0x0100000E,
};

extern const u8 ui_home[];
extern const u8 ui_opening[];
extern const u8 ui_failed[];
extern const u8 ui_hex[];
static FILE *diagnostics;

static void record_result(const char *stage, Result result) {
    if (!diagnostics) return;
    fprintf(diagnostics, "%s: 0x%08X\n", stage, (unsigned)result);
    fflush(diagnostics);
}

static bool present(Framebuffer *fb, const u8 *status, Result result) {
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
    if (status == ui_failed) {
        for (unsigned digit = 0; digit < 8; ++digit) {
            unsigned value = (result >> ((7 - digit) * 4)) & 0xF;
            const u8 *glyph = ui_hex + value * ERROR_GLYPH_WIDTH * ERROR_GLYPH_HEIGHT;
            for (unsigned y = 0; y < ERROR_GLYPH_HEIGHT; ++y) {
                for (unsigned x = 0; x < ERROR_GLYPH_WIDTH; ++x) {
                    unsigned alpha = glyph[y * ERROR_GLYPH_WIDTH + x];
                    u8 *pixel = pixels + (550 + y) * stride
                        + (180 + digit * ERROR_GLYPH_WIDTH + x) * PIXEL_BYTES;
                    for (unsigned channel = 0; channel < 3; ++channel) {
                        pixel[channel] = (255 * alpha + pixel[channel] * (255 - alpha)) / 255;
                    }
                }
            }
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
    record_result("webPageCreate", result);
    if (R_SUCCEEDED(result)) {
        // Switch 2 uses the OpenWeb AppletId, distinct from Switch 1's 0x13.
        config.appletid = (AppletId)SWITCH2_OPENWEB_APPLET_ID;
        result = webConfigSetWhitelist(&config, allowed);
        record_result("webConfigSetWhitelist", result);
    }
    if (R_SUCCEEDED(result)) {
        result = webConfigSetTouchEnabledOnContents(&config, true);
        record_result("webConfigSetTouchEnabledOnContents", result);
    }
    if (R_SUCCEEDED(result)) {
        result = webConfigSetWebAudio(&config, true);
        record_result("webConfigSetWebAudio", result);
    }
    if (R_SUCCEEDED(result)) {
        WebCommonReply reply = {0};
        record_result("opening OpenWeb 0x01000013", 0);
        result = webConfigShow(&config, &reply);
        record_result("webConfigShow", result);
        if (R_SUCCEEDED(result)) {
            WebExitReason reason;
            result = webReplyGetExitReason(&reply, &reason);
            record_result("webReplyGetExitReason", result);
            if (R_SUCCEEDED(result)) {
                if (diagnostics) {
                    fprintf(diagnostics, "web exit reason: %u\n", (unsigned)reason);
                    fflush(diagnostics);
                }
                if (reason == WebExitReason_ErrorDialog) {
                    result = MAKERESULT(Module_Libnx, LibnxError_LibAppletBadExit);
                }
            }
        }
    }
    return result;
}

static void show_error(const char *description, Result result) {
    char message[128];
    snprintf(message, sizeof message, "%s (0x%08X)", description, (unsigned)result);
    ErrorSystemConfig dialog;
    record_result(description, result);
    if (R_FAILED(errorSystemCreate(&dialog, message, NULL))) return;
    AppletHolder holder = {0};
    Result rc = appletCreateLibraryApplet(&holder, (AppletId)SWITCH2_ERROR_APPLET_ID, LibAppletMode_AllForeground);
    if (R_SUCCEEDED(rc)) {
        LibAppletArgs args;
        libappletArgsCreate(&args, 0);
        rc = libappletArgsPush(&args, &holder);
    }
    if (R_SUCCEEDED(rc)) rc = libappletPushInData(&holder, &dialog.arg, sizeof(dialog.arg));
    if (R_SUCCEEDED(rc)) rc = appletHolderStart(&holder);
    if (R_SUCCEEDED(rc)) appletHolderJoin(&holder);
    appletHolderClose(&holder);
}

int main(void) {
    mkdir("sdmc:/switch", 0777);
    mkdir("sdmc:/switch/TurnwaveNX2", 0777);
    diagnostics = fopen("sdmc:/switch/TurnwaveNX2/diagnostics.log", "w");
    if (diagnostics) {
        fprintf(diagnostics, "Turnwave NX2 1.3.1 / OpenWeb 0x01000013 / HOS 0x%08X / role %u\n",
                (unsigned)hosversionGet(), (unsigned)appletGetAppletType());
        fflush(diagnostics);
    }
    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    Framebuffer fb = {0};
    Result rc = framebufferCreate(&fb, nwindowGetDefault(), SCREEN_WIDTH, SCREEN_HEIGHT,
                                  PIXEL_FORMAT_RGBA_8888, 2);
    if (R_FAILED(rc)) {
        show_error("Could not initialize the display", rc);
        if (diagnostics) fclose(diagnostics);
        return 1;
    }
    rc = framebufferMakeLinear(&fb);
    if (R_FAILED(rc)) {
        framebufferClose(&fb);
        show_error("Could not prepare the display", rc);
        if (diagnostics) fclose(diagnostics);
        return 1;
    }

    bool visible = present(&fb, NULL, 0);
    while (visible && appletMainLoop()) {
        padUpdate(&pad);
        u64 down = padGetButtonsDown(&pad);
        if (down & (HidNpadButton_Plus | HidNpadButton_B)) break;
        if (down & HidNpadButton_A) {
            visible = present(&fb, ui_opening, 0);
            if (!visible) break;
            rc = open_apple_music();
            if (!appletMainLoop()) break;
            visible = present(&fb, R_FAILED(rc) ? ui_failed : NULL, rc);
        }
        svcSleepThread(15000000);
    }
    framebufferClose(&fb);
    if (diagnostics) fclose(diagnostics);
    return visible ? 0 : 1;
}
