# Third-Party Libraries

This project relies on several external libraries. The table below lists the
main libraries used during build and runtime together with their license and
source location.

| Library | License | Source |
| ------- | ------- | ------ |
| **libcbv2g** | Apache-2.0 | <https://github.com/EVerest/libcbv2g> |
| **libslac** (optional) | Apache-2.0 | <https://github.com/EVerest/libslac> |
| **OpenSSL** | Apache-2.0 | <https://www.openssl.org> |
| **mbedTLS** | Apache-2.0 | <https://github.com/Mbed-TLS/mbedtls> |
| **Catch2** | BSL-1.0 | <https://github.com/catchorg/Catch2> |

`libcbv2g` provides the EXI message handling used by libiso15118.  `libslac`
may be used for SLAC message definitions.  TLS support can be implemented using
either OpenSSL (typical on desktop systems) or mbedTLS (used on embedded
platforms).  Unit tests are implemented with the Catch2 framework.
