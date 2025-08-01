#include "Config.h"


namespace sylar {
    //接下来我们通过模板来配置不同的子类
    template<class T>
    class ConfigVar : public ConfigVarBase {
    public:
        typedef std::shared_ptr<ConfigVar> ptr;

        ConfigVar(const std::string& name
                ,const T& default_value
                ,const std::string& description = "")
            :ConfigVarBase(name, description)
            ,m_val(default_value) {}

        std::string toString() override {
            try {
                return boost::lexical_cast<std::string>(m_val);
            } catch (std::exception& e) {
                std::cout<< "ConfigVar::toString exception"
                << e.what() << "convert: " << typeid(m_val).name() << " to string";
            }
            return "";
        }

        bool fromString(const std::string& val) override {
            try {
                m_val = boost::lexical_cast<T>(val);
            } catch (std::exception& e) {
                std::cout<< "ConfigVar::fromString exception"
                << e.what() << "convert: string to " << typeid(m_val).name();
            }
            return false;
        }

        const T getValue() const { return m_val;}
        void setValue(const T& v) { m_val = v;}

        std::string getTypeName() const override { return typeid(T).name();}

    private:
        T m_val;
    };

    //Config类用于处理yaml配置文件
    class Config{
    public:
        typedef std::unordered_map<std::string, ConfigVarBase::ptr> ConfigVarMap;
        typedef RWMutex RWMutexType;

        template<class T>
        static typename ConfigVar<T>::ptr Lookup(const std::string& name,
                const T& default_value, const std::string& description = "") {
            auto it = GetDatas().find(name);
            if (it != GetDatas().end()) {
                auto tmp = std::dynamic_pointer_cast<ConfigVar<T> >(it -> second);
                if (tmp) {
                    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "Lookup name=" << name << " exists";
                    return tmp;
                } else {
                    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "Lookup name=" << name << " exists but type not "
                    << typeid(T).name() << " real_type=" << it->second->getTypeName()
                    << " " << it->second->toString();
                    return nullptr;
                }
            }

            if ( name.find_first_not_of("abcdefghikjlmnopqrstuvwxyz._0123456789")
                != std::string::npos) {
                SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Lookup name invalid " << name;
                throw std::invalid_argument(name);
            }

            typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, default_value, description));
            GetDatas()[name] = v;
            return v;
        }

        template <class T>
        static typename ConfigVar<T>::ptr Lookup(const std::string& name) {
            auto it = GetDatas().find(name);
            if(it == GetDatas().end()) {
                return nullptr;
            }
            return std::dynamic_pointer_cast<ConfigVar<T> >(it->second);
        }

        static void LoadFromYaml(const YAML::Node& root);
        static ConfigVarBase::ptr LookupBase(const std::string& name);
    private:
        static ConfigVarMap& GetDatas() {
            static ConfigVarMap s_datas;
            return s_datas;
        }

        static RWMutexType& GetMutex() {
            static RWMutexType s_mutex;
            return s_mutex;
        }
        };

    ConfigVarBase::ptr Config::LookupBase(const std::string& name) {
        auto it = GetDatas().find(name);
        return it == GetDatas().end() ? nullptr : it->second;
    }
    };


