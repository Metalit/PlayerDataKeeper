#pragma once

#include <array>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace DataKeeper {

// Content-only replacement: emulated Android storage can reject copying source
// permission bits onto an existing destination. Never truncate the last backup.
struct CopyResult {
    const char* operation = nullptr;
    int error = 0;
    bool committed = false;
    explicit operator bool() const { return committed; }
};

struct PosixFiles {
    static int open(const char* path, int flags, mode_t mode = 0) { return ::open(path, flags, mode); }
    static int stat(int fd, struct stat* info) { return ::fstat(fd, info); }
    static ssize_t read(int fd, void* data, size_t size) { return ::read(fd, data, size); }
    static ssize_t write(int fd, const void* data, size_t size) { return ::write(fd, data, size); }
    static int sync(int fd) { return ::fsync(fd); }
    static int close(int fd) { return ::close(fd); }
    static int rename(const char* from, const char* to) { return ::rename(from, to); }
    static int unlink(const char* path) { return ::unlink(path); }
};

template <typename Files = PosixFiles>
CopyResult AtomicCopy(const std::string& source, const std::string& destination, bool requireNonempty = false) {
    struct Resources {
        int source = -1;
        int temporary = -1;
        std::string path;
        ~Resources() {
            if (source >= 0) Files::close(source);
            if (temporary >= 0) Files::close(temporary);
            if (!path.empty()) Files::unlink(path.c_str());
        }
    } resources;
    auto failure = [](const char* operation, int error) { return CopyResult{operation, error, false}; };

    resources.source = Files::open(source.c_str(), O_RDONLY | O_CLOEXEC);
    if (resources.source < 0) return failure("open source", errno);
    struct stat before{};
    if (Files::stat(resources.source, &before)) return failure("stat source", errno);
    if (!S_ISREG(before.st_mode)) return failure("source is not a regular file", EINVAL);
    if (requireNonempty && before.st_size == 0) return failure("source save is empty", EINVAL);

    static std::atomic<unsigned long> sequence{0};
    for (int attempt = 0; attempt < 32; ++attempt) {
        auto candidate = destination + ".datakeeper-" + std::to_string(::getpid()) + "-" +
                         std::to_string(sequence.fetch_add(1, std::memory_order_relaxed)) + ".tmp";
        resources.temporary = Files::open(candidate.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
        if (resources.temporary >= 0) {
            resources.path = std::move(candidate);
            break;
        }
        if (errno != EEXIST) return failure("create temporary", errno);
    }
    if (resources.temporary < 0) return failure("temporary name exhausted", EEXIST);

    std::array<char, 16384> buffer;
    off_t copied = 0;
    for (;;) {
        auto count = Files::read(resources.source, buffer.data(), buffer.size());
        if (count < 0) {
            if (errno == EINTR) continue;
            return failure("read source", errno);
        }
        if (count == 0) break;
        copied += count;
        for (ssize_t written = 0; written < count;) {
            auto result = Files::write(resources.temporary, buffer.data() + written, count - written);
            if (result < 0) {
                if (errno == EINTR) continue;
                return failure("write temporary", errno);
            }
            if (result == 0) return failure("write temporary made no progress", EIO);
            written += result;
        }
    }
    struct stat after{};
    if (Files::stat(resources.source, &after)) return failure("restat source", errno);
    if (copied != before.st_size || after.st_size != before.st_size ||
        after.st_mtim.tv_sec != before.st_mtim.tv_sec || after.st_mtim.tv_nsec != before.st_mtim.tv_nsec ||
        after.st_ctim.tv_sec != before.st_ctim.tv_sec || after.st_ctim.tv_nsec != before.st_ctim.tv_nsec)
        return failure("source changed during copy", EAGAIN);
    while (Files::sync(resources.temporary)) {
        if (errno != EINTR) return failure("sync temporary", errno);
    }
    int temporary = resources.temporary;
    resources.temporary = -1; // close() must not be retried: the descriptor may already be released.
    if (Files::close(temporary)) return failure("close temporary", errno);
    if (Files::rename(resources.path.c_str(), destination.c_str())) return failure("replace destination", errno);
    resources.path.clear();
    return {nullptr, 0, true};
}
}
