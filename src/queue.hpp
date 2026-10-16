#pragma once
#include "catalog.hpp"

namespace psgi {
enum class QueueStage { Waiting, Checking, Downloading, Registering, Installing, Verifying, Completed, Failed, Cancelled, Cancelling, Unconfirmed };
struct QueueItem {
    Entry entry;
    QueueStage stage = QueueStage::Waiting;
    int task = -1;
    uint64_t transferred = 0, total = 0, updated = 0, verificationStarted = 0;
    bool systemCompleted = false;
    bool backgroundTransfer = false;
    bool previouslyPresent = false;
    std::string previousVersion;
    std::string dependsOn;
    std::string message;
    double sampleTime = 0;
    uint64_t sampleBytes = 0, bytesPerSecond = 0;
};
std::string queueStageName(QueueStage stage);
bool queueActive(QueueStage stage);
QueueItem* findJob(std::vector<QueueItem>& queue, const std::string& url);
void enqueueJob(std::vector<QueueItem>& queue, const Entry& entry, uint64_t now);
void verifyJob(QueueItem& job, const std::vector<Entry>& installed, uint64_t now);
void sampleJob(QueueItem& job, uint64_t bytes, uint64_t total, double elapsed);
std::string queueProgress(const QueueItem& job);
std::vector<QueueItem> parseQueue(const std::string& text);
std::string serializeQueue(const std::vector<QueueItem>& queue);
std::vector<QueueItem> loadQueue(const std::string& directory);
void saveQueue(const std::string& directory, const std::vector<QueueItem>& queue);
}
