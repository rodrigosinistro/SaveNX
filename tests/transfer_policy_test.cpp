#include "remote/TransferPolicy.hpp"
#include <cassert>
#include <iostream>
int main()
{
    using namespace remote;
    assert(completed_upload(200, 42, 42, "file-id"));
    assert(completed_upload(201, 42, 42, "file-id"));
    assert(!completed_upload(308, 42, 42, "file-id"));
    assert(!completed_upload(202, 42, 42, "file-id"));
    assert(!completed_upload(200, 42, 41, "file-id"));
    assert(!completed_upload(200, 42, 42, ""));
    assert(retryable_upload(0) && retryable_upload(429) && retryable_upload(503));
    assert(!retryable_upload(401) && !retryable_upload(403) && !retryable_upload(404));
    assert(safe_session_url("https://www.googleapis.com/upload/drive/v3/files?upload_id=x"));
    assert(!safe_session_url("https://www.googleapis.com.evil.test/upload/drive/"));
    assert(!safe_session_url("http://www.googleapis.com/upload/drive/"));
    std::cout << "Google upload acknowledgements, retries and session origins: PASS\n";
}
