#pragma once
#include "VoxelGrid.hpp"
#include "PathResult.hpp"

const PathResult& getLastAStarResult();

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