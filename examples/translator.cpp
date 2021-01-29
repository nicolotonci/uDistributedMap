#include <DMap.hpp>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <numeric>

#define THREADS 1
#define CHUNK_SIZE 100   // 0 => static sxcheduling, dynamic scheduling otherwise


int main(int argc, char*argv[]){
    DMap::Exec exec(argc, argv);
    std::string input;
    std::string output;
    //MyEnv* env = nullptr;
    if (exec.isMaster){
        // master execution => populate input

        std::ifstream f("examples/input_text.txt"); //taking file as inputstream

       if(f) {
            std::ostringstream ss;
            ss << f.rdbuf(); // reading data
            input = ss.str();
       }


       output = std::string(input.size(), 0);
    }

        // note the abolute primitive in lambda function and a scaling of 1000 which results in items of computation time limited to 50ms
    if (DMap::map(exec,
                 [](char& c) -> char { return  toupper(c);}, 
                 input.begin(), 
                 input.end(), 
                 output.begin(), 
                 CHUNK_SIZE, 
                 (void*) nullptr, 
                 THREADS) < 0){
        std::cout << "ERROR" << std::endl;
        return 1;
    }

    // if i'm here it means that the execution is fine and i'm the master so i can consume the results

    std::ofstream out("examples/output_text.txt");
    out << output;
    out.close();
    return 0;
}