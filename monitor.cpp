#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <optional>

class EventsMonitor
{
private:
    struct EventData
    {
        int id;
        std::string data;
    };
    
    std::mutex mtx;
    std::condition_variable cv;
    std::optional<EventData> current_event; // через отсутствие данных в пакете определяем отсутствие события. Не знаю, хорошо ли так делать, но пока что так.
    bool producer_finished = false;

public:
    void producer(int events_count)
    {
        for (int i = 0; i < events_count; i++)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::lock_guard<std::mutex> lock(mtx);
            current_event = EventData{i, "Event with ID: " + std::to_string(i)};
            std::cout << "Producer has sent event with ID = " << i << "." << std::endl; 
            cv.notify_one();
        }
        std::lock_guard<std::mutex> lock(mtx);
        producer_finished = true;
        cv.notify_one();
        std::cout << "Producer has finished producing, " << events_count << " events are generated." << std::endl;
    }

    void consumer()
    {
        int processed_count = 0;
        while (true)
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this]()
            { 
                return current_event.has_value() || producer_finished; 
            });

            if (current_event.has_value())
            {
                EventData event = current_event.value();
                current_event.reset();
                lock.unlock();
                std::cout << "Consumer had received event with ID = " << event.id << "." << std::endl;
                std::cout << "Data processing: " << event.data << std::endl;
                processed_count++;
                continue;
            }
            if (producer_finished)
            {
                std::cout << "Consumer has processed " << processed_count << " events and received an exit signal from producer." << std::endl;
                break;
            }
        }
    }
    
    void run(int events_count)
    {
        std::cout << "Starting events monitor. " << events_count << " events are expected to be generated.\n" << std::endl;
        std::thread producer_thread(&EventsMonitor::producer, this, events_count);
        std::thread consumer_thread(&EventsMonitor::consumer, this);
        producer_thread.join();
        consumer_thread.join();
        std::cout << std::endl;
        std::cout << "Done." << std::endl;
    }
};

int main()
{
    EventsMonitor monitor;
    int events_count = 0;
    std::cout << "Enter the amount of events for test." << std::endl;
    std::cin >> events_count;
    monitor.run(events_count);
    return(0);
}