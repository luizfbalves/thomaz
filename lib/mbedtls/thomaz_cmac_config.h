/**
 * thomaz_cmac_config.h
 *
 * CMAC-enabled Mbed TLS 2.28.10 configuration for the thomaz hactool port.
 *
 * This config is used ONLY by the `thomaz_mbedtls_cmac` static lib (lib/mbedtls/).
 * It does NOT affect the devkitPro portlib mbedtls that curl links against.
 *
 * Delta vs devkitPro Switch PKGBUILD portlib config:
 *   +MBEDTLS_CMAC_C           — required by hactool NCA header verification
 *   +MBEDTLS_ENTROPY_HARDWARE_ALT  — matches PKGBUILD (hardware RNG, no /dev/random)
 *   +MBEDTLS_NO_PLATFORM_ENTROPY   — matches PKGBUILD (no OS entropy source on Switch)
 *   -MBEDTLS_SELF_TEST         — disabled, matching PKGBUILD (no test binaries on Switch)
 *
 * All other settings follow the mbedtls 2.28.10 default config.h. This file is
 * passed via MBEDTLS_USER_CONFIG_FILE so it overrides only the listed symbols.
 */

#ifndef THOMAZ_CMAC_CONFIG_H
#define THOMAZ_CMAC_CONFIG_H

/* Enable AES-CMAC — the primary reason this second mbedtls build exists.
 * The devkitPro portlib for Switch is built without this (the 3DS PKGBUILD
 * enables it, but not the Switch one). hactool needs mbedtls_cipher_cmac_*. */
#define MBEDTLS_CMAC_C

/* Match the devkitPro Switch PKGBUILD entropy config:
 * Hardware RNG alt (csrng on Switch) replaces the software entropy collector,
 * and platform entropy sources (/dev/random etc.) are not available on Switch. */
#define MBEDTLS_ENTROPY_HARDWARE_ALT
#define MBEDTLS_NO_PLATFORM_ENTROPY

/* Disable self-test code — matches PKGBUILD; keeps binary size down and avoids
 * platform I/O in self-test paths on Switch. */
#undef MBEDTLS_SELF_TEST

/* Drop the TCP/IP sockets module: net_sockets.c has a hard `#error` on any
 * platform that is not Unix or Windows (Switch/devkitA64), and hactool needs no
 * networking. Its source is also excluded from the build (see CMakeLists.txt);
 * undefining the symbol keeps the config consistent with the trimmed sources. */
#undef MBEDTLS_NET_C

#endif /* THOMAZ_CMAC_CONFIG_H */
