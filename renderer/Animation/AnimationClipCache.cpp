#include "Animation/AnimationClipCache.h"
#include "Animation/AnimationClip.h"
#include "Common/ByteStream.h"
#include "Common/Log.h"
#include "Common/ScopedFile.h"
#include "Common/Util.h"

namespace glacier {

namespace {

//one key of a curve: the time, the value and the interpolation of the segment
constexpr size_t kVec3KeySize = sizeof(float) * 4 + 1;
constexpr size_t kQuatKeySize = sizeof(float) * 5 + 1;

//guarded reads: a truncated cache has to fail quietly instead of making
//ByteStream::Read log an error for every field that is missing
template<typename T>
bool ReadValue(ByteStream& stream, T& value) {
    if (stream.ReadableBytes() < sizeof(T)) {
        return false;
    }

    stream >> value;
    return true;
}

bool ReadCount(ByteStream& stream, uint32_t& count) {
    if (stream.ReadableBytes() == 0) {
        return false;
    }

    stream.ReadVint(count);
    return true;
}

bool ReadString(ByteStream& stream, std::string& value) {
    return stream.Read(value) > 0;
}

bool ReadEvents(ByteStream& stream, std::vector<AnimationClip::Event>& events) {
    uint32_t count = 0;
    if (!ReadCount(stream, count) || count > stream.ReadableBytes() / (sizeof(float) + 1)) {
        return false;
    }

    events.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        AnimationClip::Event event;
        if (!ReadValue(stream, event.time) || !ReadString(stream, event.name)) {
            return false;
        }

        events.push_back(std::move(event));
    }

    return true;
}

bool ReadVec3Keys(ByteStream& stream, std::vector<Vec3Keyframe>& keys) {
    uint32_t count = 0;
    if (!ReadCount(stream, count) || count > stream.ReadableBytes() / kVec3KeySize) {
        return false;
    }

    keys.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        Vec3Keyframe key;
        uint8_t interpolation = 0;
        if (!ReadValue(stream, key.time) || !ReadValue(stream, key.value) || !ReadValue(stream, interpolation)) {
            return false;
        }

        key.interpolation = (AnimationInterpolation)interpolation;
        keys.push_back(key);
    }

    return true;
}

bool ReadQuatKeys(ByteStream& stream, std::vector<QuatKeyframe>& keys) {
    uint32_t count = 0;
    if (!ReadCount(stream, count) || count > stream.ReadableBytes() / kQuatKeySize) {
        return false;
    }

    keys.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        QuatKeyframe key;
        uint8_t interpolation = 0;
        if (!ReadValue(stream, key.time) || !ReadValue(stream, key.value) || !ReadValue(stream, interpolation)) {
            return false;
        }

        key.interpolation = (AnimationInterpolation)interpolation;
        keys.push_back(key);
    }

    return true;
}

void WriteVec3Keys(ByteStream& stream, const std::vector<Vec3Keyframe>& keys) {
    stream.WriteVint((uint32_t)keys.size());

    for (const auto& key : keys) {
        stream << key.time << key.value << (uint8_t)key.interpolation;
    }
}

void WriteEvents(ByteStream& stream, const std::vector<AnimationClip::Event>& events) {
    stream.WriteVint((uint32_t)events.size());

    for (const auto& event : events) {
        stream << event.time << event.name;
    }
}

void WriteQuatKeys(ByteStream& stream, const std::vector<QuatKeyframe>& keys) {
    stream.WriteVint((uint32_t)keys.size());

    for (const auto& key : keys) {
        stream << key.time << key.value << (uint8_t)key.interpolation;
    }
}

}

std::filesystem::path AnimationClipCache::PathFor(const std::filesystem::path& source) {
    std::filesystem::path path = source;
    path += L".gclip";
    return path;
}

