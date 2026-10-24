#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>

namespace remote
{
    // No platform services here: persistence and crash recovery are host-testable.
    struct UploadJob
    {
        std::string id, user, title, name;
        std::uintmax_t size{};
        std::filesystem::path zip;
    };

    class UploadQueue
    {
        public:
            explicit UploadQueue(std::filesystem::path root) : m_root(std::move(root)) {}

            static bool safe_key(const std::string &s)
            {
                return !s.empty() && s.size() <= 128 &&
                       std::all_of(s.begin(), s.end(), [](unsigned char c) {
                           return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                  (c >= '0' && c <= '9') || c == '_' || c == '-';
                       });
            }

            bool enqueue(const std::filesystem::path &source, const std::string &user,
                         const std::string &title, const std::string &name)
            {
                if (!safe_key(user) || !safe_key(title) || name.empty() || name.size() > 512 ||
                    name.find('\0') != name.npos || name.find('/') != name.npos || name.find('\\') != name.npos)
                { return false; }
                std::error_code ec;
                const auto size = std::filesystem::file_size(source, ec);
                if (ec || size == 0) { return false; }
                std::filesystem::create_directories(m_root, ec);
                if (ec) { return false; }

                static std::atomic<unsigned long> sequence{};
                const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
                const std::string id = std::to_string(stamp) + "_" + std::to_string(sequence++);
                const auto zip = m_root / (id + ".zip");
                const auto tempZip = m_root / (id + ".zip.tmp");
                const auto tempJob = m_root / (id + ".job.tmp");
                std::filesystem::copy_file(source, tempZip, std::filesystem::copy_options::none, ec);
                if (ec) { return false; }
                if (std::filesystem::file_size(tempZip, ec) != size || ec) { return false; }
                std::filesystem::rename(tempZip, zip, ec);
                if (ec) { return false; }
                {
                    std::ofstream out(tempJob, std::ios::binary | std::ios::trunc);
                    out << "SAVENX_QUEUE_1\n" << std::quoted(user) << '\n' << std::quoted(title) << '\n'
                        << std::quoted(name) << '\n' << size << '\n';
                    out.flush();
                    if (!out.good()) { return false; }
                    out.close();
                    if (out.fail()) { return false; }
                }
                // Only a fully published marker makes a job eligible for upload.
                std::filesystem::rename(tempJob, m_root / (id + ".job"), ec);
                return !ec;
            }

            std::vector<UploadJob> list() const
            {
                std::vector<UploadJob> jobs;
                std::error_code ec;
                std::filesystem::directory_iterator it(m_root, ec), end;
                if (ec) { return jobs; }
                for (; it != end; it.increment(ec))
                {
                    if (ec) { break; }
                    if (it->path().extension() != ".job") { continue; }
                    UploadJob job;
                    job.id = it->path().stem().string();
                    if (!safe_key(job.id)) { continue; }
                    std::ifstream in(it->path(), std::ios::binary);
                    std::string version;
                    if (!(in >> version >> std::quoted(job.user) >> std::quoted(job.title) >>
                          std::quoted(job.name) >> job.size) || version != "SAVENX_QUEUE_1" ||
                        !safe_key(job.user) || !safe_key(job.title) || job.name.empty() || job.size == 0)
                    { continue; }
                    job.zip = m_root / (job.id + ".zip");
                    const auto size = std::filesystem::file_size(job.zip, ec);
                    if (ec || size != job.size) { ec.clear(); continue; }
                    jobs.push_back(std::move(job));
                }
                std::sort(jobs.begin(), jobs.end(), [](const auto &a, const auto &b) { return a.id < b.id; });
                return jobs;
            }

            bool acknowledge(const UploadJob &job)
            {
                if (!safe_key(job.id)) { return false; }
                std::error_code ec;
                // Remove the marker first. A crash afterwards leaves only an inert ZIP.
                if (!std::filesystem::remove(m_root / (job.id + ".job"), ec) || ec) { return false; }
                std::filesystem::remove(m_root / (job.id + ".zip"), ec);
                return !ec;
            }

        private:
            std::filesystem::path m_root;
    };
}
