//
// Created by admin on 2025/8/19.
//

#include "address.h"
#include "log.h"
#include <sstream>
#include <netdb.h>
#include <ifaddrs.h>
#include <stddef.h>

#include "endian.h"

namespace sylar {

    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    template <class T>
    static T CreateMask(uint32_t prefix_len) {
        return (1 << (sizeof(T) * 8 - prefix_len)) - 1;
    }

    template<class T>
    static uint32_t CountBytes(T value) {
        uint32_t result = 0;
        for(; value; ++result) {
            value &= value - 1;
        }
        return result;
    }

    // 返回满足条件的任意Address
    Address::ptr Address::LookupAny(const std::string& host,
            int family, int type, int protocol) {
        std::vector<Address::ptr> result;
        if (Lookup(result, host, family, type, protocol)) {
            return result[0];
        }
        return nullptr;
    }

    IPAddress::ptr Address::LookupAnyIPAddress(const std::string& host,
            int family, int type, int protocol) {
        std::vector<Address::ptr> result;
        if (Lookup(result, host, family, type, protocol)) {
            for(auto& i : result) {
                IPAddress::ptr v = std::dynamic_pointer_cast<IPAddress>(i);
                if(v) {
                    return v;
                }
            }
        }
        return nullptr;
    }

    // addrinfo是我们C库提供的一个结构体，用于将主机名解析为IP地址
    // 类似www.google.com转换为通讯所需要的二进制格式等

    // 1.【IPv6有端口】，如：[FC00:0000:130F:0000:0000:09C0:876A:130B]:8080
    // 2.【IPv6无端口】，如：FC00:0000:130F:0000:0000:09C0:876A:130B
    // 3.【IPv4有端口】，如：192.168.0.1:8080
    // 4.【IPv4无端口】，如：192.168.0.1
    bool Address::Lookup(std::vector<Address::ptr>& result, const std::string& host,
                        int family, int type, int protocol) {
        addrinfo hints, *results, *next;
        hints.ai_flags = 0;
        hints.ai_family = family;
        hints.ai_socktype = type;
        hints.ai_protocol = protocol;
        hints.ai_addrlen = 0;
        hints.ai_canonname = NULL;
        hints.ai_addr = NULL;
        hints.ai_next = NULL;

        // ip地址与端口
        std::string node;
        const char* service = NULL;

        //检查 ipv6address serivce
        if(!host.empty() && host[0] == '[') {
            const char* endipv6 = (const char*)memchr(host.c_str() + 1, ']', host.size() - 1);
            if(endipv6) {
                //TODO check out of range
                if(*(endipv6 + 1) == ':') {
                    service = endipv6 + 2;
                }
                node = host.substr(1, endipv6 - host.c_str() - 1);
            }
        }

        //检查 node serivce
        if(node.empty()) {
            service = (const char*)memchr(host.c_str(), ':', host.size());
            if(service) {
                if(!memchr(service + 1, ':', host.c_str() + host.size() - service - 1)) {
                    node = host.substr(0, service - host.c_str());
                    ++service;
                }
            }
        }

        if(node.empty()) {
            node = host;
        }

        int error = getaddrinfo(node.c_str(), service, &hints, &results);
        if(error) {
            SYLAR_LOG_DEBUG(g_logger) << "Address::Lookup getaddress(" << host << ", "
                << family << ", " << type << ") err=" << error << " errstr="
                << gai_strerror(error);
            return false;
        }

        next = results;
        while(next) {
            result.push_back(Create(next->ai_addr, (socklen_t)next->ai_addrlen));
            //SYLAR_LOG_INFO(g_logger) << ((sockaddr_in*)next->ai_addr)->sin_addr.s_addr;
            next = next->ai_next;
        }

        freeaddrinfo(results);
        return !result.empty();
    }

