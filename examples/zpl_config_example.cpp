#include <cppzmqzoltanext/zpl_config.h>

#include <cassert>
#include <iostream>
#include <sstream>

using namespace zmqzext;

int main() {
    const char* zpl_text = R"ZPL(
# This is a sample ZPL configuration for a telemetry gateway
app
    name = "ZPL's Example Telemetry Gateway"
    environment = production # values can be 'production', 'staging', 'development'
    logging
        # Indented comment
        level = info
        outputs/0 = stdout
        outputs/1 = file
        file
            path = /var/log/telemetry-gw.log
            rotate
                size_mb = 50
                keep = 5
network
    http
        host = 0.0.0.0
        port = 8080
    grpc
        host = 127.0.0.1
        port = 50051
rules
    sampling
        default = 0.1
        per_device/device-123 = 0.5
)ZPL";

    std::istringstream input(zpl_text);
    zpl_config_t root = zpl_config_t::from_stream(input);

    // Basic access (relative paths from root)
    std::cout << "app.name: " << root.get("app/name") << "\n";
    std::cout << "http.port: " << root.get("network/http/port") << "\n";

    // Non-throwing access
    auto maybe_rotations = root.try_get("app/logging/file/rotate/keep");
    if (maybe_rotations) {
        std::cout << "log.rotate.keep: " << *maybe_rotations << "\n";
    }

    // Default value when missing
    std::string region = root.get_or("app/region", "us-east-1");
    std::cout << "region: " << region << "\n";

    // Navigate to a child node and enumerate its children
    zpl_config_t logging = root.child("app/logging");
    for (const auto& child : logging.children()) {
        std::cout << "logging child: " << child.name() << " = " << child.value() << "\n";
    }

    // Check existence
    assert(root.contains("rules/sampling/per_device/device-123"));
    assert(!root.contains("rules/sampling/per_device/device-999"));

    return 0;
}
