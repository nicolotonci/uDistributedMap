#include <ff/ff.hpp>
#include <network.hpp>
#include <iterator>
#include <vector>

/*
    this param means how many chunk at staurtup the scheduler send to each worker in case of dynamic scheduling
    Its purpose is to minimize idle time of the workers, as shown in detail in the report.
*/
#define PREASSIGNSIZE 1 



template<typename InputIterator, typename OutputIterator, typename Env = void>
class DMapMaster : public ff::ff_pipeline{
private:
    typedef typename std::iterator_traits<InputIterator>::value_type Tin;
    typedef typename  std::iterator_traits<OutputIterator>::value_type Tout;

    // private class implementing the scheduler
    struct scheduler : public ff_node_t<Dtask<Tout>, Dtask<Tin>> {
        bool boot = true;
        InputIterator begin_in, end_in;    
        OutputIterator begin_out;
        std::map<int,int> pCount;


        scheduler(InputIterator  _begin_in, 
                  InputIterator _end_in, 
                  OutputIterator _begin_out, 
                  size_t _workers,
                  size_t _chunk_size //chunk_size > 0 => dynamic scheduling 
                  ) : begin_in(_begin_in), end_in(_end_in), begin_out(_begin_out), processedItems(0), workers(_workers), chunk_size(_chunk_size), nextItemToSend(0) { 
                      this->total_distance = std::distance(_begin_in, _end_in);

                        for(size_t i = 0; i < workers; i++)
                            pCount[i] = 0;
                  }

        Dtask<Tin>* svc(Dtask<Tout>* in){
            // this if branch is executed just once, in particular during startup to fill workers with tasks
            if (boot){
                this->Tstart = getusec(); // start taking time
                boot = false; delete in; 
                bool isStaticScheduling = (chunk_size == 0);

                // if static scheduling were selected, compute the chunk size based on the number of workers
                size_t chunk = isStaticScheduling ? ((total_distance + workers - 1) / workers) : chunk_size; // fast ceiling positive numbers

                // Fill up all the workers, sent multiple chunk at sturtup if PREASSIGNSIZE is greater than 1 and we are using dynamic policy
                for (int i = 0 ; i < (isStaticScheduling ? 1 : PREASSIGNSIZE); i++){
                    for (size_t w = 0; w < workers; w++){
                        size_t start = i*chunk*workers + w*chunk; 
                        if (start > total_distance)
                            break;
                        size_t end = (start+chunk > total_distance) ? total_distance : start+chunk;
                        this->ff_send_out(new Dtask<Tin>(w, start, end, std::next(begin_in, start), std::next(begin_in, end)));
                    }
                    // takes note to the next item that need to be sent - Usefull only for dynamic scheduling
                    nextItemToSend = (i+1)*workers*chunk >= total_distance ? total_distance : (i+1)*workers*chunk;
                }
                
                return this->GO_ON;
            }
            
            #ifdef VERBOSE
                std::cout << "Received a result" << std::endl;
            #endif
            
            // count a new task completed for the specific worker, debug purposes only
            pCount[in->id_worker]++;

            // write back the results
            auto outIterator = std::next(begin_out, in->begin_i);
            for(auto obj : in->data)
                *outIterator++ = obj;
            
            // update the number of already processed elements
            processedItems += (in->end_i - in->begin_i);

            if (chunk_size && (nextItemToSend < total_distance)){ // there is more to process (i.e dynamic scheduling)
                size_t end = nextItemToSend+chunk_size > total_distance ? total_distance : nextItemToSend+chunk_size;
                // send the new task to the same worker from which i received the result
                this->ff_send_out(new Dtask<Tin>(in->id_worker, nextItemToSend, end, std::next(begin_in, nextItemToSend), std::next(begin_in, end)));
                nextItemToSend = end;
            }

            delete in;
            
            // if i'm received the lest result 
            if (processedItems == total_distance){
                std::cout << "Elapsed time: " << (getusec()-(this->Tstart))/1000 << " ms" << std::endl;

                // print the number of tasks received each worker - Debug purposes only - Can be wrappein in a #ifdef VERBOSE #endif
                for (auto [worker, partitions] : pCount)
                    std::cout << "Worker #" << worker << " received " << partitions << "partitions" << std::endl;

                // the computation is over, send the End of stream to all the workers
                return this->EOS;   
            }

            return this->GO_ON;
        }

        private:
            size_t processedItems;
            size_t workers, chunk_size, nextItemToSend;
            size_t total_distance;
            size_t Tstart;      
    };

public:
    DMapMaster(std::string master_addr, std::vector<std::string> worker_addresses, InputIterator begin_in, InputIterator end_in, OutputIterator begin_out, Env* e = nullptr, size_t chunk_size = 0 ) {
        // create the stages for the Master pipeline
        this->add_stage(new receiver<Tout>(master_addr, worker_addresses.size(), true), true);
        this->add_stage(new scheduler(begin_in, end_in, begin_out, worker_addresses.size(), chunk_size), true);
        this->add_stage(new sender<Tin, Env>(0, worker_addresses, e), true);
    }
};