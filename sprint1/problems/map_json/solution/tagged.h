#pragma once
#include <compare>
#include <utility>

namespace util {

template <typename Value, typename Tag>
class Tagged {
public:
    using ValueType = Value;
    using TagType = Tag;

    explicit Tagged(Value v) : value_(std::move(v)) {}

    const Value& operator*() const noexcept { return value_; }
    Value& operator*() noexcept { return value_; }

    const Value* operator->() const noexcept { return &value_; }
    Value* operator->() noexcept { return &value_; }

    auto operator<=>(const Tagged&) const = default;

private:
    Value value_;
};

template <typename TaggedValue>
struct TaggedHash {
    std::size_t operator()(const TaggedValue& value) const noexcept {
        return std::hash<typename TaggedValue::ValueType>{}(*value);
    }
};

}  // namespace util
