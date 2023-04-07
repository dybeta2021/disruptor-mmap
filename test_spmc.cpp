#include "logger.h"
#include "dirruptor/spmc.h"
#include <iostream>

typedef struct {
    char data[128];
    size_t th;
} TestBufferData;


int main() {
    bool init_log = ots::utils::create_logger("test.log", "trace", false, false, false);
    // page
    {
        auto page = disruptor::Page("test.store", true);
        page.GetShm();
        auto file_path = page.GetFilePath();
        auto address = page.GetShmDataAddress();
        SPDLOG_INFO("file_path:{}", file_path);
        SPDLOG_INFO("address:{}", address);
        page.DetachShm();
        page.RemoveShm();
    }

    // set data
    {
        auto notebook = disruptor::Notebook<TestBufferData>();
        notebook.Init("test", 1024 * 1024 * 128, true, true);
        SPDLOG_INFO("start.");
        for (auto i = 0; i < 1024 * 1024 * 1; i++) {
            auto tmp = notebook.OpenData();
            tmp->th = i;
            notebook.Commit();
        }

        for (auto i = 1024 * 1024; i < 1024 * 1024 * 2; i++) {
            TestBufferData t{};
            t.th = i;
            notebook.SetData(t);
        }
        SPDLOG_INFO("end.");
    }

    // get data
    {
        auto notebook = disruptor::Notebook<TestBufferData>();
        notebook.Init("test", 1024 * 1024 * 1, false, false);
        SPDLOG_INFO("start.");
        for (auto i = 0; i < 1024 * 1024 * 1; i++) {
            notebook.WaitFor(i);
            auto ret = notebook.GetData(i);
            std::cout << ret->th << std::endl;
        }
        SPDLOG_INFO("end.");
    }

    return 0;
}
