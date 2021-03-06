
#ifndef r_discovery_r_device_info_h
#define r_discovery_r_device_info_h

#include <string>

namespace r_disco
{

struct r_device_info
{
    std::string unique_id;
    std::string rtsp_url;
    std::string type;
    std::string auth_password;
    std::string auth_user;
    std::string transport_pref;
};

bool operator < (const r_device_info& a, const r_device_info& b);

}

#endif
