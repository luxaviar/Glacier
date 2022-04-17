#include <string>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "logger.h"
#include "common/log.h"
#include "common/clock.h"

namespace glacier {

using namespace std;

void ConsoleLogger::Append(int8_t level, ByteStream& stream) {
    stream << '\0';
    OutputDebugStringA(static_cast<const char*>(stream.rbegin()));
}

void ConsoleLogger::Write(ByteStream& stream) {
    stream << '\0';
    OutputDebugStringA(static_cast<const char*>(stream.rbegin()));
}

FileLogger::FileLogger(const char* file) :
    file_path_(file)
{
    Open();
    if (file_ == nullptr) {
        fprintf(stderr, "can't open log file under %s\n", file_path_.c_str());
        exit(-1);
    }
}

FileLogger::~FileLogger() {
    Close();
}

void FileLogger::Open() {
    Close();

    fopen_s(&file_, file_path_.c_str(), "a");
    assert(file_ != nullptr);

#ifdef __linux__
    setbuffer(file_, buffer_, 64 * 1024);

#ifndef NDEBUG
    if (dup_std_err_) {
        int fd = fileno(file_);
        dup2(fd, STDERR_FILENO);
    }
#endif

#endif
}

void FileLogger::Flush() {
    if (file_ != nullptr)
        fflush(file_);
}

void FileLogger::Close() {
    if (file_ != nullptr) {
        fclose(file_);
        file_ = nullptr;
    }
}

void FileLogger::Append(int8_t level, ByteStream& stream) {
    fwrite(stream.rbegin(), stream.ReadableBytes(), 1, file_);

    if (level > Logging::L_WARNING) {
        Flush();
    }
}

void FileLogger::Write(ByteStream& stream) {
    fwrite(stream.rbegin(), stream.ReadableBytes(), 1, file_);
}

}

