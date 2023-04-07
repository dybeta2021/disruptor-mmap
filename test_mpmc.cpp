#include "logger.h"
#include "dirruptor/mpmc.h"
#include <iostream>

typedef struct {
    char data[128];
    size_t th;
} TestBufferData;


int main() {
    bool init_log = ots::utils::create_logger("test.log", "trace", false, false, false);
    // page
    {
        auto page = atomic_disruptor::Page("atomic_test.store", true);

        page.GetShm();
        auto file_path = page.GetFilePath();
        auto address = page.GetShmDataAddress();
        SPDLOG_INFO("file_path:{}", file_path);
        SPDLOG_INFO("address:{}", address);
        page.DetachShm();
        page.RemoveShm();
    }

    {
        auto notebook = atomic_disruptor::Notebook<TestBufferData>();
        notebook.Init("atomic_test", 1024 * 1024 * 16, true, true);
        SPDLOG_INFO("start.");
        for (auto i = 0; i < 1024 * 1024 * 16; i++) {
            auto tmp = notebook.OpenData(i);
            tmp->th = i;
            notebook.Commit(i);
        }
        SPDLOG_INFO("end.");
    }

    {
        auto notebook = atomic_disruptor::Notebook<TestBufferData>();
        notebook.Init("atomic_test", 1024 * 1024 * 16, false, false);
        SPDLOG_INFO("start.");
        for (auto i = 0; i < 1024 * 1024 * 16; i++) {
            notebook.WaitFor(i);
            auto ret = notebook.GetData(i);
            std::cout << ret->th << std::endl;
        }
        SPDLOG_INFO("end.");
    }
    return 0;
}
