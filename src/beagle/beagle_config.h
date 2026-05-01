#pragma once

#include <string>

namespace beagle {

struct BeagleConfig {
  std::string control_plane_url;
  std::string api_token;
  std::string vm_id;
  std::string stream_server_id;
  bool wireguard_active = false;
  bool tls_insecure = false;
};

BeagleConfig load_config();
bool detect_wireguard_active();

}  // namespace beagle
