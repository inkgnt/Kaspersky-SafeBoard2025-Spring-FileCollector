#include "FileCollector.hpp"
#include <iostream>
// struct File definition
FileCollector::File::File(size_t fsz) : fileSize(fsz) 
{
	buffer.reserve(fsz); // резервирует
    buffer.resize(fsz); // меняет size, без этого запись в вектор невозможна
}

bool FileCollector::File::isCompleteUnsafe() const 
{
    if (intervals.size() != 1)
        return false;

    auto it = intervals.begin();
    size_t start = it->first;
    size_t end = it->second;

    return (start == 0 && end == buffer.size());
}

FileCollector::File* FileCollector::getFile(uint32_t fileId) 
{
    std::shared_lock<std::shared_mutex> lock(filesMtx);

    auto it = files_.find(fileId);
    if (it == files_.end())
        return nullptr;
    return it->second.get();
}

// ---------------------------------------------------------------------------------------------
// Class FileCollector публичный интерфейс
void FileCollector::CollectFile(uint32_t fileId, size_t fileSize)
{
    std::unique_lock lock(filesMtx);
    if (files_.find(fileId) != files_.end())
        return;

    files_[fileId] = std::make_unique<File>(fileSize);
}

void FileCollector::OnNewChunk(uint32_t fileId, size_t pos, std::vector<uint8_t>&& chunk) {
    File* file = getFile(fileId);
    if (!file)
        return;

    {
        std::shared_lock<std::shared_mutex> readLock(file->fileMtx);
        if (file->isCompleteUnsafe())
            return;
    }

    processChunk(fileId, pos, std::move(chunk));
}

std::shared_ptr<const std::vector<uint8_t>> FileCollector::GetFile(uint32_t fileId) const
{
    std::shared_lock lock(filesMtx);
    auto it = files_.find(fileId);
    if (it == files_.end())
        return nullptr;

    File* file = it->second.get();
    std::shared_lock fileLock(file->fileMtx);

    if (file->isCompleteUnsafe()) { // если полный файл, то можно не бояться гонок данных, и вернуть указатель тк
                                    // больше он изменяться не будет
        return std::shared_ptr<const std::vector<uint8_t>>(
            &file->buffer,                      // сырой указатель
            [](const std::vector<uint8_t>*) {}  // лямбда делитор который ничего не делает, для предотвращения UB 
        );
    }
    // если файл неполный мы вынуждены возвращать полную копию чтобы избежать состояния гонки

    return std::make_shared<std::vector<uint8_t>>(file->buffer);
}


std::optional<bool> FileCollector::IsComplete(uint32_t fileId) const 
{
    std::shared_lock<std::shared_mutex> lockFiles(filesMtx);
    auto it = files_.find(fileId);
    if (it == files_.end())
        return std::nullopt;

    std::shared_lock<std::shared_mutex> lockFile(it->second.get()->fileMtx);

    return it->second.get()->isCompleteUnsafe();
}

// ---------------------------------------------------------------------------------------------
// Class FileCollector приватный интерфейс
void FileCollector::processChunk(uint32_t fileId, size_t chunkStart, std::vector<uint8_t>&& chunk)
{
    File* file = getFile(fileId);

    std::unique_lock lock(file->fileMtx);

    size_t chunkEnd = chunkStart + chunk.size();

    if (chunkEnd > file->fileSize)
        return; // или обрезать чанк, если выходит за пределы, я выбрал просто отклонить его
    
    if (file->intervals.find(chunkStart) != file->intervals.end() && file->intervals.find(chunkStart)->second == chunkEnd)
        return;

    std::map<size_t, size_t>::iterator it = file->intervals.lower_bound(chunkStart); // следующий существующий интервал после начальной позиции

    if (it != file->intervals.begin()) {
        std::map<size_t, size_t>::iterator prev = std::prev(it);
        if (prev->second > chunkStart) 
            it = prev;
    }

    // условие пересечения:
    // [x, y), [a, b) 
    // x < b && a < y 

    // [chunkStart, chunkEnd), [it->first, it->second)
    // chunkStart < it->second && it->first < chunkEnd

    size_t mergedStart = chunkStart; // нужно для обьединения пересекающихся интервалов
    size_t mergedEnd = chunkEnd;

    size_t writePos = chunkStart; // начальная позиция с которой хотим копировать в буффер
    while (it != file->intervals.end() && it->first < chunkEnd) 
    { 
        if (chunkStart < it->second) { // chunkStart < it->second && it->first < chunkEnd
            size_t overlapStart = std::max(chunkStart, it->first);
            size_t overlapEnd = std::min(chunkEnd, it->second);

            if (writePos < overlapStart)
                std::copy(
                    chunk.begin() + (writePos - chunkStart),
                    chunk.begin() + (overlapStart - chunkStart),
                    file->buffer.begin() + writePos
                );
            
            writePos = overlapEnd;

            mergedStart = std::min(it->first, mergedStart);
            mergedEnd = std::max(it->second, mergedEnd);

            it = file->intervals.erase(it); // ++it
        }
        else // else, так как it = file->intervals.erase(it); эквивалентно ++it;  
        {
            ++it; // в постфиксной нотации копия, поэтому префиксная
        }     
    }

    if (writePos < chunkEnd) {
        std::copy(
            chunk.begin() + (writePos - chunkStart),
            chunk.end(),
            file->buffer.begin() + writePos
        );
    }

    file->intervals[mergedStart] = mergedEnd;

    auto itNew = file->intervals.find(mergedStart);
    
    // 1. пробуем слить с предыдущим, если prev->second == itNew->first
    if (itNew != file->intervals.begin()) {
        auto prev = std::prev(itNew);
        if (prev->second == itNew->first) {
            size_t newStart = prev->first;
            size_t newEnd = itNew->second;
            file->intervals.erase(prev);
            file->intervals.erase(itNew);
            itNew = file->intervals.emplace(newStart, newEnd).first;
        }
    }
        // 2. пробуем слить с следующим, если itNew->second == next->first
    auto next = std::next(itNew);
    if (next != file->intervals.end() && itNew->second == next->first) {
        size_t newStart = itNew->first;
        size_t newEnd = next->second;
        file->intervals.erase(itNew);
        file->intervals.erase(next);
        file->intervals.emplace(newStart, newEnd);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Class ThreadPool
ThreadPool::ThreadPool() : numThreads(std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : 1), stopFlag(false) {
    for (uint16_t i = 0; i < numThreads; i++)
        workers.emplace_back([this]() { this->workerLoop(); });
    
}

ThreadPool::ThreadPool(uint16_t n) : numThreads(n ? n : 1), stopFlag(false) {
    for (uint16_t i = 0; i < numThreads; i++)
        workers.emplace_back([this]() { this->workerLoop(); });
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queueMtx);
        stopFlag = true;
    }
    condition.notify_all();

    for (auto& thread : workers)
        if (thread.joinable())
            thread.join();
}

void ThreadPool::EnqueueTask(std::function<void()>&& task) {
    {
        std::unique_lock<std::mutex> lock(queueMtx);
        tasks.emplace(std::move(task));
    }
    condition.notify_one();
}

void ThreadPool::workerLoop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queueMtx);
            condition.wait(lock, [this] { return stopFlag || !tasks.empty(); });
            if (stopFlag && tasks.empty())
                return;
            task = std::move(tasks.front());
            tasks.pop();
        }
        task();
    }
}
