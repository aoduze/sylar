//
// Created by admin on 2025/8/19.
//

#ifndef ENDIAN_H
#define ENDIAN_H

#define SYLAR_LITTLE_ENDIAN 1
#define SYLAR_BIG_ENDIAN 2

#include <byteswap.h>
#include <stdint.h>

namespace sylar {

    // 首先使用enablie_if检测是否是8字节数据量
    // 随后使用bswap_64来进行转换
    template <class T>
    typename std::enable_if<sizeof(T) == sizeof(uint8_t), T>::type
    byteswap(T value) {
        return (T)bswap_64((uint64_t)value);
    }

    template<class T>
    typename std::enable_if<sizeof(T) == sizeof(uint32_t), T>::type
    byteswap(T value) {
        return (T)bswap_32((uint32_t)value);
    }

    template<class T>
    typename std::enable_if<sizeof(T) == sizeof(uint16_t), T>::type
    byteswap(T value) {
        return (T)bswap_16((uint16_t)value);
    }

    /**
     * @brief 檢測系統的字序（Endianness），並定義一個專案內部統一的宏來表示。
     *
     * BYTE_ORDER 和 BIG_ENDIAN 是由系統標頭檔（如 <endian.h>）提供的宏。
     * 這段程式碼的目的是將系統級別的字序宏，轉換為專案（sylar）內部自定義的、
     * 統一的宏（SYLAR_BYTE_ORDER），以便在專案內部以一致的方式處理字序問題，
     * 增加程式碼的可移植性和可讀性。
     */
#if BYTE_ORDER == BIG_ENDIAN
    // 如果系統宏 BYTE_ORDER 等於 BIG_ENDIAN，表示當前系統為「大字序」（Big Endian）。
    // 在這種情況下，將專案自定義的字序宏 SYLAR_BYTE_ORDER 定義為 SYLAR_BIG_ENDIAN (其值為 2)。
#define SYLAR_BYTE_ORDER SYLAR_BIG_ENDIAN
#else
    // 如果不滿足上述條件，則表示當前系統為「小字序」（Little Endian）。
    // 在這種情況下，將專案自定義的字序宏 SYLAR_BYTE_ORDER 定義為 SYLAR_LITTLE_ENDIAN (其值為 1)。
#define SYLAR_BYTE_ORDER SYLAR_LITTLE_ENDIAN
#endif

    /**
     * @brief 根據前面已定義好的專案內部字序宏，進行條件編譯。
     *
     * 這裡的程式碼塊將只在「大字序」的系統上被編譯。
     * 你可以在這個 #if ... #endif 區塊內放置只適用於大字序系統的程式碼實現。
     * 例如，網路位元組序（Network Byte Order）就是大字序，相關的轉換函式在這裡可能會有特殊處理。
     */
#if SYLAR_BYTE_ORDER == SYLAR_BIG_ENDIAN

    /**
     * @brief 只在小端机器上执行byteswap, 在大端机器上什么都不做
     */
    template<class T>
    T byteswapOnLittleEndian(T t) {
        return t;
    }

    /**
     * @brief 只在大端机器上执行byteswap, 在小端机器上什么都不做
     */
    template<class T>
    T byteswapOnBigEndian(T t) {
        return byteswap(t);
    }
#else

    /**
     * @brief 只在小端机器上执行byteswap, 在大端机器上什么都不做
     */
    template<class T>
    T byteswapOnLittleEndian(T t) {
        return byteswap(t);
    }

    /**
     * @brief 只在大端机器上执行byteswap, 在小端机器上什么都不做
     */
    template<class T>
    T byteswapOnBigEndian(T t) {
        return t;
    }
#endif

}

#endif //ENDIAN_H
