#pragma once

#include <iostream>
#include <vector>

#include <map>
#include <unordered_map>

#include <mutex>
#include <shared_mutex>

#include <optional>

#include <queue>
#include <functional>

// для удобных тестов многопоточности
class ThreadPool 
{
public:
    explicit ThreadPool();
    explicit ThreadPool(uint16_t numThreads);
    ~ThreadPool();
    void EnqueueTask(std::function<void()>&& task);

private:
    uint16_t numThreads;

    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queueMtx;
    std::condition_variable condition;
    
    std::atomic<bool> stopFlag;
    
    void workerLoop();
};


class FileCollector
{
public:
    void CollectFile(uint32_t fileId, size_t fileSize);

    void OnNewChunk(
        uint32_t fileId,
        size_t pos, // позиция в файле
        std::vector<uint8_t>&& chunk);
 
    // Метод получения собранного файла. 
    // Требуется предложить и реализовать оптимальный интерфейс.
    // std::vector<uint8_t> GetFile(uint32_t fileId); // Как передавать данные? 

    // 1. std::optional нужен для обработки ситуации отсутствия файла
    // 2. используется std::shared_ptr для:
    //      1 случай: файл еще не собран - полная копия внутреннего буффера
    //      2 случай: файл уже собран полностью - возвращаю указатель на внутренний буффер
    // клиент должен не держать полученный shared_ptr дольше, 
    // чем живёт сам FileCollector (или конкретный File), чтобы избежать висячих указателей.

    std::shared_ptr<const std::vector<uint8_t>> GetFile(uint32_t fileId) const;
    std::optional<bool> IsComplete(uint32_t fileId) const;
private:
    struct File 
    {
        size_t fileSize;
        std::vector<uint8_t> buffer;
        std::map<size_t, size_t> intervals; // 1 аргумент старт - ключ, 2 конец - шзначение
        mutable std::shared_mutex fileMtx;

        explicit File(size_t fsz);

        bool isCompleteUnsafe() const;
    };

    std::unordered_map<uint32_t, std::unique_ptr<File>> files_;
    File* getFile(uint32_t fileId);

    mutable std::shared_mutex filesMtx;

    void processChunk(uint32_t fileId, size_t chunkStart, std::vector<uint8_t>&& chunk);
};
