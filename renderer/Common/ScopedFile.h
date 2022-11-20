#pragma once

#include <assert.h>
#include <stdio.h>
#include "Uncopyable.h"
#include "Bytestream.h"

namespace glacier {

struct ScopedFile : private Uncopyable {
    ScopedFile(const char* path, const char* mode) {
#ifdef _MSC_VER
        errno_t err = fopen_s(&fp_, path, mode);
#else
        fp_ = fopen(path, mode);
#endif
    }

    ~ScopedFile() {
        if (fp_) {
            fclose(fp_);
        }
    }

    ByteStream Drain() {
        fseek(fp_, 0, SEEK_END);
        size_t  length = ftell(fp_);
        fseek(fp_, 0, SEEK_SET);

        ByteStream stream(length);
        
        fread(stream.wbegin(), 1, length, fp_);
        stream.Fill(length);

        return stream;
    }


    operator bool() const {
        return fp_ != nullptr;
    }

    operator FILE* () const {
        return fp_;
    }

private:
    FILE* fp_ = nullptr;
};

}
