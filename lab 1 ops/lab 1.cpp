#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <memory>
#include <locale.h>

using namespace std;

class Monitor {
private:
    mutex mtx;
    condition_variable cv;
    bool event_ready = false;
    shared_ptr<string> event_data;

public:
   
    void provide() {
        for (int i = 1; i <= 5; ++i) { 
      
            this_thread::sleep_for(chrono::seconds(1));

            {
                lock_guard<mutex> lock(mtx);  

                event_data = make_shared<string>("Event data " + to_string(i));
                event_ready = true;
                cout << "Поставщик: отправил событие '" << *event_data << "'" << endl;
                cv.notify_one();
            } 

          
         
        }
    }

   
    void consume() {
        for (int i = 1; i <= 5; ++i) {
            unique_lock<mutex> lock(mtx);

        
            cv.wait(lock, [this]() { return event_ready; });

           
            cout << "Потребитель: получил событие '" << *event_data << "'" << endl;
            event_ready = false;

           
        }
    }
};

int main() {
    setlocale(LC_ALL, "Russian");

    Monitor monitor;


    thread producer_thread(&Monitor::provide, &monitor);
    thread consumer_thread(&Monitor::consume, &monitor);


    producer_thread.join();
    consumer_thread.join();

    return 0;
}