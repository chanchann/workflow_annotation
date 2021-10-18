#include <memory>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

namespace util
{
template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

static const int INET_ADDR_LEN = 128;

static inline std::string ip_bin_to_str(void* sa_sin_addr, bool ipv4 = true)
{
    struct sockaddr_in sa;
    char str[INET_ADDR_LEN];
    if(ipv4)
        inet_ntop(AF_INET, sa_sin_addr, str, INET_ADDR_LEN);
    else 
        inet_ntop(AF_INET6, sa_sin_addr, str, INET_ADDR_LEN);
    return str;
}


} // namespace util
