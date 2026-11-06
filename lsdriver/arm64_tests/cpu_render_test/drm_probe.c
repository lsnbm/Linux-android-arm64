#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <drm/drm.h>
#include <drm/drm_fourcc.h>

__asm__(
    ".section .tdata,\"awT\",%progbits\n"
    ".balign 64\n"
    ".global drm_probe_tls_alignment_anchor\n"
    ".hidden drm_probe_tls_alignment_anchor\n"
    ".type drm_probe_tls_alignment_anchor,%object\n"
    "drm_probe_tls_alignment_anchor:\n"
    ".zero 1\n"
    ".size drm_probe_tls_alignment_anchor,1\n"
    ".previous\n");

int main(void)
{
    int descriptor = open("/dev/dri/card0", O_RDONLY | O_CLOEXEC);
    if (descriptor < 0)
    {
        perror("open card0");
        return 1;
    }
    struct drm_set_client_cap capability = {.capability = DRM_CLIENT_CAP_ATOMIC, .value = 1};
    if (ioctl(descriptor, DRM_IOCTL_SET_CLIENT_CAP, &capability)) goto fail;
    uint32_t plane_ids[256];
    struct drm_mode_get_plane_res resources = {
        .plane_id_ptr = (uintptr_t)plane_ids,
        .count_planes = sizeof(plane_ids) / sizeof(plane_ids[0]),
    };
    if (ioctl(descriptor, DRM_IOCTL_MODE_GETPLANERESOURCES, &resources)) goto fail;
    if (resources.count_planes > sizeof(plane_ids) / sizeof(plane_ids[0])) goto overflow;
    printf("planes=%u\n", resources.count_planes);
    for (uint32_t index = 0; index < resources.count_planes; index++)
    {
        uint32_t formats[256];
        struct drm_mode_get_plane plane = {
            .plane_id = plane_ids[index],
            .format_type_ptr = (uintptr_t)formats,
            .count_format_types = sizeof(formats) / sizeof(formats[0]),
        };
        if (ioctl(descriptor, DRM_IOCTL_MODE_GETPLANE, &plane)) goto fail;
        if (plane.count_format_types > sizeof(formats) / sizeof(formats[0])) goto overflow;
        int argb = 0;
        for (uint32_t format_index = 0; format_index < plane.count_format_types; format_index++)
            if (formats[format_index] == DRM_FORMAT_ARGB8888) argb = 1;
        printf("plane=%u crtc=%u fb=%u possible_crtcs=0x%x ARGB8888=%d\n",
               plane.plane_id, plane.crtc_id, plane.fb_id, plane.possible_crtcs, argb);
        if (plane.crtc_id)
        {
            struct drm_mode_crtc crtc = {.crtc_id = plane.crtc_id};
            if (ioctl(descriptor, DRM_IOCTL_MODE_GETCRTC, &crtc)) goto fail;
            printf("  mode=%ux%u valid=%u\n", crtc.mode.hdisplay, crtc.mode.vdisplay, crtc.mode_valid);
        }
        uint32_t property_ids[256];
        uint64_t property_values[256];
        struct drm_mode_obj_get_properties properties = {
            .props_ptr = (uintptr_t)property_ids,
            .prop_values_ptr = (uintptr_t)property_values,
            .count_props = sizeof(property_ids) / sizeof(property_ids[0]),
            .obj_id = plane.plane_id,
            .obj_type = DRM_MODE_OBJECT_PLANE,
        };
        if (ioctl(descriptor, DRM_IOCTL_MODE_OBJ_GETPROPERTIES, &properties)) goto fail;
        if (properties.count_props > sizeof(property_ids) / sizeof(property_ids[0])) goto overflow;
        for (uint32_t prop_index = 0; prop_index < properties.count_props; prop_index++)
        {
            uint64_t values[64];
            struct drm_mode_property_enum enums[64];
            struct drm_mode_get_property property = {
                .prop_id = property_ids[prop_index],
                .values_ptr = (uintptr_t)values,
                .enum_blob_ptr = (uintptr_t)enums,
                .count_values = sizeof(values) / sizeof(values[0]),
                .count_enum_blobs = sizeof(enums) / sizeof(enums[0]),
            };
            if (ioctl(descriptor, DRM_IOCTL_MODE_GETPROPERTY, &property)) goto fail;
            if (strcmp(property.name, "type") && strcmp(property.name, "zpos") &&
                strcmp(property.name, "alpha") && strcmp(property.name, "blend_op") &&
                strcmp(property.name, "pixel blend mode") && strcmp(property.name, "rotation") &&
                strcmp(property.name, "src_config") && strcmp(property.name, "multirect_mode")) continue;
            if (property.count_values > sizeof(values) / sizeof(values[0]) ||
                property.count_enum_blobs > sizeof(enums) / sizeof(enums[0])) goto overflow;
            printf("  %s=%" PRIu64 " flags=0x%x", property.name, property_values[prop_index], property.flags);
            if ((property.flags & DRM_MODE_PROP_RANGE) && property.count_values == 2)
                printf(" range=%" PRIu64 "..%" PRIu64, values[0], values[1]);
            if (property.flags & (DRM_MODE_PROP_ENUM | DRM_MODE_PROP_BITMASK))
                for (uint32_t enum_index = 0; enum_index < property.count_enum_blobs; enum_index++)
                    printf(" [%llu=%s]", (unsigned long long)enums[enum_index].value, enums[enum_index].name);
            putchar('\n');
        }
    }
    close(descriptor);
    return 0;
overflow:
    errno = EOVERFLOW;
fail:
    perror("DRM query");
    close(descriptor);
    return 1;
}