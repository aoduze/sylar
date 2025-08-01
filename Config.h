
#ifndef CONFIG_H
#define CONFIG_H

#include "yaml-cpp/yaml.h"
#include "log.h"
#include <iostream>
#include <memory>
#include <string>
#include <boost/lexical_cast.hpp>
#include <unordered_map>
#include <algorithm>

namespace sylar {
    //配置的基类
    class ConfigVarBase {
    public:
        typedef std::shared_ptr<ConfigVarBase> ptr;
        ConfigVarBase(const std::string& name, const std::string& description)
            :m_name(name)
            ,m_description(description) {
            std::transform(m_name.begin(), m_name.end(), m_name.begin(), ::tolower);
            //我们统一对字符进行转换操作
        }

        virtual ~ConfigVarBase() {}

        const std::string& getName() const { return m_name;}
        const std::string& getDescription() const { return m_description;}

        virtual std::string toString() = 0;
        virtual bool fromString(const std::string& val) = 0;
        virtual std::string getTypeName() const = 0;

    private:
        std::string m_name;             //配置名称
        std::string m_description;      //配置参数的描述
    };

}












#endif //CONFIG_H
