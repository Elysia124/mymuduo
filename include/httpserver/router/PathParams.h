#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace mymuduo::http::router {
class PathParams
{
public:
    using ParamList = std::vector<std::pair<std::string, std::string>>;
    PathParams() = default;
    explicit PathParams(ParamList params) : params_(std::move(params)) {}

    std::optional<std::string_view> get(std::string_view key) const
    {
        for (const auto& [paramKey, paramValue] : params_) {
            if (paramKey == key) {
                return paramValue;
            }
        }

        return std::nullopt;
    }

    bool empty() const { return params_.empty(); }
    const ParamList& all() const { return params_; }

private:
    ParamList params_;
};
}   // namespace mymuduo::http::router