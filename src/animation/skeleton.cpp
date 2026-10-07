#include "animation/skeleton.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace mmo::animation {
namespace {

constexpr std::size_t kNoParent = static_cast<std::size_t>(-1);

Matrix4 identity_matrix()
{
    Matrix4 result{};
    result[0] = 1.0f;
    result[5] = 1.0f;
    result[10] = 1.0f;
    result[15] = 1.0f;
    return result;
}

Matrix4 multiply(const Matrix4& left, const Matrix4& right)
{
    Matrix4 result{};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            for (int index = 0; index < 4; ++index) {
                result[column * 4 + row] +=
                    left[index * 4 + row] * right[column * 4 + index];
            }
        }
    }
    return result;
}

float dot(Quaternion left, Quaternion right)
{
    return left.x * right.x + left.y * right.y + left.z * right.z + left.w * right.w;
}

Quaternion normalize(Quaternion value)
{
    const float length_squared = value.x * value.x + value.y * value.y +
        value.z * value.z + value.w * value.w;
    if (length_squared <= 0.000001f) {
        return {};
    }
    const float inverse_length = 1.0f / std::sqrt(length_squared);
    return {value.x * inverse_length, value.y * inverse_length,
        value.z * inverse_length, value.w * inverse_length};
}

Quaternion slerp(Quaternion first, Quaternion second, float factor)
{
    first = normalize(first);
    second = normalize(second);
    float cosine = dot(first, second);
    if (cosine < 0.0f) {
        second = {-second.x, -second.y, -second.z, -second.w};
        cosine = -cosine;
    }

    if (cosine > 0.9995f) {
        return normalize({
            first.x + (second.x - first.x) * factor,
            first.y + (second.y - first.y) * factor,
            first.z + (second.z - first.z) * factor,
            first.w + (second.w - first.w) * factor,
        });
    }

    const float angle = std::acos(std::clamp(cosine, -1.0f, 1.0f));
    const float sine = std::sin(angle);
    const float first_weight = std::sin((1.0f - factor) * angle) / sine;
    const float second_weight = std::sin(factor * angle) / sine;
    return normalize({
        first.x * first_weight + second.x * second_weight,
        first.y * first_weight + second.y * second_weight,
        first.z * first_weight + second.z * second_weight,
        first.w * first_weight + second.w * second_weight,
    });
}

Matrix4 transform_matrix(const Transform& transform)
{
    const Quaternion rotation = normalize(transform.rotation);
    const float xx = rotation.x * rotation.x;
    const float yy = rotation.y * rotation.y;
    const float zz = rotation.z * rotation.z;
    const float xy = rotation.x * rotation.y;
    const float xz = rotation.x * rotation.z;
    const float yz = rotation.y * rotation.z;
    const float wx = rotation.w * rotation.x;
    const float wy = rotation.w * rotation.y;
    const float wz = rotation.w * rotation.z;

    Matrix4 result = identity_matrix();
    result[0] = (1.0f - 2.0f * (yy + zz)) * transform.scale[0];
    result[1] = (2.0f * (xy + wz)) * transform.scale[0];
    result[2] = (2.0f * (xz - wy)) * transform.scale[0];
    result[4] = (2.0f * (xy - wz)) * transform.scale[1];
    result[5] = (1.0f - 2.0f * (xx + zz)) * transform.scale[1];
    result[6] = (2.0f * (yz + wx)) * transform.scale[1];
    result[8] = (2.0f * (xz + wy)) * transform.scale[2];
    result[9] = (2.0f * (yz - wx)) * transform.scale[2];
    result[10] = (1.0f - 2.0f * (xx + yy)) * transform.scale[2];
    result[12] = transform.translation[0];
    result[13] = transform.translation[1];
    result[14] = transform.translation[2];
    return result;
}

