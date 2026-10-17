#if defined(__linux__) || defined(__APPLE__)

#include <cerrno>
#include <system_error>
#include <sys/resource.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

namespace {

struct OpenFileLimit {
    OpenFileLimit() {
        rlimit limit{};
        if (getrlimit(RLIMIT_NOFILE, &limit) != 0) throw std::system_error(errno, std::generic_category(), "read open file limit");
#ifdef __APPLE__
        int perProcess = 0;
        std::size_t size = sizeof(perProcess);
        if (sysctlbyname("kern.maxfilesperproc", &perProcess, &size, nullptr, 0) != 0) throw std::system_error(errno, std::generic_category(), "read kern.maxfilesperproc");
        const auto ceiling = static_cast<rlim_t>(perProcess);
        if (limit.rlim_max > ceiling) limit.rlim_max = ceiling;
        if (limit.rlim_cur == limit.rlim_max) return;
#else
        if (limit.rlim_cur >= limit.rlim_max) return;
#endif
        limit.rlim_cur = limit.rlim_max;
        if (setrlimit(RLIMIT_NOFILE, &limit) != 0) throw std::system_error(errno, std::generic_category(), "raise open file limit");
    }
} openFileLimit;

}

#endif
