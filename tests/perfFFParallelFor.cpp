#include <ff/ff.hpp>
#include <ff/parallel_for.hpp>
#include <iostream>
#include <string>
#include <numeric>
#include <chrono>

void active_delay(int msecs) {
  // read current time
  float x = 1.25f;
  auto start = std::chrono::high_resolution_clock::now();
  while(true) {
    x *= sin(x) / atan(x) * tanh(x) * sqrt(x);
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    if(msec>=msecs)
      return;
  }
  return;
}


int main(int argc, char*argv[]){
    size_t tasks = std::stoul(argv[1]), threads = std::stoul(argv[2]), delay = std::stoul(argv[3]);

    std::cout << "Tasks: " << tasks << " - Threads: " << threads << " - Delay: " << delay << "ms" << std::endl;

    std::vector<int> input(tasks);
    std::vector<int> output(tasks);

    std::iota(input.begin(), input.end(), 0);

    ff::ParallelFor pf(threads);

    auto start = ff::getusec();

    pf.parallel_for(0, tasks, [&](const long i)  {
                                active_delay(delay);
                                output[i] = input[i] + 10;
                        }, threads);


    std::cout << "ParFF - Elapsed time: " << (ff::getusec()-start)/1000 << " ms" << std::endl;

    /* SEQUENTIAL EXECUTION
    start = ff::getusec();

    for(size_t i = 0; i < tasks; i++){
        active_delay(delay);
        output[i] = input[i] - 10;
    }

    std::cout << "Seq - Elapsed time: " << (ff::getusec()-start)/1000 << " ms" << std::endl;
    

    // SINGLE ELEMENT EXECUTION
    start = ff::getusec();
    active_delay(delay);
    std::cout << "Single Delay - Elapsed time: " << (ff::getusec()-start)/1000 << " ms" << std::endl;

    */
    return 0;
}