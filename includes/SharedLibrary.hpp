/**
 * @file SharedLibrary.hpp
 *
 * @addtogroup modulemanager
 * @{
 */

#ifndef SHARED_LIBRARY_HPP
#define SHARED_LIBRARY_HPP

#include <stdexcept>
#include <string>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif


/**
 * @class SharedLibrary
 * @brief Encapsule le handle d'une bibliotheque dynamique. Rien de plus.
 */
class SharedLibrary {
    public:
        #if defined(_WIN32)
            using Handle = HMODULE;
        #else
            using Handle = void *;
        #endif

        static const char *extension() {
            #if defined(_WIN32)
                return ".dll";
            #elif defined(__APPLE__)
                return ".dylib";
            #else
                return ".so";
            #endif
        }

        explicit SharedLibrary(std::string path) : _path(std::move(path)) {
            #if defined(_WIN32)
                _handle = ::LoadLibraryA(_path.c_str());
            #else
                _handle = ::dlopen(_path.c_str(), RTLD_NOW | RTLD_LOCAL);
            #endif
            if (!_handle)
                throw std::runtime_error("SharedLibrary: " + _path);
        }

        ~SharedLibrary() {
            #if defined(_WIN32)
                if (_handle) ::FreeLibrary(_handle);
            #else
                if (_handle) ::dlclose(_handle);
            #endif
        }

        SharedLibrary(const SharedLibrary &) = delete;
        SharedLibrary &operator=(const SharedLibrary &) = delete;

        template <typename T>
        T symbol(const char *name) const {
            #if defined(_WIN32)
                return reinterpret_cast<T>(::GetProcAddress(_handle, name));
            #else
                return reinterpret_cast<T>(::dlsym(_handle, name));
            #endif
        }

        const std::string &path() const { return _path; }
        Handle handle() const { return _handle; }

    private:
        Handle _handle = nullptr;
        std::string _path;
};

/** @} */

#endif /* !SHARED_LIBRARY_HPP */
