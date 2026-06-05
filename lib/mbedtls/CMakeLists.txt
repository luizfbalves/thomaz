# lib/mbedtls/CMakeLists.txt
#
# Builds a CMAC-enabled Mbed TLS 2.28.10 static library as the distinct
# target `thomaz_mbedtls_cmac`. This target is ONLY added when PLATFORM_SWITCH
# (see root CMakeLists.txt if-guard). It does NOT replace the devkitPro portlib
# mbedtls that curl links against — both coexist with thomaz_mbedtls_cmac
# preceding the portlib on APP_PLATFORM_LIB (link order; Pitfall 1).
#
# The CMAC-enabled user config is applied via MBEDTLS_USER_CONFIG_FILE pointing
# to thomaz_cmac_config.h, which overlays the default config.h to enable:
#   MBEDTLS_CMAC_C, MBEDTLS_ENTROPY_HARDWARE_ALT, MBEDTLS_NO_PLATFORM_ENTROPY
# and disables MBEDTLS_SELF_TEST — matching the devkitPro PKGBUILD minus CMAC.

cmake_minimum_required(VERSION 3.10)

set(THOMAZ_MBEDTLS_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# hactool only uses the mbedtls cipher (AES), CMAC, MD/HMAC and bignum (MPI/RSA)
# layers — see `grep mbedtls_ lib/hactool`. It needs NO TLS, x509, PK-parse, or
# networking. We therefore compile ONLY the crypto translation units (plus their
# direct deps). This deliberately excludes:
#   - net_sockets.c — has a hard `#error` on non-Unix/Windows platforms (Switch),
#   - ssl_*.c / x509*.c / certs.c / pkcs11.c / debug.c — TLS+PKI, unused by hactool.
# The stock config.h still *declares* those modules, but with their sources absent
# and nothing referencing their symbols the static lib links clean.
file(GLOB THOMAZ_MBEDTLS_SRC
    ${THOMAZ_MBEDTLS_DIR}/library/aes.c
    ${THOMAZ_MBEDTLS_DIR}/library/aesni.c
    ${THOMAZ_MBEDTLS_DIR}/library/arc4.c
    ${THOMAZ_MBEDTLS_DIR}/library/aria.c
    ${THOMAZ_MBEDTLS_DIR}/library/asn1parse.c
    ${THOMAZ_MBEDTLS_DIR}/library/asn1write.c
    ${THOMAZ_MBEDTLS_DIR}/library/base64.c
    ${THOMAZ_MBEDTLS_DIR}/library/bignum.c
    ${THOMAZ_MBEDTLS_DIR}/library/blowfish.c
    ${THOMAZ_MBEDTLS_DIR}/library/camellia.c
    ${THOMAZ_MBEDTLS_DIR}/library/ccm.c
    ${THOMAZ_MBEDTLS_DIR}/library/chacha20.c
    ${THOMAZ_MBEDTLS_DIR}/library/chachapoly.c
    ${THOMAZ_MBEDTLS_DIR}/library/cipher.c
    ${THOMAZ_MBEDTLS_DIR}/library/cipher_wrap.c
    ${THOMAZ_MBEDTLS_DIR}/library/cmac.c
    ${THOMAZ_MBEDTLS_DIR}/library/constant_time.c
    ${THOMAZ_MBEDTLS_DIR}/library/ctr_drbg.c
    ${THOMAZ_MBEDTLS_DIR}/library/des.c
    ${THOMAZ_MBEDTLS_DIR}/library/dhm.c
    ${THOMAZ_MBEDTLS_DIR}/library/ecdh.c
    ${THOMAZ_MBEDTLS_DIR}/library/ecdsa.c
    ${THOMAZ_MBEDTLS_DIR}/library/ecjpake.c
    ${THOMAZ_MBEDTLS_DIR}/library/ecp.c
    ${THOMAZ_MBEDTLS_DIR}/library/ecp_curves.c
    ${THOMAZ_MBEDTLS_DIR}/library/entropy.c
    ${THOMAZ_MBEDTLS_DIR}/library/entropy_poll.c
    ${THOMAZ_MBEDTLS_DIR}/library/error.c
    ${THOMAZ_MBEDTLS_DIR}/library/gcm.c
    ${THOMAZ_MBEDTLS_DIR}/library/havege.c
    ${THOMAZ_MBEDTLS_DIR}/library/hkdf.c
    ${THOMAZ_MBEDTLS_DIR}/library/hmac_drbg.c
    ${THOMAZ_MBEDTLS_DIR}/library/md.c
    ${THOMAZ_MBEDTLS_DIR}/library/md2.c
    ${THOMAZ_MBEDTLS_DIR}/library/md4.c
    ${THOMAZ_MBEDTLS_DIR}/library/md5.c
    ${THOMAZ_MBEDTLS_DIR}/library/memory_buffer_alloc.c
    ${THOMAZ_MBEDTLS_DIR}/library/nist_kw.c
    ${THOMAZ_MBEDTLS_DIR}/library/oid.c
    ${THOMAZ_MBEDTLS_DIR}/library/padlock.c
    ${THOMAZ_MBEDTLS_DIR}/library/pem.c
    ${THOMAZ_MBEDTLS_DIR}/library/pk.c
    ${THOMAZ_MBEDTLS_DIR}/library/pk_wrap.c
    ${THOMAZ_MBEDTLS_DIR}/library/pkcs12.c
    ${THOMAZ_MBEDTLS_DIR}/library/pkcs5.c
    ${THOMAZ_MBEDTLS_DIR}/library/pkparse.c
    ${THOMAZ_MBEDTLS_DIR}/library/pkwrite.c
    ${THOMAZ_MBEDTLS_DIR}/library/platform.c
    ${THOMAZ_MBEDTLS_DIR}/library/platform_util.c
    ${THOMAZ_MBEDTLS_DIR}/library/poly1305.c
    ${THOMAZ_MBEDTLS_DIR}/library/ripemd160.c
    ${THOMAZ_MBEDTLS_DIR}/library/rsa.c
    ${THOMAZ_MBEDTLS_DIR}/library/rsa_internal.c
    ${THOMAZ_MBEDTLS_DIR}/library/sha1.c
    ${THOMAZ_MBEDTLS_DIR}/library/sha256.c
    ${THOMAZ_MBEDTLS_DIR}/library/sha512.c
    ${THOMAZ_MBEDTLS_DIR}/library/threading.c
    ${THOMAZ_MBEDTLS_DIR}/library/version.c
    ${THOMAZ_MBEDTLS_DIR}/library/version_features.c
    ${THOMAZ_MBEDTLS_DIR}/library/xtea.c
)

# Remove duplicates (net_sockets.c was listed twice above)
list(REMOVE_DUPLICATES THOMAZ_MBEDTLS_SRC)

# Filter out any files that don't exist on disk (future-proofing for partial checkouts)
set(THOMAZ_MBEDTLS_SRC_FOUND)
foreach(f ${THOMAZ_MBEDTLS_SRC})
    if(EXISTS ${f})
        list(APPEND THOMAZ_MBEDTLS_SRC_FOUND ${f})
    endif()
endforeach()

add_library(thomaz_mbedtls_cmac STATIC ${THOMAZ_MBEDTLS_SRC_FOUND})

target_include_directories(thomaz_mbedtls_cmac
    PUBLIC
        ${THOMAZ_MBEDTLS_DIR}/include
    PRIVATE
        ${THOMAZ_MBEDTLS_DIR}/library
)

# Apply the CMAC-enabled user config header. This sets MBEDTLS_CMAC_C,
# MBEDTLS_ENTROPY_HARDWARE_ALT, MBEDTLS_NO_PLATFORM_ENTROPY and unsets
# MBEDTLS_SELF_TEST on top of the default config.h.
target_compile_definitions(thomaz_mbedtls_cmac
    PRIVATE
        MBEDTLS_USER_CONFIG_FILE="${THOMAZ_MBEDTLS_DIR}/thomaz_cmac_config.h"
)

# Relax -Werror for this vendored target (Assumption A2 from 01-RESEARCH.md):
# toolchain drift under the current devkitA64 may surface new warnings in the
# upstream mbedtls source. The app's own warning flags are not affected.
target_compile_options(thomaz_mbedtls_cmac PRIVATE -Wno-error)

# No programs, no tests — matches devkitPro PKGBUILD flags
# (-DENABLE_PROGRAMS=OFF -DENABLE_TESTING=OFF) enforced by the fact that
# only the library target is defined here.
