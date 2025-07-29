//
// Created by admin on 2025/7/29.
//

#ifndef CONFIG_H
#define CONFIG_H

#include <memory>          // 智能指针支持
#include <algorithm>       // std::transform等算法
#include <string>          // 字符串类型
#include <vector>          // 向量容器
#include <iostream>        // 输入输出流
#include <map>             // 映射容器，用于存储配置项
#include <boost/lexical_cast.hpp>  // Boost类型转换库
#include <typeinfo>        // 运行时类型信息
#include <yaml-cpp/yaml.h> // YAML解析库

namespace sylar {
    /**
     * @brief 配置变量基类
     * 所有配置变量的抽象基类，定义了配置项的基本接口
     * 提供配置项名称、描述信息的管理，以及序列化/反序列化的纯虚接口
     */
    class ConfigVarBase {
    public:
        /// 智能指针类型定义
        typedef std::shared_ptr<ConfigVarBase> ptr;

        /**
         * @brief 构造函数
         * @param name 配置项名称（会自动转换为小写）
         * @param description 配置项描述信息
         */
        ConfigVarBase(const std::string& name, const std::string& description)
            :m_name(name)
            ,m_description(description) {
            // 将配置名称转换为小写，确保配置名称的一致性
            std::transform(m_name.begin(),m_name.end(),m_name.begin(),::tolower);
            //transform将容器内元素做操作.并存储到第二参数的元素中
        }

        /// 虚析构函数，确保派生类正确析构
        virtual ~ConfigVarBase() {}

        /// 获取配置项名称
        const std::string& getName() const { return m_name; }
        /// 获取配置项描述信息
        const std::string& getDescription() const { return m_description; }

        /**
         * @brief 将配置值转换为字符串（序列化）
         * @return 配置值的字符串表示
         * 设为纯虚函数，由派生类实现具体的转换逻辑
         */
        virtual std::string toString() = 0;

        /**
         * @brief 从字符串解析配置值（反序列化）
         * @param val 要解析的字符串
         * @return 解析是否成功
         * 设为纯虚函数，由派生类实现具体的解析逻辑
         */
        virtual bool fromString(const std::string& val) = 0;

        /**
         * @brief 获取配置值的类型名称
         * @return 类型名称字符串
         * 设为纯虚函数，用于运行时类型识别和调试
         */
        virtual std::string getTypeName() const = 0;

    protected:
        std::string m_name;        ///< 配置项名称（小写）
        std::string m_description; ///< 配置项描述信息
    };

    /**
     * @brief 模板化配置变量类
     * @tparam T 配置值的数据类型
     * 继承自ConfigVarBase，实现类型安全的配置变量
     * 支持任意可序列化的数据类型
     */
    template<class T>
    class ConfigVar : public ConfigVarBase {
    public:
        /// 智能指针类型定义
        typedef std::shared_ptr<ConfigVar<T>> ptr;

        /**
         * @brief 构造函数
         * @param name 配置项名称
         * @param default_value 默认值
         * @param description 配置项描述信息
         */
        ConfigVar(const std::string& name,
                 const T& default_value,
                 const std::string& description = "")
            :ConfigVarBase(name, description)
            ,m_class(default_value) {}

        /**
         * @brief 将配置值转换为字符串
         * @return 配置值的字符串表示
         * 使用boost::lexical_cast进行类型安全的转换
         */
        std::string toString() override {
            try {
                return boost::lexical_cast<std::string>(m_class);
            } catch (std::exception& e) {
                std::cout << "ConfigVar::toString exception "
                << e.what() << " convert: " << typeid(m_class).name()
                << " to string" << std::endl;
            }
            return "";
        }

        /**
         * @brief 从字符串解析配置值
         * @param val 要解析的字符串
         * @return 解析是否成功
         * 使用boost::lexical_cast进行类型安全的转换
         */
        bool fromString(const std::string &val) override {
            try {
                m_class = boost::lexical_cast<T>(val);
                return true;
            } catch (std::exception& e) {
                std::cout << "ConfigVar::fromString exception "
                    << e.what() << " convert: string to " << typeid(m_class).name()
                    << std::endl;
            }
            return false;
        }

        /**
         * @brief 获取配置值
         * @return 当前配置值
         */
        const T getValue() const { return m_class; }

