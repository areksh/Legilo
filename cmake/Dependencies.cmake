# Source archives the build needs, each pinned by SHA-256. scripts/fetch-deps
# downloads them into third_party/downloads/ beforehand; the build itself
# never touches the network, because Debian builds run without it.
#
# Each entry is "<file name>|<URL>|<SHA-256>".
#
# Only sherpa-onnx is unpacked by Legilo. The rest are fetched during
# sherpa-onnx's configure step, by sherpa-onnx itself and by the projects it
# pulls in (kaldi-decoder, kaldifst, kaldi-native-fbank). Each looks first
# for an archive of exactly this name in sherpa-onnx's build directory.
#
# The names, URLs and hashes are copied from the cmake/*.cmake files that
# win: sherpa-onnx's cmake/ directory comes first on the module path, so
# where it has a file (eigen, openfst) that overrides the nested project's
# own. Re-derive them whenever sherpa-onnx is upgraded, from the URLs in a
# networked build's _deps/*-subbuild/CMakeLists.txt; an offline build fails
# if one is missing.

set(LEGILO_SHERPA_ONNX_ARCHIVE sherpa-onnx-2.0.1.tar.gz)

set(LEGILO_DEPENDENCIES
    "sherpa-onnx-2.0.1.tar.gz|https://github.com/k2-fsa/sherpa-onnx/archive/refs/tags/v2.0.1.tar.gz|24ecc0fccf6344c2556e7c229c849e9f61bf9456b915615197d85f35e3f177e7"
    "kaldi-native-fbank-1.22.4.tar.gz|https://github.com/csukuangfj/kaldi-native-fbank/archive/refs/tags/v1.22.4.tar.gz|257fea639585ca6ee00860493bb9d7ab32488b8a5407ffe434c1f361ea545fae"
    "kissfft-febd4caeed32e33ad8b2e0bb5ea77542c40f18ec.zip|https://github.com/mborgerding/kissfft/archive/febd4caeed32e33ad8b2e0bb5ea77542c40f18ec.zip|497103e664168ebe39580b757adbe616f6cf85a16572af581ca7bc42d0ab13fd"
    "utf8proc-2.12.0.tar.gz|https://github.com/JuliaStrings/utf8proc/archive/refs/tags/v2.12.0.tar.gz|f564011d38b2888d583d510b08e69ffa15aa117155db1b9b49ef1dfe1fa25111"
    "kaldi-decoder-0.3.0.tar.gz|https://github.com/k2-fsa/kaldi-decoder/archive/refs/tags/v0.3.0.tar.gz|b9f34cfb4fd3b1344100eead79ef4d37aa15962274b9e3056de345021f76a1b0"
    "eigen-5.0.1.tar.gz|https://gitlab.com/libeigen/eigen/-/archive/5.0.1/eigen-5.0.1.tar.gz|e9c326dc8c05cd1e044c71f30f1b2e34a6161a3b6ecf445d56b53ff1669e3dec"
    "kaldifst-1.8.0.tar.gz|https://github.com/k2-fsa/kaldifst/archive/refs/tags/v1.8.0.tar.gz|3f247b7e5a2409071202f5e2bc6200060f66728c0a3443c03923ad2723e040b3"
    "openfst-1.8.5-2026-07-09.tar.gz|https://github.com/csukuangfj/openfst/archive/refs/tags/v1.8.5-2026-07-09.tar.gz|2ff712a32952fcb01d351121a6bc8ccf4fdc6b2aa06ce8df2b3095dedd518c0e"
    "simple-sentencepiece-0.7.tar.gz|https://github.com/pkufool/simple-sentencepiece/archive/refs/tags/v0.7.tar.gz|1748a822060a35baa9f6609f84efc8eb54dc0e74b9ece3d82367b7119fdc75af"
    "json-3.12.0.tar.gz|https://github.com/nlohmann/json/archive/refs/tags/v3.12.0.tar.gz|4b92eb0c06d10683f7447ce9406cb97cd4b453be18d7279320f7b2f025c10187"
)

# Voices for the integration tests (tests/integration), in the same format.
# scripts/fetch-deps --test-voices unpacks them into third_party/test-voices/,
# one directory per archive, named after it. The tests skip a voice that is
# absent. cori-medium is the default voice; Kokoro is checked to accept the
# Piper phoneme stream.
set(LEGILO_TEST_VOICES
    "vits-piper-en_GB-cori-medium.tar.bz2|https://github.com/k2-fsa/sherpa-onnx/releases/download/tts-models/vits-piper-en_GB-cori-medium.tar.bz2|49c9a5361bbdd95d7ca9687c4de11e5908481f65e7c7c368960df79949fdac2b"
    "kokoro-en-v0_19.tar.bz2|https://github.com/k2-fsa/sherpa-onnx/releases/download/tts-models/kokoro-en-v0_19.tar.bz2|912804855a04745fa77a30be545b3f9a5d15c4d66db00b88cbcd4921df605ac7"
)

# legilo_dependency_fields(<entry> <name-var> <url-var> <hash-var>)
function(legilo_dependency_fields entry name_var url_var hash_var)
    string(REPLACE "|" ";" fields "${entry}")
    list(GET fields 0 name)
    list(GET fields 1 url)
    list(GET fields 2 hash)
    set(${name_var} "${name}" PARENT_SCOPE)
    set(${url_var} "${url}" PARENT_SCOPE)
    set(${hash_var} "${hash}" PARENT_SCOPE)
endfunction()
