#include <stdio.h>
#include <string.h>

#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include <iso15118/config.hpp>
#include <iso15118/io/connection_ssl.hpp>
#include <iso15118/io/logging.hpp>
#include <iso15118/io/poll_manager.hpp>
#include <iso15118/io/socket_helper.hpp>
#include <iso15118/io/time.hpp>
#include <iso15118/message/supported_app_protocol.hpp>
#include <iso15118/session/iso.hpp>

using namespace iso15118;

static void init_network() {
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_netif_create_default_wifi_sta();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config{};
    strcpy(reinterpret_cast<char*>(wifi_config.sta.ssid), CONFIG_WIFI_SSID);
    strcpy(reinterpret_cast<char*>(wifi_config.sta.password), CONFIG_WIFI_PASSWORD);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();
}

static std::vector<uint8_t> build_sap_request() {
    message_20::SupportedAppProtocolRequest req;
    auto& ap = req.app_protocol.emplace_back();
    ap.priority = 1;
    ap.protocol_namespace = "urn:iso:std:iso:15118:-20:DC";
    ap.schema_id = 1;
    ap.version_number_major = 1;
    ap.version_number_minor = 0;

    uint8_t payload[128];
    io::StreamOutputView out{payload, sizeof(payload)};
    const auto len = message_20::serialize(req, out);

    std::vector<uint8_t> packet(io::SdpPacket::V2GTP_HEADER_SIZE + len);
    packet[0] = io::SDP_PROTOCOL_VERSION;
    packet[1] = io::SDP_INVERSE_PROTOCOL_VERSION;
    const uint16_t type = htobe16(static_cast<uint16_t>(io::v2gtp::PayloadType::SAP));
    memcpy(packet.data() + 2, &type, sizeof(type));
    const uint32_t plen = htobe32(len);
    memcpy(packet.data() + 4, &plen, sizeof(plen));
    memcpy(packet.data() + 8, payload, len);
    return packet;
}

static void client_task(void*) {
    vTaskDelay(pdMS_TO_TICKS(1000));

    sockaddr_in6 dest{};
    io::get_first_sockaddr_in6_for_interface("STA", dest);
    dest.sin6_port = htons(50000);
    int sock = socket(AF_INET6, SOCK_STREAM, 0);
    connect(sock, reinterpret_cast<sockaddr*>(&dest), sizeof(dest));

    auto packet = build_sap_request();
    send(sock, packet.data(), packet.size(), 0);

    uint8_t buf[256];
    int r = recv(sock, buf, sizeof(buf), 0);
    if (r > 0) {
        io::SdpPacket sdp;
        memcpy(sdp.get_current_buffer_pos(), buf, r);
        sdp.update_read_bytes(r);
        if (sdp.is_complete()) {
            message_20::Variant var(sdp.get_payload_type(),
                                    io::StreamInputView{sdp.get_payload_buffer(), sdp.get_payload_length()});
            if (var.get_type() == message_20::Type::SupportedAppProtocolRes) {
                auto res = var.get<message_20::SupportedAppProtocolResponse>();
                printf("SAP response code: %d\n", static_cast<int>(res.response_code));
            }
        }
    }

    close(sock);
    vTaskDelete(nullptr);
}

extern "C" void app_main() {
    printf("ISO15118 ESP32 demo\n");

    init_network();

    io::set_logging_callback([](LogLevel, const std::string& msg) {
        printf("%s\n", msg.c_str());
    });

    io::PollManager poll_manager;

    config::SSLConfig ssl{config::CertificateBackend::EVEREST_LAYOUT,
                          {},
                          "certs/client/cso/CPO_CERT_CHAIN.pem",
                          "certs/client/cso/SECC_LEAF.key",
                          "123456",
                          "certs/ca/v2g/V2G_ROOT_CA.pem",
                          "certs/ca/oem/OEM_ROOT_CA.pem",
                          false,
                          false,
                          false,
                          "/spiffs"};

    auto connection = std::make_unique<io::ConnectionSSL>(poll_manager, "STA", ssl);
    session::feedback::Callbacks cb{};
    cb.v2g_message = [](message_20::Type t) { printf("V2G message %d\n", static_cast<int>(t)); };

    d20::EvseSetupConfig setup{};
    setup.evse_id = "ESP32";
    setup.supported_energy_services.push_back(message_20::datatypes::ServiceCategory::DC);

    std::optional<d20::PauseContext> pause;
    Session session(std::move(connection), d20::SessionConfig(setup), cb, pause);

    xTaskCreate(client_task, "client", 4096, nullptr, 5, nullptr);

    auto next = get_current_time_point();
    while (!session.is_finished()) {
        poll_manager.poll(50);
        next = offset_time_point_by_ms(get_current_time_point(), 50);
        auto n = session.poll();
        if (n < next)
            next = n;
    }

    printf("Session finished\n");
}
