#pragma once

#include <string>

namespace beagle {

bool accept_pairing_token(const std::string &token, const std::string &name);
bool check_vpn_policy(const std::string &network_mode, bool wireguard_active);

}  // namespace beagle
