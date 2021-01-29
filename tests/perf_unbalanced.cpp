#include <DMap.hpp>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <numeric>

#define INPUT_SIZE 100000
#define THREADS 40
#define CHUNK_SIZE 128   // 0 => static sxcheduling, dynamic scheduling otherwise

void active_delay(int msecs) {
  // read current time
  auto start = std::chrono::high_resolution_clock::now();
  auto end   = false;
  while(!end) {
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    if(msec>=msecs)
      end = true;
  }
  return;
}

int main(int argc, char*argv[]){
    DMap::Exec exec(argc, argv);
    std::vector<int> input;
    std::vector<int> output;
    //MyEnv* env = nullptr;
    if (exec.isMaster){
        // master execution => populate input
        

       input = std::vector<int>(INPUT_SIZE);
       output = std::vector<int>(INPUT_SIZE);

       size_t half = INPUT_SIZE / 2;
        
        // Sequence structure:
        // -half -half+1 -half+2 ..... -1 0 1 ...... half-2 half-1 half
       std::iota(input.begin(), input.end(), -half);
    }

        // note the abolute primitive in lambda function and a scaling of 1000 which results in items of computation time limited to 50ms
    if (DMap::map(exec, [](int& i){active_delay(abs(i)/100); return i;}, input.begin(), input.end(), output.begin(), CHUNK_SIZE, (void*) nullptr, THREADS) < 0){
        std::cout << "ERROR" << std::endl;
        return 1;
    }

    // if i'm here it means that the execution is fine and i'm the master so i can consume the results


    /*for (auto i : output)
        std::cout << i << std::endl;
*/
    return 0;
}