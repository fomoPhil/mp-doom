/* MPC Sample st7796s SPI panel via Linux DRM UAPI; no libdrm dependency.
 * 320x200 Doom pixels are nearest-neighbor scaled to the 320x240 panel.
 * DIRTYFB performs synchronous SPI I/O; keep the audio thread independent.
 */
#include "mpc_display.h"
#include <stddef.h>
#include <string.h>

static void scale_frame(uint32_t *out, size_t stride, const uint32_t *in)
{
    for (unsigned y = 0; y < 240; ++y)
        memcpy(out + y * stride, in + (y * 200 / 240) * 320, 320 * sizeof *out);
}

static void copy_native_frame(uint32_t *out, size_t stride, const uint32_t *in)
{
    for (unsigned y = 0; y < 240; ++y)
        memcpy(out + y * stride, in + y * 320, 320 * sizeof *out);
}

#ifdef DISPLAY_SELFTEST
#include <assert.h>
#include <stdio.h>

int main(void)
{
    static uint32_t source[320 * 200], output[324 * 240];
    for (unsigned y = 0; y < 200; ++y)
        for (unsigned x = 0; x < 320; ++x)
            source[y * 320 + x] = y * 1000 + x;
    for (unsigned i = 0; i < 324 * 240; ++i) output[i] = 0xdeadbeef;
    scale_frame(output, 324, source);
    for (unsigned y = 0; y < 240; ++y) {
        for (unsigned x = 0; x < 320; ++x)
            assert(output[y * 324 + x] == (y * 200 / 240) * 1000 + x);
        for (unsigned x = 320; x < 324; ++x)
            assert(output[y * 324 + x] == 0xdeadbeef);
    }
    assert(output[0] == 0 && output[239 * 324 + 319] == 199319);
    static uint32_t native[320 * 240];
    for (unsigned i = 0; i < 320 * 240; ++i) native[i] = i;
    copy_native_frame(output, 324, native);
    for (unsigned y = 0; y < 240; ++y) {
        for (unsigned x = 0; x < 320; ++x)
            assert(output[y * 324 + x] == y * 320 + x);
        for (unsigned x = 320; x < 324; ++x)
            assert(output[y * 324 + x] == 0xdeadbeef);
    }
    puts("display scale/native stride selftest ok");
    return 0;
}
#else
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <drm/drm.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_mode.h>

static int display_fd = -1, master, scanout;
static uint32_t framebuffer, dumb_handle, crtc;
static uint32_t *mapped;
static size_t mapped_bytes, stride;

static int drm_request(unsigned long request, void *arg, const char *operation)
{
    int result;
    do result = ioctl(display_fd, request, arg); while (result < 0 && errno == EINTR);
    if (result < 0) fprintf(stderr, "display %s: %s\n", operation, strerror(errno));
    return result;
}

void mpc_display_close(void)
{
    if (display_fd < 0) return;
    /* Stock creates a new framebuffer on restart; its previous FB is already gone. */
    if (scanout) {
        struct drm_mode_crtc off = {.crtc_id = crtc};
        drm_request(DRM_IOCTL_MODE_SETCRTC, &off, "disable scanout");
    }
    if (mapped) munmap(mapped, mapped_bytes);
    if (framebuffer) drm_request(DRM_IOCTL_MODE_RMFB, &framebuffer, "remove framebuffer");
    if (dumb_handle) {
        struct drm_mode_destroy_dumb destroy = {.handle = dumb_handle};
        drm_request(DRM_IOCTL_MODE_DESTROY_DUMB, &destroy, "destroy buffer");
    }
    if (master) drm_request(DRM_IOCTL_DROP_MASTER, NULL, "drop master");
    close(display_fd);
    display_fd = -1;
    master = scanout = 0;
    framebuffer = dumb_handle = crtc = 0;
    mapped = NULL;
    mapped_bytes = stride = 0;
}

