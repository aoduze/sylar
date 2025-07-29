//
// Created by admin on 2025/7/29.
// 配置系统实现文件
// 包含ConfigVar模板类和Config类的实现
//

#include "Config.h"
#include "log.h"

namespace sylar {
    /**
     * @brief 模板化配置变量类的实现
     * @tparam T 配置值的数据类型
     * 继承自ConfigVarBase，提供类型安全的配置变量实现
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
                << e.what() << " convert: string to" << typeid(m_class).name()
                << " to string";
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
            } catch (std::exception& e) {
                std::cout<< "ConfigVar::toString exception"
                    << e.what() << "convert: string to " << typeid(m_class).name()
                    << " - ";
            }
            return false;
        }

        /// 获取配置值
        const T getValue() const { return m_class; }
        /// 设置配置值
        void setValue(const T& val) { m_class = val; }

        /// 获取配置值的类型名称
        std::string getTypeName() const override { return typeid(T).name();}
    private:
        T m_class; ///< 存储实际的配置值
    };

    /**
     * @brief 配置管理器类
     * 静态类，提供全局配置管理功能
     * 支持配置项的查找、创建、类型检查等功能
     */
    class Config {
    public:
        /// 智能指针类型定义
        typedef std::shared_ptr<Config> ptr;

        /**
         * @brief 查找或创建配置项（带默认值版本）
         * @tparam T 配置值的数据类型
         * @param name 配置项名称
         * @param default_value 默认值
         * @param description 配置项描述信息
         * @return 配置项的智能指针
         */
        template<class T>
        static typename ConfigVar<T>::ptr Lookup(const std::string& name,
            const T& default_value, const std::string& description = "") {
            // 在全局配置容器中查找配置项
            auto it = GetDatas().find(name);
            if (it != GetDatas().end()) {
                // 智能指针强转,这里是向下转,一般来说是安全的
                // 向上转的时候dynamic_pointer_cast虽然会帮我们检查,但也要注意
                auto tmp = std::dynamic_pointer_cast<ConfigVar<T> >(it->second);
                if (tmp) {
                    // 类型匹配，返回已存在的配置项
                    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "Lookup name " << name << " exists";
                    return tmp;
                } else {
                    // 类型不匹配，记录错误信息
                    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "Lookup name " << name << " exists but type not "
                        << typeid(T).name() << " real_type = " << it->second->getTypeName()
                        << " " << it->second->toString();
                    return nullptr;
                }
            }

            // 配置项不存在，验证配置名称格式
            // 只允许小写字母、数字、点号和下划线
            if (name.find_first_not_of("abcdefghikjlmnopqrstuvwxyz._0123456789") != std::string::npos) {
                SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "Lookup name invalid " << name;
                throw std::invalid_argument(name);
            }

            // 创建新的配置项并添加到全局容器中
            typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, default_value, description));
            GetDatas()[name] = v;
            return v;
        }

        /**
         * @brief 查找已存在的配置项（仅查找版本）
         * @tparam T 配置值的数据类型
         * @param name 配置项名称
         * @return 配置项的智能指针，不存在时返回nullptr
         */
        template<class T>
        static typename ConfigVar<T>::ptr Lookup(const std::string& name) {
            auto it = GetDatas().find(name);
            if (it == GetDatas().end()) {
                return nullptr;
            }
            return std::dynamic_pointer_cast<ConfigVar<T> >(it->second);
        }

        /**
         * @brief 从YAML节点加载配置（未完成的方法）
         * @param root YAML根节点
         * 注意：此方法实现不完整，存在语法错误
         */
        static void LoadFromYaml(const YAML::Node& root) {
            static ConfigVarMap s_datas;
            return s_dates; // 注意：这里有错误，应该是s_datas
        }
    };

    /**
     * @brief 查找配置项基类指针的实现
     * @param name 配置项名称
     * @return 配置项基类指针，不存在时返回nullptr
     * 此方法不进行类型转换，直接返回基类指针
     */
    ConfigVarBase::ptr Config::LookupBase(const std::string& name) {
        auto it = GetDatas().find(name);
        return it == GetDatas().end() ? nullptr : it->second;
    }
}