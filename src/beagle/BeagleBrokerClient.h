#pragma once

#include "beagle_config.h"

#include <atomic>
#include <functional>
#include <string>
#include <thread>

namespace beagle {

using ConfigCallback = std::function<void(
  int max_fps,
  int max_bitrate_mbps,
  const std::string &resolution,
  const std::string &codec,
  const std::string &network_mode)>;

class BeagleBrokerClient {
public:
  explicit BeagleBrokerClient(BeagleConfig cfg);
  ~BeagleBrokerClient();

  const BeagleConfig &config() const;
  bool register_with_control_plane(const std::string &host, int port);
  bool validate_pairing_token(const std::string &token, const std::string &device_name);
  void fetch_config(ConfigCallback on_config);
  void report_event(const std::string &event_type, const std::string &outcome, const std::string &client_id = "");
  void start_config_refresh(ConfigCallback on_config);
  void stop_config_refresh();

private:
  BeagleConfig cfg_;
  std::thread refresh_thread_;
  std::atomic<bool> stop_refresh_ {false};

  std::string http_get(const std::string &path, long *status_code = nullptr);
  std::string http_post(const std::string &path, const std::string &body, long *status_code = nullptr);
};

extern BeagleBrokerClient *g_broker;

}  // namespace beagle
