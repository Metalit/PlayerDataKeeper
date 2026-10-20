#include "atomic-copy.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace fs = std::filesystem;
static std::string fault;
static int temporaryFD = -1;
static int sourceFD = -1;
static int statCalls = 0;
static bool interrupted = false;
static bool partial = false;
struct Injected : DataKeeper::PosixFiles {
    static int open(const char* path, int flags, mode_t mode = 0) {
        bool temp = flags & O_CREAT;
        if ((fault == "open" && !temp) || (fault == "create" && temp)) { errno = EACCES; return -1; }
        int fd = PosixFiles::open(path, flags, mode);
        if (temp) temporaryFD = fd; else sourceFD = fd;
        return fd;
    }
    static int stat(int fd, struct stat* info) {
        ++statCalls;
        if (fault == "stat") { errno = EIO; return -1; }
        int result = PosixFiles::stat(fd, info);
        if (fault == "changed" && statCalls == 2) ++info->st_mtim.tv_nsec;
        return result;
    }
    static ssize_t read(int fd, void* data, size_t length) {
        if (fault == "read") { errno = EIO; return -1; }
        if (fault == "read-eintr" && !interrupted) { interrupted = true; errno = EINTR; return -1; }
        return PosixFiles::read(fd, data, length);
    }
    static ssize_t write(int fd, const void* data, size_t length) {
        if (fault == "write") { errno = ENOSPC; return -1; }
        if (fault == "write-zero") return 0;
        if (fault == "write-eintr" && !interrupted) { interrupted = true; errno = EINTR; return -1; }
        if (fault == "write-partial" && length > 3) { partial = true; length = 3; }
        return PosixFiles::write(fd, data, length);
    }
    static int sync(int fd) {
        if (fault == "sync") { errno = EIO; return -1; }
        if (fault == "sync-eintr" && !interrupted) { interrupted = true; errno = EINTR; return -1; }
        return PosixFiles::sync(fd);
    }
    static int close(int fd) {
        int result = PosixFiles::close(fd);
        if (fault == "close" && fd == temporaryFD) { errno = EIO; return -1; }
        return result;
    }
    static int rename(const char* from, const char* to) {
        if (fault == "rename") { errno = EPERM; return -1; }
        return PosixFiles::rename(from, to);
    }
};
static std::string Read(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}
static void Write(const fs::path& path, const std::string& value) {
    std::ofstream output(path, std::ios::binary); output << value;
}
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    auto root = fs::path(argv[1]);
    fs::create_directories(root);
    auto source = root / "source";
    auto destination = root / "destination";
    const std::string old = "old valid backup";
    const std::string desired = std::string(37000, 'x') + "new save";
    int cases = 0;
    for (const char* fail : {"open", "create", "stat", "read", "write", "write-zero", "changed", "sync", "close", "rename"}) {
        for (bool existed : {false, true}) {
            fs::remove(destination);
            if (existed) Write(destination, old);
            Write(source, desired);
            fault = fail; statCalls = 0; temporaryFD = -1;
            auto result = DataKeeper::AtomicCopy<Injected>(source, destination, true);
            assert(!result && result.error != 0 && result.operation);
            assert(fs::exists(destination) == existed);
            if (existed) assert(Read(destination) == old);
            assert(Read(source) == desired);
            for (auto& entry : fs::directory_iterator(root))
                assert(entry.path().filename().string().find(".datakeeper-") == std::string::npos);
            ++cases;
        }
    }
    for (const char* success : {"", "read-eintr", "write-eintr", "write-partial", "sync-eintr"}) {
        fault = success; statCalls = 0; interrupted = false; partial = false;
        Write(source, desired); Write(destination, old);
        int oldFD = ::open(destination.c_str(), O_RDONLY);
        auto result = DataKeeper::AtomicCopy<Injected>(source, destination, true);
        assert(result && !result.error && !result.operation);
        assert(Read(destination) == desired && Read(source) == desired);
        std::array<char, 64> previous{};
        assert(::read(oldFD, previous.data(), previous.size()) == static_cast<ssize_t>(old.size()));
        assert(std::string(previous.data(), old.size()) == old); // inode was never truncated.
        ::close(oldFD);
        if (fault == "write-partial") assert(partial);
        ++cases;
    }
    fault.clear(); statCalls = 0;
    Write(source, ""); Write(destination, old);
    assert(!DataKeeper::AtomicCopy<Injected>(source, destination, true));
    assert(Read(destination) == old);
    assert(DataKeeper::AtomicCopy<Injected>(source, destination, false));
    assert(Read(destination).empty()); cases += 2;
    assert(!DataKeeper::AtomicCopy<Injected>(root, destination)); ++cases;
    // Negative control for the previous destructive copy boundary: a data transfer
    // rejected after truncation (e.g. sendfile EPERM) has already lost the backup.
    Write(destination, old);
    int truncated = ::open(destination.c_str(), O_WRONLY | O_TRUNC);
    assert(truncated >= 0); ::close(truncated);
    assert(Read(destination).empty());
    std::cout << "PASS " << cases << " production atomic-copy cases; destructive-truncate negative control reproduced\n";
}
