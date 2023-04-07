//
// Created by 稻草人 on 2021/8/5.
//

#ifndef MULTI_SHM_QUEUE_SPMC_H
#define MULTI_SHM_QUEUE_SPMC_H

#include "spdlog/spdlog.h"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


inline bool cpu_set_affinity(int cpu_id) {
#if defined __linux__
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu_id, &mask);
    return 0 == sched_setaffinity(0, sizeof(mask), &mask);
#else
    return false;
#endif
}


namespace disruptor {
    // 共享内存写入和读取的浮标
    struct Bookmark {
        size_t item_num;//存入结构体数量
        size_t page_num;// 使用page数量
        size_t cursor;  //浮标，已写入位置
                        //        size_t next;    //浮标，下次写入位置
    };

    class Page {
    public:
        static constexpr int KB = 1024;
        static constexpr int MB = KB * KB;
        static constexpr int GB = KB * MB;
        static constexpr int page_size = 1024 * MB;

        static bool RemoveFile(const std::string &path) {
            if (remove(path.c_str()) == -1) {
                SPDLOG_ERROR("Failed to remove: {}, errno: {}", path, strerror(errno));
                return false;
            }
            SPDLOG_DEBUG("Remove: {}.", path);
            return true;
        }

    private:
        std::string file_path_;
        bool write_mode_;
        void *data_ = nullptr;

    public:
        Page(const std::string &file_path, const bool &write_mode) {
            file_path_ = file_path;
            write_mode_ = write_mode;
            SPDLOG_DEBUG("Page, path:{}, mode:{}.", file_path_, write_mode_);
        }
        ~Page() {
            SPDLOG_DEBUG("~Page, path:{}, mode:{}.", file_path_, write_mode_);
        }

        bool GetShm() {
            int fd = write_mode_ ? open(file_path_.c_str(), O_RDWR | O_CREAT, 0666) : open(file_path_.c_str(), O_RDONLY);
            if (fd == -1) {
                SPDLOG_ERROR("Open error: {}", strerror(errno));
                return false;
            } else {
                SPDLOG_DEBUG("Open, fd: {}", fd);
            }

            // 获取文件大小
            struct stat st {};
            if (fstat(fd, &st) == -1) {
                SPDLOG_ERROR("Fstat error: {}", strerror(errno));
                close(fd);
                return false;
            }

            // 改变文件大小
            if (st.st_size == 0) {
                if (ftruncate(fd, (int64_t) page_size) == 0) {
                    SPDLOG_DEBUG("Ftruncate, file size:{}", page_size);
                } else {
                    SPDLOG_ERROR("Failed to ftruncate {}, size:{}, error:{}", file_path_, page_size, strerror(errno));
                    return false;
                }
            } else {
                SPDLOG_DEBUG("File exit,  path:{}, size:{}.", file_path_, st.st_size);
                if (st.st_size != page_size) {
                    SPDLOG_ERROR("File exit,  path:{}, size:{}.", file_path_, st.st_size);
                    return false;
                }
            }

            if (write_mode_) {
                data_ = mmap(nullptr, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            } else {
                data_ = mmap(nullptr, page_size, PROT_READ, MAP_PRIVATE, fd, 0);
            }

            if (data_ == MAP_FAILED) {
                SPDLOG_ERROR("Failed to mmap: {}, size: {}, errno: {}", file_path_, page_size, strerror(errno));
                close(fd);
                return false;
            } else {
                close(fd);
                return true;
            }
        }

        bool DetachShm() {
            if (msync(data_, page_size, MS_SYNC) != 0) {
                SPDLOG_ERROR("Failed to msync: {}, size: {}, errno: {}", file_path_, page_size, strerror(errno));
                return false;
            }

            if (munmap(data_, page_size) == -1) {
                SPDLOG_ERROR("Failed to munmap: {}, size: {}, errno: {}", file_path_, page_size, strerror(errno));
                return false;
            }
            data_ = nullptr;
            return true;
        }

        bool RemoveShm() {
            return RemoveFile(file_path_);
        }

        void *GetShmDataAddress() { return data_; }

        auto GetFilePath() { return file_path_; }
    };


    class WaitStrategy {
    private:
        Bookmark *bookmark_{};

    public:
        explicit WaitStrategy(Bookmark *ptr) {
            bookmark_ = ptr;
        };

        ~WaitStrategy() = default;

        size_t Wait(const size_t &idx) {
            int nCounter = 100;
            while (true) {
                const size_t current_cursor = bookmark_->cursor;
                if (idx < current_cursor) {
                    return current_cursor;
                } else {
                    //spins --> yield
                    if (nCounter == 0) {
                        std::this_thread::yield();
                    } else {
                        nCounter--;
                    }
                    continue;
                }
            }//while
        }
    };

    template<typename T>
    class Notebook {
    private:
        size_t capacity_{};           //有多少item
        std::vector<T *> content_;    //目录，指向所有item的内存地址
        Bookmark *bookmark_ = nullptr;//书签
        WaitStrategy *wait_ = nullptr;

