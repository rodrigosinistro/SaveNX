#include "remote/UploadQueue.hpp"
#include "AppPaths.hpp"
#include "TextSafety.hpp"
#include "AtomicFile.hpp"
#include <cassert>
#include <iostream>

int main()
{
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / ("savenx-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    assert(savenx::prepare_layout(root));
    const auto source = root / "backups" / "original.zip";
    { std::ofstream f(source, std::ios::binary); f << "complete-test-payload"; }
    remote::UploadQueue q(root / "pending_uploads");
    assert(!q.enqueue(source, "../other", "TITLE_1", "test.zip"));
    assert(!q.enqueue(source, "USER_1", "TITLE_1", "../test.zip"));
    assert(q.enqueue(source, "USER_1", "TITLE_1", "backup com espaços.zip"));
    auto jobs = q.list();
    assert(jobs.size() == 1 && jobs[0].name == "backup com espaços.zip");
    // A new queue instance after restart must recover an independent snapshot.
    { std::ofstream f(source, std::ios::binary | std::ios::trunc); f << "changed"; }
    remote::UploadQueue restarted(root / "pending_uploads");
    assert(restarted.list().size() == 1 && fs::file_size(jobs[0].zip) == 21);
    { std::ofstream f(root / "pending_uploads" / "interrupted.job.tmp"); f << "partial"; }
    assert(restarted.list().size() == 1);
    // Failed uploads do not acknowledge anything. The next launch sees the same job.
    assert(q.list()[0].id == jobs[0].id);
    assert(q.acknowledge(jobs[0]));
    assert(q.list().empty() && fs::exists(source));
    assert(!q.enqueue(root / "missing.zip", "USER_1", "TITLE_1", "x.zip"));
    assert(q.enqueue(source, "USER_1", "TITLE_1", "corrupt.zip"));
    jobs = q.list();
    { std::ofstream f(jobs[0].zip, std::ios::trunc); f << "x"; }
    assert(q.list().empty()); // A truncated snapshot cannot be uploaded.
    assert(savenx::plain_text("Pokémon #RED*") == "Pokémon  RED ");
    assert(savenx::plain_text(std::string("A\xff") + "B") == "AB");
    assert(savenx::plain_text("á", 1).empty());
    const auto session = root / "config" / "session.json";
    assert(savenx::atomic_write(session, "first"));
    assert(savenx::atomic_write(session, "second"));
    assert(fs::file_size(session) == 6);
    fs::rename(session, session.string() + ".previous");
    assert(fs::exists(savenx::recoverable_file(session)));
    std::error_code ec;
    fs::remove_all(root, ec);
    std::cout << "Upload persistence, interrupted writes, isolation and text safety: PASS\n";
}
