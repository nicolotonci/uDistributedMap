#include <ff/ff.hpp>
#include <ff/parallel_for.hpp>
#include <type_traits>
#include <functional>
#include <iostream>
#include <network.hpp>

template<typename Tin, typename Tout, typename Env = void>
class DMapWorker : public ff::ff_pipeline{
private:
    struct worker : public ff::ff_node_t<Dtask<Tin>, Dtask<Tout>> {
        std::function<Tout(Tin&, Env*)> transformer;
        ff::ParallelFor pf;
        Env* env;
        int threads; // number of thread to be used in the parallel for
        worker(std::function<Tout(Tin&, Env*)> transform_, int wth) : transformer(transform_), pf(wth), threads(wth) {
            if constexpr (!std::is_void<Env>::value)
                env = new Env;
        }

        Dtask<Tout>* svc(Dtask<Tin>* in){

            // create the container for the results, copying some metadata from the received task
            Dtask<Tout>* out = new Dtask<Tout>(*in);


            /* This is used to simulate unbalanced workers
            if (in->id_worker < 4)
                threads = 4;
                */
            
            
            this->pf.parallel_for(0, (in->end_i - in->begin_i),    // start, stop indexes
                       [&](const long i)  {
                                out->data[i] = transformer(in->data[i], (this->env));
                        }, threads);

            delete in;
            return out;
        }
    };

    void construct(){
        // create the pipeline from the already created stages
        this->add_stage(this->r, true);
        this->add_stage(this->w, true);
        this->add_stage(this->s, true);
    }
    
    worker* w;
    receiver<Tin, Env>* r;
    sender<Tout>* s;

public:

    /*
        This constructor is invoked when a function that takes also the environment is used
    */
    DMapWorker(Tout(*transform_)(Tin&, Env*), std::string listen_addr, std::string master_addr, int wth = FF_AUTO){
        // create the worker
        this->w = new worker(transform_, wth);
        this->r = new receiver<Tin, Env>(listen_addr, 1, false, &(this->w->env));
        this->s = new sender<Tout>(0, master_addr);
        construct();
    }

    /*
        This constructor is invoked when a function that do NOT take also the environment is used

        The function is wrapped on another function discarding an environmet pointer (i.e. the function is decorated)
    */
    DMapWorker(Tout(*transform_)(Tin&), std::string listen_addr,  std::string master_addr, int wth = FF_AUTO){
        this->w = new worker(([transform_](Tin& in, void*) -> Tout {return transform_(in);}), wth);
        this->r = new receiver<Tin, Env>(listen_addr, 1);
        this->s = new sender<Tout>(0, master_addr);
        construct();
    }
};
