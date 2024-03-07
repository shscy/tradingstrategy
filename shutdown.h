////
//// Created by zuoxiaoliang on 2024/3/2.
////
//
//#ifndef TRADINGSTRATEGY_SHUTDOWN_H
//#define TRADINGSTRATEGY_SHUTDOWN_H
//#include <atomic>
//#include <mutex>
//
//
//class Shutdown {
//    std::atomic<int> count_{0};
//    std::mutex mux_;
//    std::condition_variable cv_;
//public:
//    void Add(int n) {
//        count_ += n ;
//    }
//    void Done() {
//        count_--;
//        if(count_.load() == 0) {
//            cv_.notify_all();
//        }else if (count_.load() <0 ){
//            std::abort();
//        }
//    }
//    void Wait(){
//        if(count_.load() == 0) {
//            return;
//        }
//        std::unique_lock<std::mutex> lock{mux_};
//        cv_.wait(lock, [&](){
//            assert(this->count_.load() == 0);
//            return this->count_.load() == 0 ;
//        });
//
//    }
//};
//
//
//
//#endif //TRADINGSTRATEGY_SHUTDOWN_H
