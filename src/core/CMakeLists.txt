# Platform-independent logic: no Widgets and no direct platform API calls.
add_library(legilo_core STATIC
    Logging.cpp
    Logging.h
    audio/AudioQueue.cpp
    audio/AudioQueue.h
    models/ModelInstaller.cpp
    models/ModelInstaller.h
    tts/Phonemiser.cpp
    tts/Phonemiser.h
    tts/SherpaTtsEngine.cpp
    tts/SherpaTtsEngine.h
    tts/TtsEngine.h
    tts/Voice.h
    tts/VoiceRegistry.cpp
    tts/VoiceRegistry.h
)
target_include_directories(legilo_core PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(legilo_core PUBLIC Qt6::Core)

# espeak-ng turns text into phonemes for the voices. Linux uses the
# distribution's library; the Windows build is set up in stage 5.
find_package(PkgConfig REQUIRED)
pkg_check_modules(ESPEAK_NG REQUIRED IMPORTED_TARGET espeak-ng>=1.52)
target_link_libraries(legilo_core PRIVATE PkgConfig::ESPEAK_NG Legilo::SherpaOnnx)

# libarchive unpacks voice model archives (.tar.bz2).
find_package(LibArchive 3.6 REQUIRED)
target_link_libraries(legilo_core PRIVATE LibArchive::LibArchive)
legilo_set_warnings(legilo_core)