template <typename Value>
Value sample_keyframes(
    const std::vector<float>& times,
    const std::vector<Value>& values,
    float time,
    Interpolation interpolation,
    Value fallback)
{
    if (times.empty() || values.empty() || times.size() != values.size()) {
        return fallback;
    }
    if (time <= times.front()) {
        return values.front();
    }
    if (time >= times.back()) {
        return values.back();
    }

    const auto upper = std::upper_bound(times.begin(), times.end(), time);
    const std::size_t next = static_cast<std::size_t>(upper - times.begin());
    const std::size_t previous = next - 1;
    if (interpolation == Interpolation::Step) {
        return values[previous];
    }
    const float interval = times[next] - times[previous];
    const float factor = interval > 0.0f ? (time - times[previous]) / interval : 0.0f;
    if constexpr (std::is_same_v<Value, Quaternion>) {
        return slerp(values[previous], values[next], factor);
    } else {
        Value result{};
        for (std::size_t component = 0; component < result.size(); ++component) {
            result[component] = values[previous][component] +
                (values[next][component] - values[previous][component]) * factor;
        }
        return result;
    }
}

}

std::size_t Skeleton::add_bone(Bone bone)
{
    bone.index = bones.size();
    bones.push_back(std::move(bone));
    return bones.size() - 1;
}