    bool Address::GetInterfaceAddresses(std::multimap<std::string
            , std::pair<Address::ptr, uint32_t> >& result, int family) {
        struct ifaddrs *next, *results;
        // getifaddrs会动态的帮助我们转换, 无需我们手动向results中添加参数
        if(getifaddrs(&results) != 0) {
            SYLAR_LOG_ERROR(g_logger) << "Address::GetInterfaceAddresses getifaddrs "
                << " err=" << errno << " errstr=" << strerror(errno);
            return false;
        }

        try {
            for (next = results; next; next = next -> ifa_next) {
                Address::ptr addr;
                uint32_t prefix_len = ~0u;
                // AF_UNSPEC表示任意协议簇, 我们这里不想让为任意协议簇
                if (family != AF_UNSPEC && family != next -> ifa_addr -> sa_family) {
                    continue;
                }
                switch (next -> ifa_addr -> sa_family) {
                    case AF_INET: {
                        addr = Create(next -> ifa_addr, sizeof(sockaddr_in));
                        uint32_t netmask = ((sockaddr_in*)next->ifa_netmask)->sin_addr.s_addr;
                        prefix_len = CountBytes(netmask);
                    }break;
                    case AF_INET6: {
                        addr = Create(next -> ifa_addr, sizeof(sockaddr_in6));
                        in6_addr& netmask = ((sockaddr_in6*)next -> ifa_netmask) -> sin6_addr;
                        prefix_len = 0;
                        for (int i = 0; i < 16; ++i) {
                            prefix_len += CountBytes(netmask.s6_addr[i]);
                        }
                    }break;

                    default:
                        break;
                }

                if (addr) {
                    result.insert(std::make_pair(next -> ifa_name,
                          std::make_pair(addr, prefix_len)));
                }
            }
        } catch (...) {
            SYLAR_LOG_ERROR(g_logger) << "Addres::GetInterfaceAddresses exception";
                freeifaddrs(results);
                return false;
        }
    }

    bool Address::GetInterfaceAddresses(std::vector<std::pair<Address::ptr, uint32_t> >&result,
            const std::string& iface, int family) {
        // 代表使用者想要一個「任意」或「未指定」的位址
        if (iface.empty() || iface == "*") {
            if (family == AF_INET || family == AF_UNSPEC) {
                result.push_back(std::make_pair(Address::ptr(new IPv4Address()), 0u));
            }
            if (family == AF_INET6 || family == AF_UNSPEC) {
                result.push_back(std::make_pair(Address::ptr(new IPv6Address()), 0u));
            }
            return true;
        }

        std::multimap<std::string
                ,std::pair<Address::ptr, uint32_t> > results;

        if (!GetInterfaceAddresses(results, family)) {
            return false;
        }

        // 寻找名称符合iface的区间后, 压入result中
        auto its = results.equal_range(iface);
        for (; its.first != its.second; ++its.first) {
            result.push_back(its.first->second);
        }
        return !result.empty();
    }

    int Address::getFamily() const {
        return getAddr() -> sa_family;
    }

    std::string Address::toString() const {
        std::stringstream ss;
        insert(ss);
        return ss.str();
    }

    Address::ptr Address::Create(const sockaddr* addr, socklen_t addrlen) {
        if (addr == nullptr ) {
            return nullptr;
        }

        Address::ptr result;
        switch (addr -> sa_family) {
            case AF_INET:
                result = std::make_shared<IPv4Address>(*(const sockaddr_in*)addr);
                break;
            case AF_INET6:
                result = std::make_shared<IPv6Address>(*(const sockaddr_in6*)addr);
                break;
            default:
                result = std::make_shared<UnknownAddress>(*addr);
                break;
        }
        return result;
    }

    bool Address::operator<(const Address &rhs) const {
        socklen_t minlen = std::min(getAddrLen(), rhs.getAddrLen());
        int result = memcmp(getAddr(), rhs.getAddr(), minlen);
        if (result < 0) {
            return true;
        } else if (result > 0) {
            return false;
        } else if (getAddrLen() < rhs.getAddrLen()) {
            return true;
        }
        return false;
    }

    bool Address::operator==(const Address &rhs) const {
        return getAddrLen() == rhs.getAddrLen()
            && memcmp(getAddr(), rhs.getAddr(), getAddrLen()) == 0;
    }

    // 除了一开始的 < 方法之外, 此外的我们都可以通过this来返回
    bool Address::operator!=(const Address &rhs) const {
        return !(*this == rhs);
    }

    // 基类的创建中, 我们一般使用了getaddrinfo来自动识别
    // 但请注意, addrinfo等操作会带来网络延迟等
    // 请在后续ipv4/6 的操作中注意性能优化
    IPAddress::ptr IPAddress::Create(const char* address, uint16_t port) {
        addrinfo *results, hints;
        memset(&hints, 0, sizeof(addrinfo));

        // AI_NUMERICHOST: 仅返回数字地址
        // AF_UNSPEC: 任意协议簇
        hints.ai_flags = AI_NUMERICHOST;
        hints.ai_family = AF_UNSPEC;

        int error = getaddrinfo(address, NULL, &hints, &results);
        if (error) {
            SYLAR_LOG_ERROR(g_logger) << "IPAddress::Create(" << address
                << ", " << port << ") err=" << error << " errstr="
                << gai_strerror(error);
            return nullptr;
        }

        try {
            IPAddress::ptr ip = std::dynamic_pointer_cast<IPAddress>(
                    Address::Create(results->ai_addr, (socklen_t)results->ai_addrlen));
            if (ip) {
                ip->setPort(port);
            }
            freeaddrinfo(results);
            return ip;
        } catch (...) {
            freeaddrinfo(results);
            return nullptr;
        }
    }

