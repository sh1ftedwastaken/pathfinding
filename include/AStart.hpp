#pragma once
#include <cstddef> 
#include <functional>
#include <vector>

#include "VoxelGrid.hpp"

struct coordHasher
{
    std::size_t operator()(const coord& arr) const noexcept 
    {
        std::size_t h = 0;
        for (int val : arr) {
            h ^= std::hash<int>{}(val) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }
};

struct Node
{
    coord position;
    Node* parent;
    double g, h, f;
    
    Node(coord pos, Node* parent = nullptr) : 
        position(pos), parent(parent), g(0), h(0), f(0)
    {
    }
};

struct CompareNodes {
    bool operator()(const Node* lhs, const Node* rhs) const {
        // check greater than for min heap behavior
        return lhs->f > rhs->f;
    }
};