        bool SetCapacity(const size_t &capacity) {
            try {
                content_.reserve(capacity + 1024);
            } catch (const std::length_error &e) {
                SPDLOG_ERROR("SetCapacity Error: {}.", e.what());
                return false;
            }
            capacity_ = capacity;
            return true;
        }

    public:
        Notebook() = default;
        ~Notebook() = default;

        bool Init(const std::string &folder_path, const size_t &item_num, const bool &writer, const bool &init, const int &cpu_id = 1) {
            // cpu亲和力
            if (cpu_id > 0) {
                if (cpu_set_affinity(cpu_id)) {
                    SPDLOG_INFO("Set cpu_id {} successfully.", cpu_id);
                } else {
                    SPDLOG_INFO("Set cpu_id {} failed.", cpu_id);
                }
            }

            if (!SetCapacity(item_num)) {
                return false;
            }

            const size_t item_size = sizeof(T);                              //结构体大小
            const size_t item_num_in_mark = sizeof(Bookmark) / item_size + 1;//书签相当于多少个结构体
            const size_t total_item_num = item_num + item_num_in_mark;       //书签和结构体加总相当于多少个item结构体的空间占用
            const size_t item_num_in_page = Page::page_size / item_size - 1; //一页能装下多少item
            const size_t page_num = total_item_num / item_num_in_page + 1;   //需要多少page才能全部装下
            size_t idx = 0;                                                  //目录索引，对应读取写入游标
            SPDLOG_DEBUG("params.");
            SPDLOG_DEBUG("item_size:{}", item_size);
            SPDLOG_DEBUG("item_num:{}", item_num);
            SPDLOG_DEBUG("mark_size:{}", sizeof(Bookmark));
            SPDLOG_DEBUG("item_num_in_mark:{}", item_num_in_mark);
            SPDLOG_DEBUG("item_num_in_page:{}", item_num_in_page);
            SPDLOG_DEBUG("page_num:{}", page_num);
            SPDLOG_DEBUG("page_size:{}", Page::page_size);
            SPDLOG_DEBUG("total_item_num:{}", total_item_num);
            SPDLOG_DEBUG("item_num_in_all_page:{}", item_num_in_page * page_num);

            // 创建第一个Page，书签Bookmark也在这个页面上面
            {
                std::string file_path = folder_path + "_page_0.store";
                auto page = Page(file_path, writer);
                if (!page.GetShm()) {
                    return false;
                }

                // 书签
                bookmark_ = (Bookmark *) page.GetShmDataAddress();
                wait_ = new WaitStrategy(bookmark_);
                if (init) {
                    bookmark_->item_num = item_num;
                    bookmark_->page_num = page_num;
                    bookmark_->cursor = 0;
                    //                    bookmark_->next = -1;
                }

                // 只使用一个页面
                if (page_num == 1) {
                    for (auto i = 0; i < item_num; i++) {
                        content_[i] = (T *) ((char *) page.GetShmDataAddress() + sizeof(Bookmark) + sizeof(T) * i);
                    }
                    return true;
                }

                //使用多个页面
                for (auto i = 0; i < item_num_in_page - item_num_in_mark; i++) {
                    content_[idx++] = (T *) ((char *) page.GetShmDataAddress() + sizeof(Bookmark) + sizeof(T) * i);
                }
            }

            //Pages
            for (auto p = 1; p < page_num; p++) {
                std::string file_path = folder_path + "_page_" + std::to_string(p) + ".store";
                auto page = Page(file_path, writer);
                if (!page.GetShm()) {
                    return false;
                }

                auto left_item_num =
                        total_item_num - (item_num_in_page - item_num_in_mark) - item_num_in_page * (p - 1);
                if (left_item_num < item_num_in_page) {
                    for (auto i = 0; i < left_item_num; i++) {
                        content_[idx++] = (T *) ((char *) page.GetShmDataAddress() + sizeof(T) * i);
                    }
                } else {
                    for (auto i = 0; i < item_num_in_page; i++) {
                        content_[idx++] = (T *) ((char *) page.GetShmDataAddress() + sizeof(T) * i);
                    }
                }
            }
            return true;
        }

        //        size_t ClaimIndex() {
        //            return bookmark_->next++ + 1;
        //        };

        void SetData(const T &data) {
            constexpr size_t item_size = sizeof(T);
            memcpy(content_[bookmark_->cursor], &data, item_size);
            bookmark_->cursor++;
        }

        T *OpenData() {
            return content_[bookmark_->cursor];
        }

        void Commit() {
            bookmark_->cursor++;
        };

        //consumer
        size_t WaitFor(const size_t &idx) {
            const size_t current_cursor = bookmark_->cursor;
            if (idx < current_cursor) {
                return current_cursor;
            } else {
                return wait_->Wait(idx);
            }
        }

        T *GetData(const size_t &idx) {
            return content_[idx];
        }
    };
}// namespace disruptor

#endif//MULTI_SHM_QUEUE_SPMC_H
