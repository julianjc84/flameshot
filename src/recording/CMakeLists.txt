# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

target_sources(
    flameshot
    PRIVATE
        recordingcontext.h
        recordingcontext.cpp
        recordingrequest.h
        recordingrequest.cpp
        recordingcontroller.h
        recordingcontroller.cpp
        framegrabber.h
        framegrabber.cpp
        videoencoder.h
        videoencoder.cpp
        ffmpegutils.h
        ffmpegutils.cpp
)

# Platform-specific frame grabbers
if (WIN32)
    target_sources(flameshot PRIVATE windowsframegrabber.h windowsframegrabber.cpp)
elseif (APPLE)
    target_sources(flameshot PRIVATE macframegrabber.h macframegrabber.mm)
    target_link_libraries(flameshot "-framework CoreGraphics")
else()
    target_sources(flameshot PRIVATE x11framegrabber.h x11framegrabber.cpp)
    # Wayland frame grabber (requires PipeWire)
    if (PIPEWIRE_FOUND)
        target_sources(flameshot PRIVATE waylandframegrabber.h waylandframegrabber.cpp)
    endif()
endif()
