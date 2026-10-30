# Platform-independent logic: no Widgets and no direct platform API calls.
add_library(legilo_core STATIC
    Logging.cpp
    Logging.h
    audio/AudioOutput.h
    audio/AudioQueue.cpp
    audio/AudioQueue.h
    audio/MiniaudioOutput.cpp
    audio/MiniaudioOutput.h
    models/ModelInstaller.cpp
    models/ModelInstaller.h
    playback/FirstChunk.cpp
    playback/FirstChunk.h
    playback/PlaybackController.cpp
    playback/PlaybackController.h
    playback/SynthesisWorker.cpp
    playback/SynthesisWorker.h
    text/Normaliser.cpp
    text/Normaliser.h
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

# miniaudio plays the audio. It is a single header, from Debian's
# libminiaudio-dev on Linux (the Windows build is set up in stage 5). Its
# implementation is compiled as C in audio/miniaudio.c, apart from Legilo's
# code and warnings, without the decoding, encoding and engine parts Legilo
# does not use.
find_path(LEGILO_MINIAUDIO_INCLUDE_DIR miniaudio.h REQUIRED)
add_library(legilo_miniaudio STATIC audio/miniaudio.c)
target_include_directories(legilo_miniaudio SYSTEM PUBLIC ${LEGILO_MINIAUDIO_INCLUDE_DIR})
target_compile_definitions(legilo_miniaudio PUBLIC
    MA_NO_DECODING MA_NO_ENCODING MA_NO_GENERATION
    MA_NO_RESOURCE_MANAGER MA_NO_NODE_GRAPH MA_NO_ENGINE)
find_package(Threads REQUIRED)
target_link_libraries(legilo_miniaudio PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
if(UNIX)
    target_link_libraries(legilo_miniaudio PUBLIC m)
endif()
target_link_libraries(legilo_core PRIVATE legilo_miniaudio)

# libarchive unpacks voice model archives (.tar.bz2).
find_package(LibArchive 3.6 REQUIRED)
target_link_libraries(legilo_core PRIVATE LibArchive::LibArchive)
legilo_set_warnings(legilo_core)