int mpc_display_open(void)
{
    if (display_fd >= 0) return mapped ? 0 : -1;
    display_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (display_fd < 0) { perror("open DRM display"); return -1; }
    if (drm_request(DRM_IOCTL_SET_MASTER, NULL, "set master (stop ac50 first)") < 0) goto fail;
    master = 1;

    /* This device has one SPI connector. Bound all kernel-returned counts. */
    uint32_t crtcs[16], connectors[16];
    struct drm_mode_card_res resources = {
        .crtc_id_ptr = (uintptr_t)crtcs, .connector_id_ptr = (uintptr_t)connectors,
        .count_crtcs = 16, .count_connectors = 16
    };
    if (drm_request(DRM_IOCTL_MODE_GETRESOURCES, &resources, "get resources") < 0) goto fail;
    if (!resources.count_crtcs || resources.count_crtcs > 16 ||
        !resources.count_connectors || resources.count_connectors > 16) {
        fputs("display: unexpected resource counts\n", stderr);
        goto fail;
    }
    struct drm_mode_modeinfo selected_mode = {0};
    uint32_t selected_connector = 0;
    for (unsigned n = 0; n < resources.count_connectors && !selected_connector; ++n) {
        struct drm_mode_modeinfo modes[16];
        uint32_t encoders[16];
        struct drm_mode_get_connector connector = {.connector_id = connectors[n]};
        if (drm_request(DRM_IOCTL_MODE_GETCONNECTOR, &connector, "probe connector") < 0) goto fail;
        if (connector.connection != 1 || !connector.count_modes || connector.count_modes > 16 ||
            !connector.count_encoders || connector.count_encoders > 16) continue;
        /* Do not carry the probed property count into a request with no arrays. */
        connector = (struct drm_mode_get_connector){
            .connector_id = connectors[n], .modes_ptr = (uintptr_t)modes,
            .count_modes = connector.count_modes, .encoders_ptr = (uintptr_t)encoders,
            .count_encoders = connector.count_encoders
        };
        if (drm_request(DRM_IOCTL_MODE_GETCONNECTOR, &connector, "get connector") < 0) goto fail;
        if (connector.count_modes > 16 || connector.count_encoders > 16) goto fail;
        for (unsigned m = 0; m < connector.count_modes && !selected_connector; ++m) {
            if (modes[m].hdisplay != 320 || modes[m].vdisplay != 240) continue;
            for (unsigned e = 0; e < connector.count_encoders && !selected_connector; ++e) {
                struct drm_mode_get_encoder encoder = {.encoder_id = encoders[e]};
                if (drm_request(DRM_IOCTL_MODE_GETENCODER, &encoder, "get encoder") < 0) goto fail;
                for (unsigned c = 0; c < resources.count_crtcs; ++c) {
                    if (!(encoder.possible_crtcs & (1u << c))) continue;
                    crtc = crtcs[c];
                    selected_connector = connector.connector_id;
                    selected_mode = modes[m];
                    break;
                }
            }
        }
    }
    if (!selected_connector) { fputs("display: connected 320x240 mode not found\n", stderr); goto fail; }

    struct drm_mode_create_dumb dumb = {.width = 320, .height = 240, .bpp = 32};
    if (drm_request(DRM_IOCTL_MODE_CREATE_DUMB, &dumb, "create buffer") < 0) goto fail;
    dumb_handle = dumb.handle;
    if (dumb.pitch < 320 * 4 || dumb.pitch % 4 || dumb.size < (uint64_t)dumb.pitch * 240 || dumb.size > SIZE_MAX) {
        fputs("display: invalid buffer geometry\n", stderr);
        goto fail;
    }
    struct drm_mode_fb_cmd2 add = {
        .width = 320, .height = 240, .pixel_format = DRM_FORMAT_XRGB8888,
        .handles = {dumb.handle}, .pitches = {dumb.pitch}
    };
    if (drm_request(DRM_IOCTL_MODE_ADDFB2, &add, "add framebuffer") < 0) goto fail;
    framebuffer = add.fb_id;
    struct drm_mode_map_dumb map = {.handle = dumb.handle};
    if (drm_request(DRM_IOCTL_MODE_MAP_DUMB, &map, "map buffer") < 0) goto fail;
    void *address = mmap(NULL, dumb.size, PROT_READ | PROT_WRITE, MAP_SHARED, display_fd, map.offset);
    if (address == MAP_FAILED) { perror("mmap display"); goto fail; }
    mapped = address;
    mapped_bytes = dumb.size;
    stride = dumb.pitch / 4;
    memset(mapped, 0, mapped_bytes);
    struct drm_mode_crtc set = {
        .crtc_id = crtc, .fb_id = framebuffer,
        .set_connectors_ptr = (uintptr_t)&selected_connector, .count_connectors = 1,
        .mode = selected_mode, .mode_valid = 1
    };
    if (drm_request(DRM_IOCTL_MODE_SETCRTC, &set, "set scanout") < 0) goto fail;
    scanout = 1;
    printf("display: DRM 320x240, crtc %u, connector %u, pitch %u\n", crtc, selected_connector, dumb.pitch);
    return 0;
fail:
    mpc_display_close();
    return -1;
}

static int flush_frame(void)
{
    struct drm_clip_rect clip = {0, 0, 320, 240};
    struct drm_mode_fb_dirty_cmd dirty = {
        .fb_id = framebuffer, .num_clips = 1, .clips_ptr = (uintptr_t)&clip
    };
    return drm_request(DRM_IOCTL_MODE_DIRTYFB, &dirty, "flush pixels");
}

int mpc_display_frame(const uint32_t *pixels)
{
    if (!mapped || !pixels) return -1;
    scale_frame(mapped, stride, pixels);
    return flush_frame();
}

int mpc_display_native_frame(const uint32_t *pixels)
{
    if (!mapped || !pixels) return -1;
    copy_native_frame(mapped, stride, pixels);
    return flush_frame();
}
#endif
