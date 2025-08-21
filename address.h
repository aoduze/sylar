//
// Created by admin on 2025/8/19.
//

#ifndef ADDRESS_H
#define ADDRESS_H

#include <memory>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <map>

namespace sylar {
    class IPAddress;

    class Address {
    public:
        typedef std::shared_ptr<Address> ptr;

        // 使用socketaddr 指针创建Address
        // 失败返回nullptr
        static Address::ptr Create(const sockaddr* addr, socklen_t addrlen);

        /**
         * @brief 通过host地址返回对应条件的所有Address
         * @param[out] result 保存满足条件的Address
         * @param[in] host 域名,服务器名等.举例: www.sylar.top[:80] (方括号为可选内容)
         * @param[in] family 协议族(AF_INT, AF_INT6, AF_UNIX)
         * @param[in] type socketl类型SOCK_STREAM、SOCK_DGRAM 等
         * @param[in] protocol 协议,IPPROTO_TCP、IPPROTO_UDP 等
         * @return 返回是否转换成功
         */
        static bool Lookup(std::vector<Address::ptr>& result, const std::string& host,
                int family = AF_INET, int type = 0, int protocol = 0);

        /**
         * @brief 通过host地址返回对应条件的任意IPAddress
         * @param[in] host 域名,服务器名等.举例: www.sylar.top[:80] (方括号为可选内容)
         * @param[in] family 协议族(AF_INT, AF_INT6, AF_UNIX)
         * @param[in] type socketl类型SOCK_STREAM、SOCK_DGRAM 等
         * @param[in] protocol 协议,IPPROTO_TCP、IPPROTO_UDP 等
         * @return 返回满足条件的任意Address,失败返回nullptr
         */
        static Address::ptr LookupAny(const std::string& host,
                int family = AF_INET, int type = 0, int protocol = 0);

         /**
         * @brief 通过host地址返回对应条件的任意IPAddress
         * @param[in] host 域名,服务器名等.举例: www.sylar.top[:80] (方括号为可选内容)
         * @param[in] family 协议族(AF_INT, AF_INT6, AF_UNIX)
         * @param[in] type socketl类型SOCK_STREAM、SOCK_DGRAM 等
         * @param[in] protocol 协议,IPPROTO_TCP、IPPROTO_UDP 等
         * @return 返回满足条件的任意IPAddress,失败返回nullptr
         */
        static std::shared_ptr<IPAddress> LookupAnyIPAddress(const std::string& host,
                int family = AF_INET, int type = 0, int protocol = 0);

        // 返回本机所有的网卡的< 网卡名， 地址, 子网掩码的位数>
        // result 保存本机所有地址
        // return 是否获取成功
        static bool GetInterfaceAddresses(std::multimap<std::string
                        , std::pair<Address::ptr, uint32_t> >& result,
                        int family = AF_INET);

        // 获取指定网卡的地址信息
        static bool GetInterfaceAddresses(std::vector<std::pair<Address::ptr, uint32_t> >&result
                            ,const std::string& iface, int family = AF_INET);

        // 析构函数
        virtual ~Address() {}

        // 返回协议簇
        int getFamily() const;

        // 返回sockaddr指针, 只读
        virtual const sockaddr* getAddr() const = 0;

        // 读写
        virtual sockaddr* getAddr() = 0;

        // 返回socketaddr 的长度
        virtual socklen_t getAddrLen() const = 0;

        // 可读性输出地址
        virtual std::ostream& insert(std::ostream& os) const = 0;

        // 返回可读性字符串
        std::string toString() const;

        // 重载比较运算符
        bool operator<(const Address& rhs) const;
        bool operator==(const Address& rhs) const;
        bool operator!=(const Address& rhs) const;
    };

    // ip地址的基类
    class IPAddress : public Address {
    public:
        typedef std::shared_ptr<IPAddress> ptr;

        // 通过域名，ip，服务器名等创建IPAddress
        static IPAddress::ptr Create(const char* address, uint16_t port = 0);

        // 获取该地址的广播地址
        // prefix为子网掩码位数
        // 失败返回nullptr
        virtual IPAddress::ptr broadcastAddress(uint32_t prefix_len) = 0;

