#include <DMap.hpp>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <numeric>

#define THREADS 1
#define CHUNK_SIZE 0   // 0 => static sxcheduling, dynamic scheduling otherwise


int main(int argc, char*argv[]){
    DMap::Exec exec(argc, argv);
    std::vector<std::array<int,3>> matrix_input;
    std::vector<int> vector_output;
    std::array<int, 3> * v; // this represent the environment 

    if (exec.isMaster){
        // master execution => populate input

       matrix_input = std::vector<std::array<int, 3>>(2);
       matrix_input[0] = {1, -1, 2};
       matrix_input[1] = {0, -3, 1};
       v = new std::array<int,3>({2, 1, 0});
       vector_output = std::vector<int>(matrix_input.size());
    }

        // note the abolute primitive in lambda function and a scaling of 1000 which results in items of computation time limited to 50ms
    if (DMap::map(exec,
                 [](std::array<int, 3>& a, std::array<int, 3>* vect) -> int { 
                     int result = 0;
                     for (int i = 0; i < 3; i++)
                        result += a[i] * (*vect)[i];
                     return result;
                     }, 
                 matrix_input.begin(), 
                 matrix_input.end(), 
                 vector_output.begin(), 
                 CHUNK_SIZE, 
                 v, 
                 THREADS) < 0){
        std::cout << "ERROR" << std::endl;
        return 1;
    }

    // if i'm here it means that the execution is fine and i'm the master so i can consume the results
    
    // print the result vector 
    for(auto i : vector_output)
        std::cout << i << std::endl;

    /*With the specified input parameters the results should be:
    |1 |
    |-3| 
    */


    return 0;
}