    IPv4Address::ptr IPv4Address::Create(const char* address, uint16_t port) {
        IPv4Address::ptr rt(new IPv4Address);
        rt->m_addr.sin_port = byteswapOnLittleEndian(port);
        int result = inet_pton(AF_INET, address, &rt->m_addr.sin_addr);
        if(result <= 0) {
            SYLAR_LOG_DEBUG(g_logger) << "IPv4Address::Create(" << address << ", "
                    << port << ") rt=" << result << " errno=" << errno
                    << " errstr=" << strerror(errno);
            return nullptr;
        }
        return rt;
    }

    IPv4Address::IPv4Address(const sockaddr_in& address) {
        m_addr = address;
    }


    IPv4Address::IPv4Address(uint32_t address, uint16_t port) {
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sin_family = AF_INET;
        m_addr.sin_port = byteswapOnLittleEndian(port);
        m_addr.sin_addr.s_addr = byteswapOnLittleEndian(address);
    }

    sockaddr* IPv4Address::getAddr() {
        return (sockaddr*)&m_addr;
    }

    const sockaddr* IPv4Address::getAddr() const {
        return (sockaddr*)&m_addr;
    }

    socklen_t IPv4Address::getAddrLen() const {
        return sizeof(m_addr);
    }

    std::ostream& IPv4Address::insert(std::ostream& os) const {
        uint32_t addr = byteswapOnLittleEndian(m_addr.sin_addr.s_addr);
        os << ((addr >> 24) & 0xff) << "."
            << ((addr >> 16) & 0xff) << "."
            << ((addr >> 8) & 0xff) << "."
            << (addr & 0xff);
        os << ":" << byteswapOnLittleEndian(m_addr.sin_port);
        return os;
    }

    // 获取广播地址, prefix_len为子网掩码位数
    IPAddress::ptr IPv4Address::broadcastAddress(uint32_t prefix_len) {
        if(prefix_len > 32) {
            return nullptr;
        }

        sockaddr_in baddr(m_addr);
        baddr.sin_addr.s_addr |= byteswapOnLittleEndian(CreateMask<uint32_t>(prefix_len));
        return IPv4Address::ptr(new IPv4Address(baddr));
    }

    // 获取网段
    IPAddress::ptr IPv4Address::networdAddress(uint32_t prefix_len) {
        if (prefix_len > 32) {
            return nullptr;
        }

        sockaddr_in baddr(m_addr);
        // 正确的实现：使用网络掩码
        uint32_t network_mask = ~CreateMask<uint32_t>(prefix_len);
        baddr.sin_addr.s_addr &= byteswapOnLittleEndian(network_mask);
        return IPv4Address::ptr(new IPv4Address(baddr));
    }

    // 子网掩码地址
    IPAddress::ptr IPv4Address::subnetMask(uint32_t prefix_len) {
        sockaddr_in subnet;
        memset(&subnet, 0, sizeof(subnet));
        subnet.sin_family = AF_INET;
        subnet.sin_addr.s_addr = ~byteswapOnLittleEndian(CreateMask<uint32_t>(prefix_len));
        return IPv4Address::ptr(new IPv4Address(subnet));
    }

    uint32_t IPv4Address::getPort() const {
        // 大小端使用bswap系列函数进行转换
        return byteswapOnLittleEndian(m_addr.sin_port);
    }

    void IPv4Address::setPort(uint16_t v) {
        // 在endian端口我们做了对应的转换
        // 端口号的大小端切换同理区分
        m_addr.sin_port = byteswapOnLittleEndian(v);
    }

    /**
     * 以下为IPv6系列函数
     * 请注意, sock的ipv6端口是一个结构体
     * 请注意默认参数的传递
     */

    IPv6Address::ptr IPv6Address::Create(const char* address, uint16_t port) {
        if(!address || !*address) {
            SYLAR_LOG_DEBUG(g_logger) << "IPv6Address::Create empty address";
            return nullptr;
        }

        IPv6Address::ptr rt(new IPv6Address);
        int result = inet_pton(AF_INET6, address, &rt->m_addr.sin6_addr);
        if(result <= 0) {
            SYLAR_LOG_DEBUG(g_logger) << "IPv6Address::Create(" << address << ", "
                    << port << ") rt=" << result << " errno=" << errno
                    << " errstr=" << strerror(errno);
            return nullptr;
        }
        rt->m_addr.sin6_port = byteswapOnLittleEndian(port);
        return rt;
    }

    ipV6Address::ipV6Address() {
        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sin6_family = AF_INET6;
    }
}