        // 获取网段
        virtual IPAddress::ptr networdAddress(uint32_t prefix_len) = 0;

        // 获取子网掩码地址
        virtual IPAddress::ptr subnetMask(uint32_t prefix_len) = 0;

        // 返回端口号
        virtual uint32_t getPort() const = 0;

        // 设置端口号
        virtual void setPort(uint16_t port) = 0;
    };

    class IPv4Address : public IPAddress {
        typedef std::shared_ptr<IPv4Address> ptr;
    public:
        // 使用点分十进制地址创建IPv4Address
        // address与port分别对应ip和端口
        // 成功返回ipv4address,失败返回nullptr
        static IPv4Address::ptr Create(const char* address, uint16_t port = 0);

        // 通过sockaddr地址来创建IPv4Address
        IPv4Address(const sockaddr_in& address);

        // 通过二进制地址构造
        IPv4Address(uint32_t address = INADDR_ANY, uint16_t port = 0);

        const sockaddr* getAddr() const override;
        sockaddr* getAddr() override;
        socklen_t getAddrLen() const override;
        std::ostream& insert(std::ostream& os) const override;

        IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;
        IPAddress::ptr networdAddress(uint32_t prefix_len) override;
        IPAddress::ptr subnetMask(uint32_t prefix_len) override;
        uint32_t getPort() const override;
        void setPort(uint16_t v) override;
    private:
        sockaddr_in m_addr;
    };

    class IPv6Address : public IPAddress {
    public:
        typedef std::shared_ptr<IPv6Address> ptr;

        /**
         * @brief 通过IPv6地址字符串构造IPv6Address
         * @param[in] address IPv6地址字符串
         * @param[in] port 端口号
         */
        static IPv6Address::ptr Create(const char* address, uint16_t port = 0);

        // 由于Cpp语法的限制, sockaddr_int6由于是一个结构体
        // 而结构体无法本身无法作为默认参数传递,所以我们选用无参数构造函数
        IPv6Address();

        /**
         * @brief 通过sockaddr_in6构造IPv6Address
         * @param[in] address sockaddr_in6结构体
         */
        IPv6Address(const sockaddr_in6& address);

        /**
         * @brief 通过IPv6二进制地址构造IPv6Address
         * @param[in] address IPv6二进制地址
         */
        IPv6Address(const uint8_t address[16], uint16_t port = 0);

        const sockaddr* getAddr() const override;
        sockaddr* getAddr() override;
        socklen_t getAddrLen() const override;
        std::ostream& insert(std::ostream& os) const override;

        IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;
        IPAddress::ptr networdAddress(uint32_t prefix_len) override;
        IPAddress::ptr subnetMask(uint32_t prefix_len) override;
        uint32_t getPort() const override;
        void setPort(uint16_t v) override;
    private:
        sockaddr_in6 m_addr;
    };

    class UnixAddress : public Address {
    public:
        typedef std::shared_ptr<UnixAddress> ptr;

        /**
         * @brief 无参构造函数
         */
        UnixAddress();

        /**
         * @brief 通过路径构造UnixAddress
         * @param[in] path UnixSocket路径(长度小于UNIX_PATH_MAX)
         */
        UnixAddress(const std::string& path);

        const sockaddr* getAddr() const override;
        sockaddr* getAddr() override;
        socklen_t getAddrLen() const override;
        void setAddrLen(uint32_t v);
        std::string getPath() const;
        std::ostream& insert(std::ostream& os) const override;
    private:
        sockaddr_un m_addr;
        socklen_t m_length;
    };

    /**
     * @brief 未知地址
     */
    class UnknownAddress : public Address {
    public:
        typedef std::shared_ptr<UnknownAddress> ptr;
        UnknownAddress(int family);
        UnknownAddress(const sockaddr& addr);
        const sockaddr* getAddr() const override;
        sockaddr* getAddr() override;
        socklen_t getAddrLen() const override;
        std::ostream& insert(std::ostream& os) const override;
    private:
        sockaddr m_addr;
    };

    /**
     * @brief 流式输出Address
     */
    std::ostream& operator<<(std::ostream& os, const Address& addr);

}

#endif //ADDRESS_H
