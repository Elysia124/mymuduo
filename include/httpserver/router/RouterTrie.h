#pragma once

#include "httpserver/router/PathParams.h"
#include "httpserver/utils/StringHash.h"
#include "net/noncopyable.h"
#include <cassert>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
namespace mymuduo::http::router {
class RouterTrie : noncopyable
{
public:
    using ParamList = PathParams::ParamList;

    void insert(std::string_view path, std::size_t routeId);

    // 匹配请求路径，例如：
    // /user/123
    //
    // 匹配成功：
    // routeId 返回对应路由编号
    // params 返回路径参数
    bool match(std::string_view path, std::size_t& routeId, ParamList& params) const;

private:
    struct Node
    {
        using StaticChildren =
            std::unordered_map<std::string, std::unique_ptr<Node>, TransparentStringHash, std::equal_to<>>;
        // 静态路径节点

        StaticChildren staticChildren;

        // 动态参数节点
        std::string paramName;   // 参数名，如:id
        std::unique_ptr<Node> paramChild;

        // 通配符节点(*)
        std::unique_ptr<Node> wildcardChild;

        // 当前节点是否对应一条完整路由
        std::optional<std::size_t> routeId;
    };

    static std::vector<std::string_view> splitPath(std::string_view path);

    // 是否是动态参数
    static bool isParamSegment(std::string_view segment) { return segment.size() > 1 && segment[0] == ':'; }

    // 是否是通配符
    static bool isWildCardSegment(std::string_view segment) { return segment == "*"; }

    static Node* findStaticChild(Node* node, std::string_view segment);
    static const Node* findStaticChild(const Node* node, std::string_view segment);

    bool matchRecursive(const Node* node, const std::vector<std::string_view>& segments, std::size_t index,
                        std::size_t& routeId, ParamList& params) const;

    Node root_;
};
}   // namespace mymuduo::http::router