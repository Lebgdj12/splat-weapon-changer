# Dockerfile for building the Splat Weapon Changer Aroma plugin
# Multi-stage: starts from devkitPPC base, copies WUPS + libnotifications in.
# Matches the canonical pattern used by wiiu-env plugins (ftpiiu_plugin,
# SwipSwapMe, region_free_plugin, AromaBasePlugin, etc.)

# devkitPPC base image (devkitPro + WUT preinstalled)
FROM ghcr.io/wiiu-env/devkitppc:20260225

# Copy WUPS library + headers into $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/wiiupluginsystem:20260418 /artifacts $DEVKITPRO

# Copy libnotifications (for on-screen debug toasts)
COPY --from=ghcr.io/wiiu-env/libnotifications:20260404 /artifacts $DEVKITPRO

WORKDIR /project
