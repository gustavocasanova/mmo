#pragma once

#include <array>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mmo::animation {

using Matrix4 = std::array<float, 16>;
using Vector3 = std::array<float, 3>;
inline constexpr std::size_t kNoBone = std::numeric_limits<std::size_t>::max();
inline constexpr std::size_t kMaxSkinningBones = 66;

struct Quaternion {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

struct Transform {
    Vector3 translation{0.0f, 0.0f, 0.0f};
    Quaternion rotation{};
    Vector3 scale{1.0f, 1.0f, 1.0f};
};

using BoneTransform = Transform;

enum class Interpolation {
    Linear,
    Step,
};

struct Bone {
    std::string name;
    std::size_t index = kNoBone;
    std::size_t parent_index = kNoBone;
    BoneTransform bind_local_transform{};
    Matrix4 inverse_bind_matrix{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
};

struct Skeleton {
    std::vector<Bone> bones;

    [[nodiscard]] std::size_t add_bone(Bone bone);
    [[nodiscard]] std::optional<std::size_t> find_bone(std::string_view name) const;
    [[nodiscard]] std::optional<std::size_t> root_bone_index() const;
    [[nodiscard]] bool is_valid() const;
};

struct Pose {
    std::vector<BoneTransform> local_transforms;
    std::vector<Matrix4> global_matrices;
    std::vector<Matrix4> skin_matrices;
};

struct Vector3Keyframes {
    std::vector<float> times;
    std::vector<Vector3> values;
    Interpolation interpolation = Interpolation::Linear;
};

struct QuaternionKeyframes {
    std::vector<float> times;
    std::vector<Quaternion> values;
    Interpolation interpolation = Interpolation::Linear;
};

struct AnimationChannel {
    std::size_t bone_index = 0;
    Vector3Keyframes translations;
    QuaternionKeyframes rotations;
    Vector3Keyframes scales;
};

struct AnimationClip {
    std::string name;
    float duration = 0.0f;
    std::vector<AnimationChannel> channels;

    [[nodiscard]] bool is_valid(std::size_t bone_count) const;
};

class Animator {
public:
    Animator(const Skeleton& skeleton, std::vector<AnimationClip> clips = {});

    void play(std::size_t clip_index, bool loop = true);
    // `keep_phase` starts the new clip at the same normalized time (for same-gait locomotion).
    [[nodiscard]] bool cross_fade(std::size_t clip_index, float duration_seconds,
        bool loop = true, bool keep_phase = false);
    void stop();
    void update(float delta_seconds);

    [[nodiscard]] const Pose& pose() const;
    [[nodiscard]] bool playing() const;
    [[nodiscard]] std::size_t active_clip() const;
    [[nodiscard]] std::size_t clip_count() const;
    [[nodiscard]] std::optional<std::size_t> find_clip(std::string_view name) const;
    [[nodiscard]] const AnimationClip& clip(std::size_t clip_index) const;
    [[nodiscard]] const Skeleton& skeleton() const;
    // Layer-mixing building blocks: sample a clip over the bind pose and rebuild matrices.
    void sample_clip_pose(std::size_t clip_index, float time_seconds,
        std::vector<BoneTransform>& transforms) const;
    void resolve_pose(Pose& pose) const;

private:
    void rebuild_pose();
    void sample_clip(const AnimationClip& clip, float time_seconds,
        std::vector<BoneTransform>& transforms) const;
    void advance_time(float delta_seconds, float duration, bool loop, float& time_seconds) const;

    const Skeleton& skeleton_;
    std::vector<AnimationClip> clips_;
    Pose pose_;
    std::vector<BoneTransform> fade_source_pose_;
    std::size_t fade_source_clip_ = kNoBone;
    std::size_t active_clip_ = 0;
    float time_seconds_ = 0.0f;
    float fade_source_time_seconds_ = 0.0f;
    float fade_elapsed_seconds_ = 0.0f;
    float fade_duration_seconds_ = 0.0f;
    bool playing_ = false;
    bool loop_ = true;
};

}
