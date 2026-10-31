# Dockerfile for building the Splat Weapon Changer Aroma plugin
# Multi-stage: starts from devkitPPC base, copies WUPS in.
# Matches the canonical pattern used by wiiu-env plugins (ftpiiu_plugin,
# SwipSwapMe, region_free_plugin, etc.)

# devkitPPC base image (devkitPro + WUT preinstalled)
FROM ghcr.io/wiiu-env/devkitppc:20260225

# Copy WUPS library + headers into $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/wiiupluginsystem:20260418 /artifacts $DEVKITPRO

WORKDIR /project
