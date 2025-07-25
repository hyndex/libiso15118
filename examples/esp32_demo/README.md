ESP32 Demo
==========

This example shows how to bring up libiso15118 on an ESP32.  It opens a TLS
listener, performs the ISO‑15118 handshake and replies to a
`SupportedAppProtocolReq` message.

Wi‑Fi/Ethernet Setup
---------------------

- Configure the station SSID and password via `CONFIG_WIFI_SSID` and
  `CONFIG_WIFI_PASSWORD` in your ESP‑IDF `sdkconfig`.
- Ensure IPv6 is enabled for the network interface.
- The demo uses interface name `STA` for Wi‑Fi.  Change to `ETH` if using Ethernet.
- Generate the test certificates by running `test/iso15118/io/pki/pki.sh` and keep
  the resulting `certs` directory in the project.

Build the example with PlatformIO:

```bash
pio run -e esp32s3
```
