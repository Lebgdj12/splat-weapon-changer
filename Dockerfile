# Dockerfile for building the Splat Weapon Changer Aroma plugin
# Based on CodePatchPlugin (theawesomecoder61) + Inkay (PretendoNetwork) patterns.

# devkitPPC base image (devkitPro + WUT preinstalled)
FROM ghcr.io/wiiu-env/devkitppc:20260225

# Copy WUPS library + headers into $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/wiiupluginsystem:20260418 /artifacts $DEVKITPRO

# Copy WUMS (Wii U Module System) - provides libwums, used by kernel.hpp
COPY --from=ghcr.io/wiiu-env/wiiumodulesystem:20260418 /artifacts $DEVKITPRO

# Copy libkernel - provides KernelCopyData, used by kernel.hpp
COPY --from=ghcr.io/wiiu-env/libkernel:20260331 /artifacts $DEVKITPRO

WORKDIR /project

