#include "BeagleAuth.h"

#include "beagle_config.h"
#include "../logging.h"
#include "../nvhttp.h"

namespace beagle {

bool accept_pairing_token(const std::string &token, const std::string &name) {
  return nvhttp::pin(token, name);
}

bool check_vpn_policy(const std::string &network_mode, bool wireguard_active) {
  if (network_mode == "vpn_required" && !(wireguard_active || detect_wireguard_active())) {
    BOOST_LOG(warning) << "Beagle pairing rejected because vpn_required policy is active and wg-beagle is unavailable";
    return false;
  }
  return true;
}

}  // namespace beagle
