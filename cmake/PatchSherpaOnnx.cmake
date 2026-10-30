# Legilo's changes to sherpa-onnx, applied to its unpacked source before it
# is configured. Run as: cmake -DSOURCE_DIR=<sherpa-onnx source> -P PatchSherpaOnnx.cmake
#
# Each change replaces exact text and fails the build if that text is not
# found, so an upgrade that moves the code cannot silently drop a patch.
# Applying twice is harmless.

cmake_minimum_required(VERSION 3.25)

# legilo_patch(<file> <expected text> <replacement text>)
function(legilo_patch file expected replacement)
    set(path "${SOURCE_DIR}/${file}")
    file(READ "${path}" content)
    string(FIND "${content}" "${replacement}" applied)
    if(NOT applied EQUAL -1)
        return()
    endif()
    string(FIND "${content}" "${expected}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Cannot patch ${file}: the expected code has changed. "
                            "Check the patch in cmake/PatchSherpaOnnx.cmake against this release.")
    endif()
    string(REPLACE "${expected}" "${replacement}" content "${content}")
    file(WRITE "${path}" "${content}")
    message(STATUS "Patched ${file}")
endfunction()

# Piper voices: no pad token after the start marker. sherpa-onnx 2.0.1
# builds [^, _, p1, _, p2, _, ..., $] from phonemes; Piper's own runtime
# (piper-tts) builds [^, p1, _, p2, _, ..., $]. With the extra pad, cori
# babbles for seconds on short words such as "Eight." and "Open." in nearly
# every call, and squashes the start of sentences; without it, it does
# neither. Measured on 30 October 2026 with cori-medium run directly through
# ONNX Runtime, 20 calls each. Report upstream so this can go. The function
# also serves icefall VITS models, which Legilo does not use.
legilo_patch(sherpa-onnx/csrc/offline-tts-frontend.cc
"  if (is_inflect) {
    ans.push_back(pad);
  } else {
    ans.push_back(bos);
    ans.push_back(pad);
  }"
"  if (is_inflect) {
    ans.push_back(pad);
  } else {
    // Patched by Legilo: no pad after bos, as in Piper's own runtime.
    ans.push_back(bos);
  }")
