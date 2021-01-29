#include <DMap.hpp>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <numeric>

#define INPUT_SIZE 100000
#define EXECUTION_TIME 100
#define THREADS 40
#define CHUNK_SIZE 128   // 0 => static sxcheduling, dynamic scheduling otherwise

float active_delay(int msecs) {
  // read current time
  float x = 1.25f;
  auto start = std::chrono::high_resolution_clock::now();
  auto end   = false;
  while(!end) {
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    x *= sin(x) / atan(x) * tanh(x) * sqrt(x);
    if(msec>=msecs)
      end = true;
  }
  return x;
}

int main(int argc, char*argv[]){
    DMap::Exec exec(argc, argv);
    std::vector<int> input;
    std::vector<int> output;
    //MyEnv* env = nullptr;
    if (exec.isMaster){
        // master execution => populate input
        

       input = std::vector<int>(INPUT_SIZE, EXECUTION_TIME);
       output = std::vector<int>(INPUT_SIZE);


    }

        // note the abolute primitive in lambda function and a scaling of 1000 which results in items of computation time limited to 50ms
    if (DMap::map(exec, [](int& i){active_delay(i); return i;}, input.begin(), input.end(), output.begin(), CHUNK_SIZE, (void*) nullptr, THREADS) < 0){
        std::cout << "ERROR" << std::endl;
        return 1;
    }

    // if i'm here it means that the execution is fine and i'm the master so i can consume the results

    /*for (auto i : output)
        std::cout << i << std::endl;
*/
    return 0;
}