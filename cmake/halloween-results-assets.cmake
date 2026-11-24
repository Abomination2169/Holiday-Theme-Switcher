# Use the approved v1.0.1 illustrations and release audio.
set(_christmas_eve_source "https://raw.githubusercontent.com/Abomination2169/Halloween-Theme/11e648824194ca0bc83d0f9e0679764595c132a4")
foreach(index RANGE 0 7)
    fetch_christmas_eve_asset("assets/ghost_new_${index}.png"
                              "${CMAKE_CURRENT_SOURCE_DIR}/assets/ghost_new_${index}.png")
endforeach()
set(halloween_package "${CMAKE_BINARY_DIR}/halloween-v1.0.1-assets.qmod")
file(DOWNLOAD
     "https://github.com/Abomination2169/Halloween-Theme/releases/download/v1.0.1/Halloween-Theme.qmod"
     "${halloween_package}" TLS_VERIFY ON TIMEOUT 60
     EXPECTED_HASH SHA256=065220dd3a6110015b88049373ee4cadce6f807b01568d1f7d68cde832cfc09b)
file(ARCHIVE_EXTRACT INPUT "${halloween_package}" DESTINATION "${CMAKE_BINARY_DIR}"
     PATTERNS "ghost_ogrebane.ogg" "ghost_scary_1.ogg" "ghost_breath.ogg" "ghost_pixabay_creepy.ogg" "ghost_pixabay_wind.ogg" "ghost_extra_0.png" "ghost_extra_1.png" "ghost_extra_2.png" "ghost_extra_3.png" "GHOST_AUDIO_CREDITS.txt" "PIXABAY_AUDIO_CREDITS.txt")
foreach(asset ghost_ogrebane.ogg ghost_scary_1.ogg ghost_breath.ogg ghost_pixabay_creepy.ogg ghost_pixabay_wind.ogg ghost_extra_0.png ghost_extra_1.png ghost_extra_2.png ghost_extra_3.png GHOST_AUDIO_CREDITS.txt PIXABAY_AUDIO_CREDITS.txt)
    if(NOT EXISTS "${CMAKE_BINARY_DIR}/${asset}")
        message(FATAL_ERROR "Missing Halloween release asset: ${asset}")
    endif()
endforeach()
