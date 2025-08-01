//
// Created by admin on 2025/8/1.
//

#ifndef SINGLETON_H
#define SINGLETON_H

#include <memory>

namespace sylar {
namespace {
    template<class T, class X, int N>
    T& GetInstanceX() {
        static T v;
        return v;
    }

    template<class T, class X, int N>
    std::shared_ptr<T> GetInstancePtr() {
        static std::shared_ptr<T> v(new T);
        return v;
    }
}

    template<class T, class X = void, int N = 0>
    class Singleton {
    public:
        static T& GetInstance() {
            static T v;
            return v;
        }
        static std::shared_ptr<T> GetInstancePtr() {
            static std::shared_ptr<T> v(new T);
            return v;
        }
    };

    template<class T, class X = void, int N = 0>
    class SingletonPtr {
        static std::shared_ptr<T> GetInstance() {
            static std::shared_ptr<T> v(new T);
            return v;
        }
    };
}

#endif //SINGLETON_H
