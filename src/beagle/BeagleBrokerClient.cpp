#include "BeagleBrokerClient.h"

#include <chrono>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <utility>

#include "../logging.h"

namespace beagle {
namespace {

size_t write_response(char *ptr, size_t size, size_t nmemb, void *userdata) {
  auto *buffer = static_cast<std::string *>(userdata);
  buffer->append(ptr, size * nmemb);
  return size * nmemb;
}

std::string join_url(const std::string &base, const std::string &path) {
  if (base.empty()) {
    return path;
  }
  if (path.empty()) {
    return base;
  }
  if (base.back() == '/' && path.front() == '/') {
    return base.substr(0, base.size() - 1) + path;
  }
  if (base.back() != '/' && path.front() != '/') {
    return base + "/" + path;
  }
  return base + path;
}

bool http_success(long status_code) {
  return status_code >= 200 && status_code < 300;
}

}  // namespace

BeagleBrokerClient *g_broker = nullptr;

BeagleBrokerClient::BeagleBrokerClient(BeagleConfig cfg):
    cfg_(std::move(cfg)) {
}

BeagleBrokerClient::~BeagleBrokerClient() {
  stop_config_refresh();
}

std::string BeagleBrokerClient::http_get(const std::string &path, long *status_code) {
  CURL *curl = curl_easy_init();  // NOSONAR
  if (!curl) {
    BOOST_LOG(warning) << "Beagle broker HTTP init failed";
    return {};
  }

  std::string response;
  struct curl_slist *headers = nullptr;
  const auto token_header = "X-Beagle-Token: " + cfg_.api_token;
  headers = curl_slist_append(headers, token_header.c_str());
  headers = curl_slist_append(headers, "Content-Type: application/json");

  const auto url = join_url(cfg_.control_plane_url, path);
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
  if (cfg_.tls_insecure) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  }

  const CURLcode result = curl_easy_perform(curl);
  long code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
  if (status_code) {
    *status_code = code;
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  if (result != CURLE_OK) {
    BOOST_LOG(warning) << "Beagle broker GET failed: " << curl_easy_strerror(result);
    return {};
  }

  if (!http_success(code)) {
    BOOST_LOG(warning) << "Beagle broker GET returned HTTP " << code << " for path=" << path;
    return {};
  }

  return response;
}

std::string BeagleBrokerClient::http_post(const std::string &path, const std::string &body, long *status_code) {
  CURL *curl = curl_easy_init();  // NOSONAR
  if (!curl) {
    BOOST_LOG(warning) << "Beagle broker HTTP init failed";
    return {};
  }

  std::string response;
  struct curl_slist *headers = nullptr;
  const auto token_header = "X-Beagle-Token: " + cfg_.api_token;
  headers = curl_slist_append(headers, token_header.c_str());
  headers = curl_slist_append(headers, "Content-Type: application/json");

  const auto url = join_url(cfg_.control_plane_url, path);
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
  if (cfg_.tls_insecure) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  }

  const CURLcode result = curl_easy_perform(curl);
  long code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
  if (status_code) {
    *status_code = code;
  }

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  if (result != CURLE_OK) {
    BOOST_LOG(warning) << "Beagle broker POST failed: " << curl_easy_strerror(result);
    return {};
  }

  if (!http_success(code)) {
    BOOST_LOG(warning) << "Beagle broker POST returned HTTP " << code << " for path=" << path;
    return {};
  }

  return response;
}

bool BeagleBrokerClient::register_with_control_plane(const std::string &host, int port) {
  int vm_id = 0;
  try {
    vm_id = std::stoi(cfg_.vm_id);
  } catch (const std::exception &) {
    BOOST_LOG(warning) << "Beagle registration skipped: invalid VM id";
    return false;
  }

  nlohmann::json body {
    {"vm_id", vm_id},
    {"stream_server_id", cfg_.stream_server_id},
    {"host", host},
    {"port", port},
    {"wireguard_active", detect_wireguard_active()},
    {"server_version", PROJECT_VERSION},
    {"capabilities", nlohmann::json::object()},
  };

  long status_code = 0;
  http_post("/api/v1/streams/register", body.dump(), &status_code);
  const bool ok = http_success(status_code);
  BOOST_LOG(info) << "Beagle registration: " << (ok ? "success" : "failed") << " status=" << status_code;
  return ok;
}

void BeagleBrokerClient::fetch_config(ConfigCallback on_config) {
  long status_code = 0;
  const auto body = http_get("/api/v1/streams/" + cfg_.vm_id + "/config", &status_code);
  if (!http_success(status_code) || body.empty()) {
    BOOST_LOG(warning) << "Beagle config fetch failed status=" << status_code;
    return;
  }

  try {
    const auto json = nlohmann::json::parse(body);
    const auto config = json.at("config");
    const auto policy = config.at("policy");
    on_config(
      policy.value("max_fps", 60),
      policy.value("max_bitrate_mbps", 20),
      policy.value("resolution", std::string {"1920x1080"}),
      policy.value("codec", std::string {"h264"}),
      policy.value("network_mode", std::string {"vpn_preferred"}));
  } catch (const std::exception &e) {
    BOOST_LOG(warning) << "Beagle config parse failed: " << e.what();
  }
}

void BeagleBrokerClient::report_event(const std::string &event_type, const std::string &outcome, const std::string &client_id) {
  nlohmann::json details = nlohmann::json::object();
  if (!client_id.empty()) {
    details["client_id"] = client_id;
  }
  details["wireguard_active"] = detect_wireguard_active();

  nlohmann::json body {
    {"event_type", event_type},
    {"outcome", outcome},
    {"details", details},
  };

  long status_code = 0;
  http_post("/api/v1/streams/" + cfg_.vm_id + "/events", body.dump(), &status_code);
  if (!http_success(status_code)) {
    BOOST_LOG(warning) << "Beagle event report failed type=" << event_type << " status=" << status_code;
  }
}

void BeagleBrokerClient::start_config_refresh(ConfigCallback on_config) {
  stop_config_refresh();
  stop_refresh_.store(false, std::memory_order_relaxed);
  refresh_thread_ = std::thread([this, on_config = std::move(on_config)]() {
    while (!stop_refresh_.load(std::memory_order_relaxed)) {
      fetch_config(on_config);
      for (int i = 0; i < 60 && !stop_refresh_.load(std::memory_order_relaxed); ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    }
  });
}

void BeagleBrokerClient::stop_config_refresh() {
  stop_refresh_.store(true, std::memory_order_relaxed);
  if (refresh_thread_.joinable()) {
    refresh_thread_.join();
  }
}

}  // namespace beagle
