
#include "main.hpp"

#include <chrono>
#include <iostream>

void QueueEvents::on_start()
{
    Writer::wakeAll();
}

void QueueEvents::on_stop()
{
    Writer::wakeAll();
}

void QueueEvents::on_hwm()
{
    std::clog << "***Queue high watermark reached!\n";
    Writer::suspendAll();
}

void QueueEvents::on_lwm()
{
    std::clog << "***Queue low watermark reached!\n";
    Writer::wakeAll();
}

Main::Main(size_t rnum, size_t wnum) :
    _mqueueSP(std::make_shared<MessageQueue<std::string>>(10, 0, 10))
{
    _mqueueSP->set_events(std::make_shared<QueueEvents>());

    for(size_t i = 0; i != rnum; i++)
        _readers.emplace_back(Reader("Reader" + std::to_string(i),
                                     _mqueueSP));

    for(size_t i = 0; i != wnum; i++)
        _writers.emplace_back(Writer(static_cast<int>(i), /* increasing priority */
                                     "Writer" + std::to_string(i),
                                     _mqueueSP));
}

void Main::main()
{
    _mqueueSP->run();
    for(auto & reader : _readers)
        reader.run();

    for(auto & writer : _writers)
        writer.run();
}

void Main::stop()
{
    _mqueueSP->stop();

    _readers.clear();
    _writers.clear();
    
    {
        auto queueFlush = Reader("LastReader", _mqueueSP);
        /* notifiers not needed */
        _mqueueSP->set_events(nullptr);

        std::clog << "Let's flush queue\n";
        _mqueueSP->run(); // runnable state again
        queueFlush.run();
        /* hope this is enough */
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        _mqueueSP->stop();
    }

    std::clog << ("Writers wrote " +
                  std::to_string(Writer::gmsgNum) +
                  " messages\n");
    std::clog << ("Readers handled " +
                  std::to_string(Reader::gmsgNum) +
                  " messages\n");
}

Main::~Main()
{
    stop();
}

int main(int argc, char ** argv)
{
    Main app(1/*readers*/, 2/*writers*/);

    std::clog << "Press enter to start\n";
    std::cin.get();
    
    app.main();

    std::this_thread::sleep_for(
        std::chrono::milliseconds(100));

    /* destructors do stop & cleanup */
    return 0;
}
