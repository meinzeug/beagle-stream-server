#include "BeagleAuth.h"

#include "beagle_config.h"
#include "../logging.h"
#include "../nvhttp.h"

#include <cctype>
#include <optional>
#include <string>
#include <string_view>

namespace beagle {

namespace {

std::string base64url_to_base64(std::string value) {
  for (char &ch : value) {
    if (ch == '-') {
      ch = '+';
    } else if (ch == '_') {
      ch = '/';
    }
  }
  while ((value.size() % 4) != 0) {
    value.push_back('=');
  }
  return value;
}

std::optional<std::string> decode_base64(std::string_view value) {
  static constexpr unsigned char DECODE_TABLE[256] = {
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 65, 64, 64,
    64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
    64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
  };

  std::string decoded;
  decoded.reserve((value.size() / 4) * 3);
  int val = 0;
  int bits = -8;
  for (unsigned char c : value) {
    unsigned char d = DECODE_TABLE[c];
    if (d == 64) {
      if (std::isspace(static_cast<unsigned char>(c)) != 0) {
        continue;
      }
      return std::nullopt;
    }
    if (d == 65) {
      break;
    }
    val = (val << 6) + d;
    bits += 6;
    if (bits >= 0) {
      decoded.push_back(static_cast<char>((val >> bits) & 0xFF));
      bits -= 8;
    }
  }
  return decoded;
}

std::optional<std::string> extract_json_string_claim(const std::string &json_payload, const std::string &claim_name) {
  const std::string key = "\"" + claim_name + "\"";
  std::size_t pos = json_payload.find(key);
  if (pos == std::string::npos) {
    return std::nullopt;
  }
  pos = json_payload.find(':', pos + key.size());
  if (pos == std::string::npos) {
    return std::nullopt;
  }
  ++pos;
  while (pos < json_payload.size() && std::isspace(static_cast<unsigned char>(json_payload[pos])) != 0) {
    ++pos;
  }
  if (pos >= json_payload.size() || json_payload[pos] != '"') {
    return std::nullopt;
  }
  ++pos;

  std::string value;
  while (pos < json_payload.size()) {
    const char ch = json_payload[pos++];
    if (ch == '\\') {
      if (pos < json_payload.size()) {
        value.push_back(json_payload[pos++]);
      }
      continue;
    }
    if (ch == '"') {
      return value;
    }
    value.push_back(ch);
  }
  return std::nullopt;
}

std::optional<std::string> extract_jwt_claim(const std::string &token, const std::string &claim_name) {
  const std::size_t first_dot = token.find('.');
  if (first_dot == std::string::npos) {
    return std::nullopt;
  }
  const std::size_t second_dot = token.find('.', first_dot + 1);
  if (second_dot == std::string::npos || second_dot <= first_dot + 1) {
    return std::nullopt;
  }

  const std::string payload_b64url = token.substr(first_dot + 1, second_dot - first_dot - 1);
  const std::string payload_b64 = base64url_to_base64(payload_b64url);
  const auto decoded_payload = decode_base64(payload_b64);
  if (!decoded_payload.has_value()) {
    return std::nullopt;
  }
  return extract_json_string_claim(decoded_payload.value(), claim_name);
}

}  // namespace

bool accept_pairing_token(const std::string &token, const std::string &name) {
  std::string pairing_value;

  // Prefer the token-native secret claim, then compatibility pin claim.
  if (auto claim = extract_jwt_claim(token, "pairing_secret"); claim.has_value() && !claim->empty()) {
    pairing_value = *claim;
  } else if (auto claim = extract_jwt_claim(token, "pairing_pin"); claim.has_value() && !claim->empty()) {
    pairing_value = *claim;
  }

  if (pairing_value.empty()) {
    pairing_value = token;
  }

  return nvhttp::pin(pairing_value, name);
}

bool check_vpn_policy(const std::string &network_mode, bool wireguard_active) {
  if (network_mode == "vpn_required" && !(wireguard_active || detect_wireguard_active())) {
    BOOST_LOG(warning) << "Beagle pairing rejected because vpn_required policy is active and wg-beagle is unavailable";
    return false;
  }
  return true;
}

}  // namespace beagle
