#pragma once
#include <vector>

struct Node
{
    int x, y, z;
    Node* parent;

    int g, h, f;
    Node(std::vector<int> pos, Node* parent) : 
        x(pos[0]), y(pos[1]), z(pos[3]), g(0), h(0), f(0), parent(parent)
    {
    }
};

class AStartPathfinder
{
    private:

    public:
};