#include "httpserver/router/RouterTrie.h"
#include <cassert>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace mymuduo::http::router;

void RouterTrie::insert(std::string_view path, std::size_t routeId)
{
    assert(!path.empty());
    assert(path[0] == '/');

    Node* cur = &root_;
    auto segments = splitPath(path);

    for (std::size_t i = 0; i < segments.size(); ++i) {
        const std::string_view segment = segments[i];

        if (isParamSegment(segment)) {   // 如果是动态参数(如 /:id)
            std::string paramName(segment.substr(1));

            if (!cur->paramChild) {   // 当前节点不存在动态参数子节点，则添加
                cur->paramName = std::move(paramName);
                cur->paramChild = std::make_unique<Node>();
            }
            else {
                assert(cur->paramName == paramName && "Parameter name conflict in the same segment");
            }

            cur = cur->paramChild.get();
        }
        else if (isWildCardSegment(segment)) {   // 如果是通配符
            // 通配符必须是最后一个路径参数
            assert(i == segments.size() - 1 && "Wildcard must be the last segment of the path");

            if (!cur->wildcardChild) {
                cur->wildcardChild = std::make_unique<Node>();
            }

            cur = cur->wildcardChild.get();
        }
        else {   // 如果是静态路由
            auto it = cur->staticChildren.find(segment);
            if (it == cur->staticChildren.end()) {
                auto [newIt, inserted] =
                    cur->staticChildren.emplace(std::string(segment), std::make_unique<Node>());
                assert(inserted);
                it = newIt;
            }
            cur = it->second.get();
        }
    }

    if (cur->routeId.has_value()) {
        throw std::runtime_error("Route has already been registered");
    }
    cur->routeId = routeId;
}

bool RouterTrie::match(std::string_view path, std::size_t& routeId, ParamList& params) const
{
    params.clear();
    if (path.empty() || path[0] != '/') {
        return false;
    }

    auto segments = splitPath(path);

    return matchRecursive(&root_, segments, 0, routeId, params);
}

std::vector<std::string_view> RouterTrie::splitPath(std::string_view path)
{
    std::vector<std::string_view> segments;
    std::size_t i = 0;

    while (i < path.size()) {
        while (i < path.size() && path[i] == '/') { ++i; }

        if (i >= path.size()) {
            break;
        }

        std::size_t j = i;
        while (j < path.size() && path[j] != '/') { ++j; }

        segments.emplace_back(path.substr(i, j - i));
        i = j;
    }

    return segments;
}

RouterTrie::Node* RouterTrie::findStaticChild(Node* node, std::string_view segment)
{
    return const_cast<RouterTrie::Node*>(findStaticChild(static_cast<const Node*>(node), segment));
}

const RouterTrie::Node* RouterTrie::findStaticChild(const Node* node, std::string_view segment)
{
    auto it = node->staticChildren.find(segment);
    if (it != node->staticChildren.end()) {
        return it->second.get();
    }

    return nullptr;
}

bool RouterTrie::matchRecursive(const Node* node, const std::vector<std::string_view>& segments,
                                std::size_t index, std::size_t& routeId, ParamList& params) const
{
    if (!node) {
        return false;
    }

    if (index == segments.size()) {   // 当前已是最后一个路径参数
        if (node->routeId.has_value()) {
            routeId = node->routeId.value();
            return true;
        }

        // 允许 /static/* 匹配 /static/
        if (node->wildcardChild && node->wildcardChild->routeId.has_value()) {
            params.emplace_back("*", "");
            routeId = node->wildcardChild->routeId.value();
            return true;
        }
        return false;
    }

    std::string_view segment = segments[index];

    // 先匹配静态路径
    if (const auto* child = findStaticChild(node, segment)) {
        if (matchRecursive(child, segments, index + 1, routeId, params)) {
            return true;
        }
    }

    // 再匹配动态参数
    if (node->paramChild) {
        params.emplace_back(node->paramName, std::string(segment));
        if (matchRecursive(node->paramChild.get(), segments, index + 1, routeId, params)) {
            return true;
        }
        params.pop_back();
    }

    // 最后匹配通配符
    if (node->wildcardChild && node->wildcardChild->routeId.has_value()) {
        std::string rest;

        // 把剩余路径参数添加到 rest 中
        for (std::size_t i = index; i < segments.size(); ++i) {
            if (!rest.empty()) {
                rest.push_back('/');
            }
            rest += segments[i];
        }

        params.emplace_back("*", std::move(rest));

        routeId = node->wildcardChild->routeId.value();
        return true;
    }

    return false;
}