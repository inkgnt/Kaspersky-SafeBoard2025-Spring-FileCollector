#include <gtest/gtest.h>
#include "FileCollector.hpp"

// проверка корректного создания файла
TEST(FileCollectorTest, CanCreateFile) {
    FileCollector collector;
    collector.CollectFile(1, 100);
    auto file = collector.GetFile(1);
    ASSERT_TRUE(!file.empty());
    EXPECT_EQ(file.size(), 100);
}

// проверка получения несуществующего файла
TEST(FileCollectorTest, FileReturnsNullopt) {
    FileCollector collector;
    auto file = collector.GetFile(999);
    std::vector<uint8_t> buf{};
    EXPECT_EQ(file, buf);
}

// проверка простого добавления чанка
TEST(FileCollectorTest, CanAddChunk) {
    FileCollector collector;
    collector.CollectFile(1, 10);

    collector.OnNewChunk(1, 0, { 'a', 'b', 'c', 'd' });
    auto result = collector.GetFile(1);

    ASSERT_TRUE(!result.empty());
    
    EXPECT_EQ(result[0], 'a');
    EXPECT_EQ(result[1], 'b');
    EXPECT_EQ(result[2], 'c');
    EXPECT_EQ(result[3], 'd');
}

// проверка добавления чанка за границами файла
TEST(FileCollectorTest, IgnoreOutOfRangeChunk) {
    FileCollector collector;
    collector.CollectFile(1, 5);

    collector.OnNewChunk(1, 6, { 'x', 'y', 'z' });

    auto result = collector.GetFile(1);
    ASSERT_TRUE(!result.empty());

    for (size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(result[i], 0);
    }
}

// проверка пересечения справа
TEST(FileCollectorTest, MergeOverlappingChunks) {
    FileCollector collector;
    collector.CollectFile(1, 10);

    collector.OnNewChunk(1, 0, { 'a', 'b', 'c', 'd' });
    collector.OnNewChunk(1, 2, { 'x', 'y', 'z' });

    auto result = collector.GetFile(1);
    ASSERT_TRUE(!result.empty());

    std::vector<uint8_t> expected{ 'a', 'b', 'c', 'd', 'z', '\0', '\0', '\0', '\0', '\0' };
    EXPECT_EQ(result, expected);
}

// проверка пересечения слева
TEST(FileCollectorTest, MergeOverlappingChunks2) {
    FileCollector collector;
    collector.CollectFile(1, 10);

    collector.OnNewChunk(1, 3, { 'a', 'b', 'c', 'd' });
    collector.OnNewChunk(1, 2, { 'x', 'y', 'z' });

    auto result = collector.GetFile(1);
    ASSERT_TRUE(!result.empty());

    std::vector<uint8_t> expected{ '\0', '\0', 'x', 'a', 'b', 'c', 'd', '\0', '\0', '\0' };
    EXPECT_EQ(result, expected);
}

// проверка пересечения слева и справа
TEST(FileCollectorTest, MergeOverlappingChunks3) {
    FileCollector collector;
    collector.CollectFile(1, 10);

    collector.OnNewChunk(1, 3, { 'a', 'a', 'a', 'a' });
    collector.OnNewChunk(1, 2, { 'b', 'b', 'b' });
    collector.OnNewChunk(1, 5, { 'c', 'c', 'c', 'c', 'c' });

    auto result = collector.GetFile(1);
    ASSERT_TRUE(!result.empty());

    std::vector<uint8_t> expected{ '\0', '\0', 'b', 'a', 'a', 'a', 'a', 'c', 'c', 'c' };
    EXPECT_EQ(result, expected);
}

// множественное пересечение сложный случай
TEST(FileCollectorTest, MergeOverlappingChunks4) {
    FileCollector collector;
    collector.CollectFile(1, 20);

    collector.OnNewChunk(1, 2, { 'a', 'a', 'a', 'a' });
    collector.OnNewChunk(1, 3, { 'x', 'y', 'z' });
    collector.OnNewChunk(1, 12, { 'b', 'b', 'b', 'b' });
    collector.OnNewChunk(1, 4, { 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x' }); //15, последний индекс 18
    collector.OnNewChunk(1, 16, { 'y', 'y', 'y', 'y' });
    collector.OnNewChunk(1, 21, { 'f', 'f', 'f', 'f', 'f' });

    auto result = collector.GetFile(1);
    ASSERT_TRUE(!result.empty());

    std::vector<uint8_t> expected{ '\0', '\0', 'a', 'a', 'a', 'a', 'x', 'x', 'x', 'x', 'x', 'x', 'b', 'b', 'b', 'b', 'x', 'x', 'x', 'y' };
    EXPECT_EQ(result, expected);
}

// множественное пересечение еще сложный случай
TEST(FileCollectorTest, MergeOverlappingChunks5) {
    FileCollector collector;
    collector.CollectFile(1, 20);

    collector.OnNewChunk(1, 21, { 'f', 'f', 'f', 'f', 'f' });
    collector.OnNewChunk(1, 4, { 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x' }); //15, последний индекс 18
    collector.OnNewChunk(1, 2, { 'a', 'a', 'a', 'a' });
    collector.OnNewChunk(1, 3, { 'x', 'y', 'z' });
    collector.OnNewChunk(1, 12, { 'b', 'b', 'b', 'b' });
    collector.OnNewChunk(1, 16, { 'y', 'y', 'y', 'y' });
    collector.OnNewChunk(1, 0, { 'k', 'k', 'k', 'k', 'k' });
    

    auto result = collector.GetFile(1);
    ASSERT_TRUE(!result.empty());

    std::vector<uint8_t> expected{ 'k', 'k', 'a', 'a', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'y' };
    EXPECT_EQ(result, expected);
}
