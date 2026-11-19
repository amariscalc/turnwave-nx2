#include <switch.h>
#include <stdio.h>

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

int main(void) {
    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    consoleInit(NULL);
    printf("\n  TURNWAVE NX2\n\n"
           "  music.apple.com/es/\n\n"
           "  A: Open Apple Music   +: Back to Homebrew Menu\n");
    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 down = padGetButtonsDown(&pad);
        if (down & HidNpadButton_Plus) break;
        if (down & HidNpadButton_A) {
            Result result = open_apple_music();
            consoleClear();
            printf("\n  TURNWAVE NX2\n\n");
            if (R_FAILED(result)) printf("  Web applet error: 0x%08x\n\n", (unsigned)result);
            printf("  A: Open Apple Music   +: Back to Homebrew Menu\n");
        }
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    return 0;
}
