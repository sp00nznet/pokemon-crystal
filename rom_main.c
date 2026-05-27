/* Main entry point */
#include "rom.h"
#include "gbrt.h"
#include "hwtrace.h"
#ifdef GB_HAS_SDL2
#include "platform_sdl.h"
#ifdef LA_HAS_MULTIPLAYER
#include "multiplayer/mp_session.h"
#include "multiplayer/mp_gen2_serial.h"
#endif
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int max_frames = 0;  /* 0 = unlimited */

/* Deferred multiplayer setup — CLI flags are parsed before
 * gb_platform_init runs (which initialises ENet), so we can't call
 * mp_gen2_host/connect immediately. Stash the requested action and
 * apply it after gb_platform_init. */
static int  g_mp_action = 0;     /* 0=none, 1=host, 2=connect */
static uint16_t g_mp_port = 0;
static char g_mp_host_buf[256] = {0};
static int  g_mp_test = 0;       /* 0=normal, 1=test-master, 2=test-slave */

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--trace") == 0) {
            gbrt_trace_enabled = true;
            printf("Trace enabled\n");
        } else if (strcmp(argv[i], "--trace-entries") == 0 && i + 1 < argc) {
            gbrt_set_trace_file(argv[++i]);
        } else if (strcmp(argv[i], "--limit") == 0 && i + 1 < argc) {
            gbrt_instruction_limit = strtoull(argv[++i], NULL, 10);
            printf("Instruction limit: %llu\n", (unsigned long long)gbrt_instruction_limit);
        } else if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) {
            gb_platform_set_input_script(argv[++i]);
        } else if (strcmp(argv[i], "--dump-frames") == 0 && i + 1 < argc) {
            gb_platform_set_dump_frames(argv[++i]);
        } else if (strcmp(argv[i], "--screenshot-prefix") == 0 && i + 1 < argc) {
            gb_platform_set_screenshot_prefix(argv[++i]);
        } else if (strcmp(argv[i], "--hw-trace") == 0 && i + 1 < argc) {
            hwtrace_init(argv[++i]);
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            max_frames = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--save-dir") == 0 && i + 1 < argc) {
            /* Override the directory where .sav / .rtc / save-states /
             * bindings.cfg live. Useful for running two rom.exe instances
             * side-by-side (link-cable multiplayer testing) without their
             * saves clobbering each other. */
            gb_platform_set_save_dir(argv[++i]);
#ifdef LA_HAS_MULTIPLAYER
        } else if (strcmp(argv[i], "--mp-host") == 0 && i + 1 < argc) {
            /* Open a Cable Club listener on the given port. */
            g_mp_action = 1;
            g_mp_port = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--mp-connect") == 0 && i + 1 < argc) {
            /* Connect to an open Cable Club at host:port. */
            const char* spec = argv[++i];
            const char* colon = strrchr(spec, ':');
            if (colon) {
                size_t hostlen = (size_t)(colon - spec);
                if (hostlen >= sizeof(g_mp_host_buf)) hostlen = sizeof(g_mp_host_buf) - 1;
                memcpy(g_mp_host_buf, spec, hostlen);
                g_mp_host_buf[hostlen] = '\0';
                g_mp_port = (uint16_t)atoi(colon + 1);
            } else {
                strncpy(g_mp_host_buf, spec, sizeof(g_mp_host_buf) - 1);
                g_mp_port = 0;  /* mp_gen2_connect uses default */
            }
            g_mp_action = 2;
        } else if (strcmp(argv[i], "--mp-test-master") == 0) {
            g_mp_test = 1;
        } else if (strcmp(argv[i], "--mp-test-slave") == 0) {
            g_mp_test = 2;
#endif
        }
    }

    GBContext* ctx = gb_context_create(NULL);
    if (!ctx) {
        fprintf(stderr, "Failed to create context\n");
        return 1;
    }
#ifdef GB_HAS_SDL2
    if (!gb_platform_init(3)) {
        fprintf(stderr, "Failed to initialize platform\n");
        gb_context_destroy(ctx);
        return 1;
    }
    gb_platform_register_context(ctx);
#ifdef LA_HAS_MULTIPLAYER
    if (g_mp_action == 1) {
        mp_gen2_host(g_mp_port);
    } else if (g_mp_action == 2) {
        mp_gen2_connect(g_mp_host_buf, g_mp_port);
    }
    /* Self-test mode: skip the entire game and just exercise the
     * transport layer against the partner. Used to validate the
     * Cable Club plumbing without driving two Gen 2 ROMs all the
     * way to the in-game Trade Center. */
    if (g_mp_test != 0) {
        bool ok = mp_gen2_self_test(g_mp_test == 1, /*connect_timeout*/ 10000);
        gb_platform_shutdown();
        gb_context_destroy(ctx);
        return ok ? 0 : 1;
    }
#endif
#endif

    rom_init(ctx);

#ifdef GB_HAS_SDL2
    int frame_count = 0;
    while (1) {
#ifdef LA_HAS_MULTIPLAYER
        if (mp_session_is_client_connected()) {
            if (!gb_platform_poll_events(ctx)) break;
            mp_session_update();
            const uint32_t* fb = mp_session_get_framebuffer();
            if (fb) gb_platform_render_frame(fb);
            gb_platform_vsync();
            frame_count++;
            continue;
        }
#endif
        gb_run_frame(ctx);
        if (!gb_platform_poll_events(ctx)) break;
        if (ctx->frame_done) {
            const uint32_t* fb = gb_get_framebuffer(ctx);
            if (fb) gb_platform_render_frame(fb);
            gb_reset_frame(ctx);
            ctx->stopped = 0;
            gb_platform_vsync();
            frame_count++;
            if (max_frames > 0 && frame_count >= max_frames) break;
        }
    }
    gb_platform_shutdown();
#else
    rom_run(ctx);
    printf("Recompiled code executed successfully!\n");
    printf("Registers: A=%02X B=%02X C=%02X\n", ctx->a, ctx->b, ctx->c);
#endif

    gb_context_destroy(ctx);
    return 0;
}
