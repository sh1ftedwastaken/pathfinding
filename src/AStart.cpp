#include <iostream>
#include <vector>
#include <queue>
#include <math.h>
#include <algorithm>

double manhattanDistance(std::vector<int> pos1, std::vector<int> pos2) 
{
    return std::sqrt(
        std::abs(pos1[0] - pos2[0]) + 
        std::abs(pos1[1] - pos2[1]) + 
        std::abs(pos1[2] - pos2[2])
    );
}

std::vector<int> AStart(
    std::vector<int> grid, 
    std::vector<int> start_pos, 
    std::vector<int> end_pos
)
{
    
}