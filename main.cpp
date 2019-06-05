
#include "main.hpp"

#include <chrono>
#include <iostream>

#include "console.hpp"

namespace zodiactest {

void QueueEvents::on_start() {
    Writer::wakeAll();
}

void QueueEvents::on_stop() noexcept {
    Writer::wakeAll();
}

void QueueEvents::on_hwm() {
    logConsole("***Queue high watermark reached!\n");
    Writer::suspendAll();
}

void QueueEvents::on_lwm() {
    logConsole("***Queue low watermark reached!\n");
    Writer::wakeAll();
}

Main::Main(size_t rnum, size_t wnum)
    : _mqueue_sp(std::make_shared<MessageQueue<std::string>>(10, 0, 10)) {
    _mqueue_sp->setEvents(std::make_shared<QueueEvents>());

    for(size_t i = 0; i != rnum; i++)
        _readers.emplace_back(Reader("Reader" + std::to_string(i),
                                     _mqueue_sp));

    for(size_t i = 0; i != wnum; i++)
        _writers.emplace_back(Writer(static_cast<int>(i), /* increasing priority */
                                     "Writer" + std::to_string(i),
                                     _mqueue_sp));
}

void Main::main() {
    _mqueue_sp->run();
    for(auto & reader : _readers)
        reader.run();

    for(auto & writer : _writers)
        writer.run();
}

void Main::stop() noexcept
{
    _mqueue_sp->stop();
    /* join all threads */
    _readers.clear();
    _writers.clear();
}

void Main::flush()
{
    auto queueFlush = Reader("LastReader", _mqueue_sp);

    /* notifiers not needed */
    _mqueue_sp->setEvents(nullptr);

    std::clog << "Let's flush queue\n";
    
    _mqueue_sp->run(); // runnable state again
    queueFlush.run();
    
    /* hope this is enough for cleanup */
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    _mqueue_sp->stop();

    std::clog << ("Writers wrote " +
                  std::to_string(Writer::gmsg_num) +
                  " messages\n");
    std::clog << ("Readers handled " +
                  std::to_string(Reader::gmsg_num) +
                  " messages\n");
}


Main::~Main()
{
    stop();
}


} // namespace zodiactest

int main(int argc, char ** argv)
{
    zodiactest::Main app(1/*readers*/, 2/*writers*/);

    std::clog << "Press enter to start\n";
    std::cin.get();
    
    app.main();

    std::this_thread::sleep_for(
        std::chrono::milliseconds(100));

    app.stop();
    app.flush();
    
    /* destructors do stop & cleanup */
    return 0;
}