bool AnimationClipCache::Load(const std::filesystem::path& source,
    std::vector<std::shared_ptr<AnimationClip>>& clips)
{
    ScopedFile file(PathFor(source).string().c_str(), "rb");
    if (!file) {
        return false;
    }

    ByteStream stream = file.Drain();

    constexpr size_t kHeaderSize = sizeof(uint32_t) * 2 + sizeof(uint64_t) * 2;
    if (stream.ReadableBytes() < kHeaderSize) {
        LOG_DEBUG("animation cache for '{}' is truncated, importing again", source.string());
        return false;
    }

    uint32_t magic = 0;
    uint32_t version = 0;
    uint64_t source_size = 0;
    int64_t source_time = 0;
    stream >> magic >> version >> source_size >> source_time;

    if (magic != kMagic || version != kVersion) {
        LOG_DEBUG("animation cache for '{}' was written by another version, importing again", source.string());
        return false;
    }

    const FileStamp stamp = StampOfFile(source);
    if (source_size != stamp.size || source_time != stamp.time) {
        LOG_DEBUG("animation cache for '{}' is stale, importing again", source.string());
        return false;
    }

    uint32_t clip_count = 0;
    if (!ReadCount(stream, clip_count)) {
        return false;
    }

    clips.clear();
    clips.reserve(clip_count);

    for (uint32_t c = 0; c < clip_count; ++c) {
        std::string clip_name;
        std::vector<AnimationClip::Event> events;
        if (!ReadString(stream, clip_name) || !ReadEvents(stream, events)) {
            clips.clear();
            return false;
        }

        auto clip = std::make_shared<AnimationClip>(clip_name.c_str());
        for (const auto& event : events) {
            clip->AddEvent(event.time, event.name.c_str());
        }

        uint32_t track_count = 0;
        if (!ReadCount(stream, track_count)) {
            clips.clear();
            return false;
        }

        for (uint32_t t = 0; t < track_count; ++t) {
            std::string node_name;
            if (!ReadString(stream, node_name)) {
                clips.clear();
                return false;
            }

            std::vector<Vec3Keyframe> positions;
            std::vector<QuatKeyframe> rotations;
            std::vector<Vec3Keyframe> scales;
            if (!ReadVec3Keys(stream, positions) || !ReadQuatKeys(stream, rotations) || !ReadVec3Keys(stream, scales)) {
                clips.clear();
                return false;
            }

            auto& track = clip->AddTrack(node_name.c_str());
            for (const auto& key : positions) track.AddPosition(key);
            for (const auto& key : rotations) track.AddRotation(key);
            for (const auto& key : scales) track.AddScale(key);
        }

        clips.push_back(clip);
    }

    LOG_DEBUG("animation cache for '{}': {} clip(s)", source.string(), clip_count);
    return true;
}

void AnimationClipCache::Save(const std::filesystem::path& source,
    const std::vector<std::shared_ptr<AnimationClip>>& clips)
{
    const FileStamp stamp = StampOfFile(source);

    ByteStream stream;
    stream << (uint32_t)kMagic << (uint32_t)kVersion << stamp.size << stamp.time;
    stream.WriteVint((uint32_t)clips.size());

    for (const auto& clip : clips) {
        if (!clip) {
            continue;
        }

        stream << clip->name();
        WriteEvents(stream, clip->events());
        stream.WriteVint((uint32_t)clip->track_count());

        for (const auto& track : clip->tracks()) {
            stream << track.node_name();
            WriteVec3Keys(stream, track.positions());
            WriteQuatKeys(stream, track.rotations());
            WriteVec3Keys(stream, track.scales());
        }
    }

    ScopedFile file(PathFor(source).string().c_str(), "wb");
    if (!file) {
        LOG_WARN("could not write the animation cache of '{}'", source.string());
        return;
    }

    //rbegin() is the first unread byte, data() would start at the prepend area
    fwrite(stream.rbegin(), stream.ReadableBytes(), 1, file);
    LOG_DEBUG("animation cache for '{}': {} clip(s), {} bytes", source.string(), clips.size(), stream.ReadableBytes());
}

}