        /**
         * @brief 设置配置值
         * @param val 新的配置值
         */
        void setValue(const T& val) { m_class = val; }

        /**
         * @brief 获取配置值的类型名称
         * @return 类型名称字符串
         * 使用typeid获取运行时类型信息
         */
        std::string getTypeName() const override { return typeid(T).name();}

    private:
        T m_class; ///< 存储实际的配置值
    };

    /**
     * @brief 配置管理器类
     * 静态类，提供全局配置管理功能
     * 支持配置项的查找、创建、类型检查等功能
     * 采用单例模式管理全局配置容器
     */
    class Config {
    public:
        /// 智能指针类型定义
        typedef std::shared_ptr<Config> ptr;
        /// 配置变量映射表类型定义：配置名称 -> 配置对象
        typedef std::map<std::string, ConfigVarBase::ptr> ConfigVarMap;

        /**
         * @brief 查找或创建配置项（带默认值版本）
         * @tparam T 配置值的数据类型
         * @param name 配置项名称
         * @param default_value 默认值（配置项不存在时使用）
         * @param description 配置项描述信息
         * @return 配置项的智能指针，类型不匹配时返回nullptr
         *
         * 工作流程：
         * 1. 在全局配置容器中查找指定名称的配置项
         * 2. 如果找到，进行类型安全检查
         * 3. 如果类型匹配，返回配置项；否则返回nullptr
         * 4. 如果未找到，验证配置名称格式
         * 5. 创建新的配置项并添加到容器中
         */
        template<class T>
        static typename ConfigVar<T>::ptr Lookup(const std::string& name,
            const T& default_value, const std::string& description = "") {
            // 在全局配置容器中查找配置项
            auto it = GetDatas().find(name);
            if (it != GetDatas().end()) {
                // 配置项已存在，进行类型安全的向下转型
                // 智能指针强转,这里是向下转,一般来说是安全的
                // 向上转的时候dynamic_pointer_cast虽然会帮我们检查,但也要注意
                auto tmp = std::dynamic_pointer_cast<ConfigVar<T> >(it->second);
                if (tmp) {
                    // 类型匹配，返回已存在的配置项
                    std::cout << "Lookup name " << name << " exists" << std::endl;
                    return tmp;
                } else {
                    // 类型不匹配，输出错误信息并返回nullptr
                    std::cout << "Lookup name " << name << " exists but type not "
                        << typeid(T).name() << " real_type = " << it->second->getTypeName()
                        << " " << it->second->toString() << std::endl;
                    return nullptr;
                }
            }

            // 配置项不存在，验证配置名称格式
            // 只允许小写字母、数字、点号和下划线
            if (name.find_first_not_of("abcdefghikjlmnopqrstuvwxyz._0123456789") != std::string::npos) {
                std::cout << "Lookup name invalid " << name << std::endl;
                throw std::invalid_argument(name);
            }

            // 创建新的配置项
            typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, default_value, description));
            // 添加到全局配置容器中
            GetDatas()[name] = v;
            return v;
        }

        /**
         * @brief 查找已存在的配置项（仅查找版本）
         * @tparam T 配置值的数据类型
         * @param name 配置项名称
         * @return 配置项的智能指针，不存在或类型不匹配时返回nullptr
         *
         * 注意：此版本不会创建新的配置项，仅用于查找已存在的配置
         */
        template<class T>
        static typename ConfigVar<T>::ptr Lookup(const std::string& name) {
            auto it = GetDatas().find(name);
            if (it == GetDatas().end()) {
                return nullptr;
            }
            // 进行类型安全的转换
            return std::dynamic_pointer_cast<ConfigVar<T> >(it->second);
        }

        /**
         * @brief 获取全局配置容器
         * @return 配置变量映射表的引用
         *
         * 使用静态局部变量实现单例模式
         * 确保全局唯一的配置存储容器
         */
        static ConfigVarMap& GetDatas() {
            static ConfigVarMap s_datas;
            return s_datas;
        }

        /**
         * @brief 查找配置项基类指针（无类型检查版本）
         * @param name 配置项名称
         * @return 配置项基类指针，不存在时返回nullptr
         *
         * 此方法返回基类指针，不进行类型转换
         * 主要用于内部实现和调试
         */
        static ConfigVarBase::ptr LookupBase(const std::string& name);
    };
}

#endif //CONFIG_H