std::optional<std::size_t> Skeleton::find_bone(std::string_view name) const
{
    for (const Bone& bone : bones) {
        if (bone.name == name) {
            return bone.index;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> Skeleton::root_bone_index() const
{
    for (const Bone& bone : bones) {
        if (bone.parent_index == kNoBone) {
            return bone.index;
        }
    }
    return std::nullopt;
}

bool Skeleton::is_valid() const
{
    std::vector<unsigned char> visit_state(bones.size(), 0);
    std::size_t root_count = 0;
    for (std::size_t index = 0; index < bones.size(); ++index) {
        const Bone& bone = bones[index];
        if (bone.index != index || bone.name.empty() ||
            (bone.parent_index != kNoBone &&
                (bone.parent_index >= bones.size() || bone.parent_index == index))) {
            return false;
        }
        if (bone.parent_index == kNoBone) {
            ++root_count;
        }
        for (std::size_t other = index + 1; other < bones.size(); ++other) {
            if (bone.name == bones[other].name) {
                return false;
            }
        }
    }
    if (!bones.empty() && root_count != 1) {
        return false;
    }
    if (bones.empty()) {
        return true;
    }

    const auto visit = [&](auto&& self, std::size_t index) -> bool {
        if (visit_state[index] == 1) {
            return false;
        }
        if (visit_state[index] == 2) {
            return true;
        }
        visit_state[index] = 1;
        const std::size_t parent = bones[index].parent_index;
        if (parent != kNoBone && !self(self, parent)) {
            return false;
        }
        visit_state[index] = 2;
        return true;
    };

    for (std::size_t index = 0; index < bones.size(); ++index) {
        if (!visit(visit, index)) {
            return false;
        }
    }
    return true;
}

bool AnimationClip::is_valid(std::size_t bone_count) const
{
    if (!std::isfinite(duration) || duration < 0.0f) {
        return false;
    }
    const auto valid_times = [this](const auto& keyframes) {
        if (keyframes.times.size() != keyframes.values.size()) {
            return false;
        }
        float previous = -1.0f;
        for (float time : keyframes.times) {
            if (!std::isfinite(time) || time < 0.0f || time < previous ||
                (duration > 0.0f && time > duration)) {
                return false;
            }
            previous = time;
        }
        return true;
    };
    for (const AnimationChannel& channel : channels) {
        if (channel.bone_index >= bone_count ||
            !valid_times(channel.translations) || !valid_times(channel.rotations) ||
            !valid_times(channel.scales)) {
            return false;
        }
        for (const Vector3& value : channel.translations.values) {
            for (float component : value) {
                if (!std::isfinite(component)) return false;
            }
        }
        for (const Vector3& value : channel.scales.values) {
            for (float component : value) {
                if (!std::isfinite(component)) return false;
            }
        }
        for (const Quaternion& value : channel.rotations.values) {
            if (!std::isfinite(value.x) || !std::isfinite(value.y) ||
                !std::isfinite(value.z) || !std::isfinite(value.w)) {
                return false;
            }
        }
    }
    return true;
}

Animator::Animator(const Skeleton& skeleton, std::vector<AnimationClip> clips)
    : skeleton_(skeleton)
    , clips_(std::move(clips))
{
    if (!skeleton_.is_valid()) {
        throw std::invalid_argument("cannot animate an invalid skeleton");
    }
    for (const AnimationClip& clip : clips_) {
        if (!clip.is_valid(skeleton_.bones.size())) {
            throw std::invalid_argument("cannot animate an invalid clip");
        }
    }
    pose_.local_transforms.reserve(skeleton_.bones.size());
    pose_.global_matrices.resize(skeleton_.bones.size(), identity_matrix());
    pose_.skin_matrices.resize(skeleton_.bones.size(), identity_matrix());
    rebuild_pose();
}

void Animator::play(std::size_t clip_index, bool loop)
{
    if (clip_index >= clips_.size()) {
        stop();
        return;
    }
    if (active_clip_ != clip_index) {
        time_seconds_ = 0.0f;
    }
    fade_source_clip_ = kNoBone;
    fade_duration_seconds_ = 0.0f;
    active_clip_ = clip_index;
    loop_ = loop;
    playing_ = true;
    rebuild_pose();
}

bool Animator::cross_fade(std::size_t clip_index, float duration_seconds, bool loop)
{
    if (clip_index >= clips_.size()) {
        return false;
    }
    if (!playing_ || active_clip_ == clip_index || duration_seconds <= 0.0f) {
        play(clip_index, loop);
        return true;
    }

    fade_source_clip_ = active_clip_;
    fade_source_time_seconds_ = time_seconds_;
    fade_elapsed_seconds_ = 0.0f;
    fade_duration_seconds_ = duration_seconds;
    active_clip_ = clip_index;
    time_seconds_ = 0.0f;
    loop_ = loop;
    playing_ = true;
    rebuild_pose();
    return true;
}

void Animator::stop()
{
    playing_ = false;
    time_seconds_ = 0.0f;
    fade_source_clip_ = kNoBone;
    fade_duration_seconds_ = 0.0f;
    rebuild_pose();
}

void Animator::update(float delta_seconds)
{
    if (!playing_ || clips_.empty() || active_clip_ >= clips_.size()) {
        return;
    }

    const float elapsed = std::max(delta_seconds, 0.0f);
    advance_time(elapsed, clips_[active_clip_].duration, loop_, time_seconds_);
    if (!loop_ && clips_[active_clip_].duration > 0.0f &&
        time_seconds_ >= clips_[active_clip_].duration) {
        playing_ = false;
    }
    if (fade_source_clip_ != kNoBone) {
        advance_time(elapsed, clips_[fade_source_clip_].duration, true, fade_source_time_seconds_);
        fade_elapsed_seconds_ += elapsed;
        if (fade_elapsed_seconds_ >= fade_duration_seconds_) {
            fade_source_clip_ = kNoBone;
            fade_duration_seconds_ = 0.0f;
        }
    }
    rebuild_pose();
}

const Pose& Animator::pose() const
{
    return pose_;
}

bool Animator::playing() const
{
    return playing_;
}

std::size_t Animator::active_clip() const
{
    return active_clip_;
}

std::size_t Animator::clip_count() const
{
    return clips_.size();
}

std::optional<std::size_t> Animator::find_clip(std::string_view name) const
{
    for (std::size_t index = 0; index < clips_.size(); ++index) {
        if (clips_[index].name == name) {
            return index;
        }
    }
    return std::nullopt;
}

void Animator::advance_time(
    float delta_seconds,
    float duration,
    bool loop,
    float& time_seconds) const
{
    time_seconds += delta_seconds;
    if (duration > 0.0f) {
        if (loop) {
            time_seconds = std::fmod(time_seconds, duration);
        } else {
            time_seconds = std::min(time_seconds, duration);
        }
    }
}

void Animator::rebuild_pose()
{
    pose_.local_transforms.clear();
    pose_.local_transforms.reserve(skeleton_.bones.size());
    for (const Bone& bone : skeleton_.bones) {
        pose_.local_transforms.push_back(bone.bind_local_transform);
    }

    if (playing_ && active_clip_ < clips_.size()) {
        sample_clip(clips_[active_clip_], time_seconds_, pose_.local_transforms);
    }
    if (fade_source_clip_ != kNoBone && fade_source_clip_ < clips_.size()) {
        fade_source_pose_.clear();
        for (const Bone& bone : skeleton_.bones) {
            fade_source_pose_.push_back(bone.bind_local_transform);
        }
        sample_clip(clips_[fade_source_clip_], fade_source_time_seconds_, fade_source_pose_);
        const float blend = fade_duration_seconds_ > 0.0f
            ? std::clamp(fade_elapsed_seconds_ / fade_duration_seconds_, 0.0f, 1.0f)
            : 1.0f;
        for (std::size_t index = 0; index < pose_.local_transforms.size(); ++index) {
            Transform& target = pose_.local_transforms[index];
            const Transform& source = fade_source_pose_[index];
            for (std::size_t component = 0; component < target.translation.size(); ++component) {
                target.translation[component] = source.translation[component] +
                    (target.translation[component] - source.translation[component]) * blend;
                target.scale[component] = source.scale[component] +
                    (target.scale[component] - source.scale[component]) * blend;
            }
            target.rotation = slerp(source.rotation, target.rotation, blend);
        }
    }

    resolve_pose(pose_);
}

const AnimationClip& Animator::clip(std::size_t clip_index) const
{
    return clips_.at(clip_index);
}

void Animator::sample_clip_pose(
    std::size_t clip_index,
    float time_seconds,
    std::vector<BoneTransform>& transforms) const
{
    transforms.clear();
    transforms.reserve(skeleton_.bones.size());
    for (const Bone& bone : skeleton_.bones) {
        transforms.push_back(bone.bind_local_transform);
    }
    sample_clip(clips_.at(clip_index), time_seconds, transforms);
}

void Animator::resolve_pose(Pose& pose) const
{
    pose.global_matrices.resize(skeleton_.bones.size(), identity_matrix());
    pose.skin_matrices.resize(skeleton_.bones.size(), identity_matrix());
    std::vector<bool> resolved(skeleton_.bones.size(), false);
    const auto resolve = [&](auto&& self, std::size_t joint_index) -> void {
        if (resolved[joint_index]) {
            return;
        }
        const std::size_t parent = skeleton_.bones[joint_index].parent_index;
        if (parent != kNoParent && parent < skeleton_.bones.size()) {
            self(self, parent);
            pose.global_matrices[joint_index] = multiply(
                pose.global_matrices[parent],
                transform_matrix(pose.local_transforms[joint_index]));
        } else {
            pose.global_matrices[joint_index] =
                transform_matrix(pose.local_transforms[joint_index]);
        }
        resolved[joint_index] = true;
    };

    for (std::size_t bone = 0; bone < skeleton_.bones.size(); ++bone) {
        resolve(resolve, bone);
        pose.skin_matrices[bone] = multiply(
            pose.global_matrices[bone], skeleton_.bones[bone].inverse_bind_matrix);
    }
}

void Animator::sample_clip(
    const AnimationClip& clip,
    float time_seconds,
    std::vector<BoneTransform>& transforms) const
{
    for (const AnimationChannel& channel : clip.channels) {
        if (channel.bone_index >= transforms.size()) {
            continue;
        }
        Transform& transform = transforms[channel.bone_index];
        transform.translation = sample_keyframes(
            channel.translations.times, channel.translations.values,
            time_seconds, channel.translations.interpolation, transform.translation);
        transform.rotation = sample_keyframes(
            channel.rotations.times, channel.rotations.values,
            time_seconds, channel.rotations.interpolation, transform.rotation);
        transform.scale = sample_keyframes(
            channel.scales.times, channel.scales.values,
            time_seconds, channel.scales.interpolation, transform.scale);
    }
}

}
