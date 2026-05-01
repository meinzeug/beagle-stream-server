#include "beagle_config.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

namespace beagle {
namespace {

constexpr auto kConfigPath = "/etc/beagle/stream-server.env";

std::string trim(std::string value) {
  auto is_space = [](unsigned char ch) {
    return std::isspace(ch) != 0;
  };

  value.erase(value.begin(), std::find_if_not(value.begin(), value.end(), is_space));
  value.erase(std::find_if_not(value.rbegin(), value.rend(), is_space).base(), value.end());
  return value;
}

bool truthy(std::string value) {
  value = trim(std::move(value));
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value == "1" || value == "true" || value == "yes" || value == "on";
}

std::unordered_map<std::string, std::string> read_env_file() {
  std::unordered_map<std::string, std::string> values;
  std::ifstream file(kConfigPath);
  if (!file.is_open()) {
    return values;
  }

  std::string line;
  while (std::getline(file, line)) {
    line = trim(std::move(line));
    if (line.empty() || line[0] == '#') {
      continue;
    }

    const auto pos = line.find('=');
    if (pos == std::string::npos) {
      continue;
    }

    auto key = trim(line.substr(0, pos));
    auto value = trim(line.substr(pos + 1));
    if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\''))) {
      value = value.substr(1, value.size() - 2);
    }
    if (!key.empty()) {
      values[key] = value;
    }
  }

  return values;
}

std::string get_value(const std::unordered_map<std::string, std::string> &values, const std::string &key) {
  if (const char *env = std::getenv(key.c_str())) {
    return trim(env);
  }

  if (auto it = values.find(key); it != values.end()) {
    return trim(it->second);
  }

  return {};
}

}  // namespace

BeagleConfig load_config() {
  const auto values = read_env_file();

  BeagleConfig cfg;
  cfg.control_plane_url = get_value(values, "BEAGLE_CONTROL_PLANE");
  cfg.api_token = get_value(values, "BEAGLE_STREAM_TOKEN");
  cfg.vm_id = get_value(values, "BEAGLE_VM_ID");
  cfg.stream_server_id = cfg.vm_id.empty() ? "" : "beagle-stream-server-vm" + cfg.vm_id;
  cfg.wireguard_active = detect_wireguard_active();
  cfg.tls_insecure = truthy(get_value(values, "BEAGLE_TLS_INSECURE"));
  return cfg;
}

bool detect_wireguard_active() {
  return std::filesystem::exists("/sys/class/net/wg-beagle");
}

}  // namespace beagle
