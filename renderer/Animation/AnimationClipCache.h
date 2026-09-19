#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

namespace glacier {

class AnimationClip;
class Skeleton;

//Binary cache of the clips imported from one file: writing them once keeps the
//next start from converting the animation channels of the source again. The
//cache is derived data next to the source ("<file>.gclip"), it is never
//committed and can be deleted at any time.
//
//A cache only matches when the source file and the format version are the same;
//the animation data does not depend on anything else, and the clip to bone
//bindings are resolved from the bones of the model as usual, so a cached clip
//behaves exactly like an imported one.
//
//The file is a plain ByteStream (little endian, the floats are written as they
//are in memory): header, then a clip count and the clips, each with its events,
//a track count, the track's node name and the keys of its three channels. Counts
//and string lengths are variable length integers.
class AnimationClipCache {
public:
    //"<source>.gclip"
    static std::filesystem::path PathFor(const std::filesystem::path& source);

    //Loads the clips of the source; returns false when the cache is missing,
    //stale or unreadable, in which case the caller imports the file
    static bool Load(const std::filesystem::path& source,
        std::vector<std::shared_ptr<AnimationClip>>& clips);

    //Stores the clips imported from the source
    static void Save(const std::filesystem::path& source,
        const std::vector<std::shared_ptr<AnimationClip>>& clips);

private:
    static constexpr uint32_t kMagic = 0x43414C47; //"GLAC"
    //3: the clips carry their animation events
    static constexpr uint32_t kVersion = 3;
};

}
