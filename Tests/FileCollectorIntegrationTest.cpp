#include <gtest/gtest.h>
#include "FileCollector.hpp"

TEST(ThreadPool_Unit, SingleTask) {
    std::atomic<int> value{ 0 };
    {
        ThreadPool pool(1);
        pool.EnqueueTask([&] { value = 42; });
    }
    EXPECT_EQ(value.load(), 42);
}


TEST(ThreadPool_Unit, MultipleTasks) {
    std::atomic<int> counter{ 0 };
    const int tasksCount = 100;
    {
        ThreadPool pool;
        for (int i = 0; i < tasksCount; ++i) {
            pool.EnqueueTask([&] { ++counter; });
        }
    }
    EXPECT_EQ(counter.load(), tasksCount);
}


TEST(FileCollector_Integration, ThreadPoolCollectsFile) {
    FileCollector collector;
    uint32_t fileId = 1;
    size_t fileSize = 5000000000;
    collector.CollectFile(fileId, fileSize);

    {
        ThreadPool pool;
        const size_t chunkSize = 2000; // chunkSize должен быть кратен fileSize, так как если чанк выходит за границу он будет отклонен и тест не пройдет!!!!
        for (size_t i = 0; i < fileSize; i += chunkSize) {
            pool.EnqueueTask([&, i] {
                std::vector<uint8_t> chunk(chunkSize);
                for (size_t j = 0; j < chunkSize; ++j)
                    chunk[j] = static_cast<uint8_t>(i + j);
                collector.OnNewChunk(fileId, i, std::move(chunk));
                });
        }      
    } // ThreadPool.~ThredPool()

    // теперь можно получить полностью собранный файл,  
    auto opt = collector.GetFile(fileId);
    ASSERT_TRUE(opt);
    
    ASSERT_EQ(opt->size(), fileSize);
    for (size_t i = 0; i < fileSize; ++i) {
        EXPECT_EQ((*opt)[i], static_cast<uint8_t>(i));
    }
}
