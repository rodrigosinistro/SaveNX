#include "remote/GoogleDrive.hpp"
#include "remote/TransferPolicy.hpp"
#include "AtomicFile.hpp"
#if __has_include("oauth_client.generated.hpp")
#include "oauth_client.generated.hpp"
#else
namespace savenx::oauth { inline constexpr const char *CLIENT_ID = ""; inline constexpr const char *CLIENT_SECRET = ""; }
#endif

#include "error.hpp"
#include "logging/logger.hpp"
#include "remote/Form.hpp"
#include "remote/URL.hpp"
#include "remote/remote.hpp"
#include "strings/strings.hpp"
#include "stringutil.hpp"

#include <algorithm>
#include <cstring>
#include <chrono>
#include <filesystem>
#include <thread>

namespace
{
    // This is the sort-of template for the authorization/token header.
    constexpr std::string_view HEADER_AUTH_BEARER = "Authorization: Bearer ";

    // Content type headers used.
    constexpr const char *HEADER_CONTENT_TYPE_JSON = "Content-Type: application/json";
    constexpr const char *HEADER_CONTENT_TYPE_FORM = "Content-Type: application/x-www-form-urlencoded";

    // This is used for upload/patch for grabbing the upload location.
    constexpr std::string_view HEADER_UPLOAD_LOCATION = "Location";

    // These are API endpoints used in multiple request calls.
    constexpr const char *URL_OAUTH2_TOKEN_URL = "https://oauth2.googleapis.com/token";
    constexpr const char *URL_DRIVE_FILE_API   = "https://www.googleapis.com/drive/v3/files";
    constexpr const char *URL_DRIVE_UPLOAD_API = "https://www.googleapis.com/upload/drive/v3/files";

    // These are json keys that are used for various requests.
    constexpr const char *JSON_KEY_ACCESS_TOKEN  = "access_token";
    constexpr const char *JSON_KEY_CLIENT_ID     = "client_id";
    constexpr const char *JSON_KEY_CLIENT_SECRET = "client_secret";
    constexpr const char *JSON_KEY_DEVICE_CODE   = "device_code";
    constexpr const char *JSON_KEY_EXPIRES_IN    = "expires_in";
    constexpr const char *JSON_KEY_GRANT_TYPE    = "grant_type";
    constexpr const char *JSON_KEY_ID            = "id";
    constexpr const char *JSON_KEY_INSTALLED     = "installed";
    constexpr const char *JSON_KEY_MIMETYPE      = "mimeType";
    constexpr const char *JSON_KEY_NAME          = "name";
    constexpr const char *JSON_KEY_PARENTS       = "parents";
    constexpr const char *JSON_KEY_REFRESH_TOKEN = "refresh_token";

    /// @brief Folder mimetype string.
    constexpr const char *MIME_TYPE_DIRECTORY = "application/vnd.google-apps.folder";
} // namespace

//                      ---- Construction ----

remote::GoogleDrive::GoogleDrive() : Storage("[GD]", true)
{
    m_clientId = savenx::oauth::CLIENT_ID;
    m_clientSecret = savenx::oauth::CLIENT_SECRET;
    m_embeddedClient = !m_clientId.empty();
    const auto configPath = savenx::recoverable_file(std::filesystem::path(remote::PATH_GOOGLE_DRIVE_CONFIG)).string();
    auto config = json::new_object(json_object_from_file, configPath.c_str());
    json_object *installed = config ? json::get_object(config, "installed") : nullptr;
    if (installed)
    {
        const char *savedId = json_object_get_string(json_object_object_get(installed, "client_id"));
        const char *savedSecret = json_object_get_string(json_object_object_get(installed, "client_secret"));
        const char *token = json_object_get_string(json_object_object_get(installed, "refresh_token"));
        if (!m_embeddedClient || (savedId && savedSecret))
        {
            m_embeddedClient = false;
            if (savedId) { m_clientId = savedId; }
            if (savedSecret) { m_clientSecret = savedSecret; }
        }
        if (token && (!savedId || m_clientId == savedId)) { m_refreshToken = token; }
    }
    if (m_clientId.empty() || m_refreshToken.empty()) { return; }
    if (refresh_token() && get_root_id() && request_listing()) { m_isInitialized = true; }
}

bool remote::GoogleDrive::save_session()
{
    auto config = json::new_object(json_object_new_object);
    auto installed = json_object_new_object();
    json_object_object_add(installed, "client_id", json_object_new_string(m_clientId.c_str()));
    if (!m_embeddedClient)
    { json_object_object_add(installed, "client_secret", json_object_new_string(m_clientSecret.c_str())); }
    json_object_object_add(installed, "refresh_token", json_object_new_string(m_refreshToken.c_str()));
    json_object_object_add(config.get(), "installed", installed);
    if (!savenx::atomic_write(std::filesystem::path(remote::PATH_GOOGLE_DRIVE_CONFIG), json_object_to_json_string_ext(config.get(), JSON_C_TO_STRING_PRETTY)))
    { m_lastError = "Falha ao salvar a sessão no SD"; return false; }
    return true;
}

//                      ---- Public functions ----

bool remote::GoogleDrive::create_directory(std::string_view name)
{
    if (!GoogleDrive::token_is_valid() && !GoogleDrive::refresh_token()) { return false; }

    curl::HeaderList headers = curl::new_header_list();
    curl::append_header(headers, m_authHeader);
    curl::append_header(headers, HEADER_CONTENT_TYPE_JSON);

    json::Object postJson      = json::new_object(json_object_new_object);
    json_object *directoryName = json_object_new_string(name.data());
    json_object *mimeType      = json_object_new_string(MIME_TYPE_DIRECTORY);
    json::add_object(postJson, JSON_KEY_NAME, directoryName);
    json::add_object(postJson, JSON_KEY_MIMETYPE, mimeType);
    if (!m_parent.empty())
    {
        // I don't understand why this is an array.
        json_object *parentArray = json_object_new_array();
        json_object *parentId    = json_object_new_string(m_parent.c_str());
        json_object_array_add(parentArray, parentId);
        json::add_object(postJson, JSON_KEY_PARENTS, parentArray);
    }

    std::string response;
    curl::prepare_post(m_curl);
    curl::set_option(m_curl, CURLOPT_HTTPHEADER, headers.get());
    curl::set_option(m_curl, CURLOPT_URL, URL_DRIVE_FILE_API);
    curl::set_option(m_curl, CURLOPT_WRITEFUNCTION, curl::write_response_string);
    curl::set_option(m_curl, CURLOPT_WRITEDATA, &response);
    curl::set_option(m_curl, CURLOPT_POSTFIELDS, json_object_get_string(postJson.get()));

    if (!curl::perform(m_curl)) { return false; }

    json::Object parser = json::new_object(json_tokener_parse, response.c_str());
    if (!parser)
    {
        logger::log("Error creating directory: Error parsing server response.");
        return false;
    }

    // This is all I really need from the response.
    json_object *id = json::get_object(parser, JSON_KEY_ID);
    if (error::is_null(id))
    {
        // This doesn't really mean the request wasn't successful. It just means the new directory couldn't be appended
        // to the list...
        logger::log("Error creating directory: Error finding directory ID in response!");
        return false;
    }

    m_list.emplace_back(name, json_object_get_string(id), m_parent, 0, true);

    return true;
}

bool remote::GoogleDrive::upload_file(const fslib::Path &source, std::string_view name, sys::ProgressTask *task)
{
    const std::string path = source.string();
    const std::string key = path.find("/pending_uploads/") != path.npos ?
                            std::filesystem::path(path).stem().string() : std::string();
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        m_lastTransferCode = 0;
        if (!token_is_valid() && !refresh_token()) { return false; }
        if (!key.empty())
        {
            fslib::File file(source, FsOpenMode_Read);
            bool found{};
            if (!file.is_open()) { return false; }
            if (!find_uploaded_job(key, file.get_size(), found))
            {
                if (!remote::retryable_upload(m_lastTransferCode)) { return false; }
                if (attempt < 2) { std::this_thread::sleep_for(std::chrono::milliseconds(500 * (1 << attempt))); }
                continue;
            }
            if (found) { return true; }
        }
        if (upload_attempt(source, name, key, task)) { return true; }
        if (!remote::retryable_upload(m_lastTransferCode)) { break; }
        if (attempt < 2) { std::this_thread::sleep_for(std::chrono::milliseconds(500 * (1 << attempt))); }
    }
    return false;
}

bool remote::GoogleDrive::find_uploaded_job(const std::string &key, std::int64_t expected, bool &found)
{
    found = false;
    remote::URL url{URL_DRIVE_FILE_API};
    const std::string query = "trashed = false and '" + m_parent + "' in parents and appProperties has { key='savenxJob' and value='" + key + "' }";
    url.append_parameter("q", query).append_parameter("fields", "files(id,name,size)").append_parameter("pageSize", "2");
    auto headers = curl::new_header_list();
    curl::append_header(headers, m_authHeader);
    std::string response;
    curl::prepare_get(m_curl);
    curl::set_option(m_curl, CURLOPT_URL, url.get());
    curl::set_option(m_curl, CURLOPT_HTTPHEADER, headers.get());
    curl::set_option(m_curl, CURLOPT_WRITEFUNCTION, curl::write_response_string);
    curl::set_option(m_curl, CURLOPT_WRITEDATA, &response);
    if (!curl::perform(m_curl)) { m_lastTransferCode = 0; return false; }
    m_lastTransferCode = curl::get_response_code(m_curl);
    if (m_lastTransferCode != 200) { return false; }
    auto body = json::new_object(json_tokener_parse, response.c_str());
    auto files = body ? json::get_object(body, "files") : nullptr;
    if (!files || !json_object_is_type(files, json_type_array)) { return false; }
    if (json_object_array_length(files) == 0) { return true; }
    auto file = json_object_array_get_idx(files, 0);
    const char *id = json_object_get_string(json_object_object_get(file, "id"));
    const char *name = json_object_get_string(json_object_object_get(file, "name"));
    const auto size = json_object_get_int64(json_object_object_get(file, "size"));
    if (!remote::completed_upload(200, expected, size, id ? id : "") || !name) { return false; }
    if (!get_item_by_id(id)) { m_list.emplace_back(name, id, m_parent, size, false); }
    found = true;
    return true;
}

bool remote::GoogleDrive::upload_attempt(const fslib::Path &source, std::string_view name,
                                        const std::string &key, sys::ProgressTask *task)
{
    fslib::File file(source, FsOpenMode_Read);
    if (!file.is_open() || file.get_size() <= 0) { return false; }
    const auto size = file.get_size();
    auto headers = curl::new_header_list();
    curl::append_header(headers, m_authHeader);
    curl::append_header(headers, HEADER_CONTENT_TYPE_JSON);
    curl::append_header(headers, "X-Upload-Content-Type: application/zip");
    const std::string lengthHeader = "X-Upload-Content-Length: " + std::to_string(size);
    curl::append_header(headers, lengthHeader);
    remote::URL url{URL_DRIVE_UPLOAD_API};
    url.append_parameter("uploadType", "resumable").append_parameter("fields", "id,name,size");
    auto body = json::new_object(json_object_new_object);
    json::add_object(body, "name", json_object_new_string_len(name.data(), name.size()));
    auto parents = json_object_new_array();
    json_object_array_add(parents, json_object_new_string(m_parent.c_str()));
    json::add_object(body, "parents", parents);
    if (!key.empty())
    {
        auto props = json_object_new_object();
        json_object_object_add(props, "savenxJob", json_object_new_string(key.c_str()));
        json::add_object(body, "appProperties", props);
    }
    curl::HeaderArray returnedHeaders;
    std::string response;
    curl::prepare_post(m_curl);
    curl::set_option(m_curl, CURLOPT_URL, url.get());
    curl::set_option(m_curl, CURLOPT_HTTPHEADER, headers.get());
    curl::set_option(m_curl, CURLOPT_HEADERFUNCTION, curl::write_header_array);
    curl::set_option(m_curl, CURLOPT_HEADERDATA, &returnedHeaders);
    curl::set_option(m_curl, CURLOPT_WRITEFUNCTION, curl::write_response_string);
    curl::set_option(m_curl, CURLOPT_WRITEDATA, &response);
    curl::set_option(m_curl, CURLOPT_POSTFIELDS, json::get_string(body));
    if (!curl::perform(m_curl)) { return false; }
    m_lastTransferCode = curl::get_response_code(m_curl);
    if (m_lastTransferCode != 200 && m_lastTransferCode != 201) { return false; }
    std::string location;
    if (!curl::get_header_value(returnedHeaders, "Location", location) || !remote::safe_session_url(location))
    { m_lastError = "Sessão de upload inválida"; return false; }
    auto uploadHeaders = curl::new_header_list();
    curl::append_header(uploadHeaders, m_authHeader);
    curl::append_header(uploadHeaders, "Content-Type: application/zip");
    if (task) { task->reset(static_cast<double>(size)); }
    curl::UploadStruct upload{.source = &file, .task = task};
    response.clear();
    curl::prepare_upload(m_curl);
    curl::set_option(m_curl, CURLOPT_URL, location.c_str());
    curl::set_option(m_curl, CURLOPT_HTTPHEADER, uploadHeaders.get());
    curl::set_option(m_curl, CURLOPT_READFUNCTION, curl::read_data_from_file);
    curl::set_option(m_curl, CURLOPT_READDATA, &upload);
    curl::set_option(m_curl, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(size));
    curl::set_option(m_curl, CURLOPT_TIMEOUT, 0L);
    curl::set_option(m_curl, CURLOPT_WRITEFUNCTION, curl::write_response_string);
    curl::set_option(m_curl, CURLOPT_WRITEDATA, &response);
    if (!curl::perform(m_curl)) { m_lastTransferCode = 0; return false; }
    m_lastTransferCode = curl::get_response_code(m_curl);
    auto result = json::new_object(json_tokener_parse, response.c_str());
    if (!result) { return false; }
    const char *id = json_object_get_string(json::get_object(result, "id"));
    const char *filename = json_object_get_string(json::get_object(result, "name"));
    const auto received = json_object_get_int64(json::get_object(result, "size"));
    if (!filename || !remote::completed_upload(m_lastTransferCode, size, received, id ? id : ""))
    { logger::log("Google upload was not confirmed: HTTP %ld", m_lastTransferCode); return false; }
    if (!get_item_by_id(id)) { m_list.emplace_back(filename, id, m_parent, size, false); }
    return true;
}

bool remote::GoogleDrive::patch_file(remote::Item *file, const fslib::Path &source, sys::ProgressTask *task)
{
    static constexpr const char *STRING_PATCH_ERROR = "Error patching file: %s";

    if (!GoogleDrive::token_is_valid() && !GoogleDrive::refresh_token()) { return false; }

    fslib::File sourceFile(source, FsOpenMode_Read);
    if (!sourceFile)
    {
        logger::log(STRING_PATCH_ERROR, fslib::error::get_string());
        return false;
    }

    curl::HeaderList header = curl::new_header_list();
    curl::append_header(header, m_authHeader);

    remote::URL url{URL_DRIVE_UPLOAD_API};
    url.append_path(file->get_id()).append_parameter("uploadType", "resumable");

    std::string response;
    curl::HeaderArray headerArray;
    curl::reset_handle(m_curl);
    curl::set_option(m_curl, CURLOPT_CUSTOMREQUEST, "PATCH");
    curl::set_option(m_curl, CURLOPT_HTTPHEADER, header.get());
    curl::set_option(m_curl, CURLOPT_HEADERFUNCTION, curl::write_header_array);
    curl::set_option(m_curl, CURLOPT_HEADERDATA, &headerArray);
    curl::set_option(m_curl, CURLOPT_URL, url.get());
    curl::set_option(m_curl, CURLOPT_WRITEFUNCTION, curl::write_response_string);
    curl::set_option(m_curl, CURLOPT_WRITEDATA, &response);

    if (!curl::perform(m_curl)) { return false; }

    // This is the target location to upload to.
    std::string location;
    if (!curl::get_header_value(headerArray, HEADER_UPLOAD_LOCATION, location))
    {
        logger::log(STRING_PATCH_ERROR, "Location could not be read from headers!");
        return false;
    }

    if (task) { task->reset(static_cast<double>(sourceFile.get_size())); }

    // For some reason, this doesn't need the auth header.
    curl::UploadStruct uploadData = {.source = &sourceFile, .task = task};

    curl::prepare_upload(m_curl);
    curl::set_option(m_curl, CURLOPT_URL, location.c_str());
    curl::set_option(m_curl, CURLOPT_READFUNCTION, curl::read_data_from_file);
    curl::set_option(m_curl, CURLOPT_READDATA, &uploadData);
    curl::set_option(m_curl, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(sourceFile.get_size()));
    curl::set_option(m_curl, CURLOPT_FAILONERROR, 1L);

    if (!curl::perform(m_curl)) { return false; }

    // Update the file size with the source file size.
    file->set_size(sourceFile.get_size());

    return true;
}

bool remote::GoogleDrive::download_file(const remote::Item *file, const fslib::Path &destination, sys::ProgressTask *task)
{
    if (!GoogleDrive::token_is_valid() && !GoogleDrive::refresh_token()) { return false; }

    const int64_t itemSize = file->get_size();
    fslib::File destFile{destination, FsOpenMode_Create | FsOpenMode_Write};
    if (!destFile)
    {
        logger::log("Error downloading file: local file could not be opened for writing!");
        return false;
    }

    if (task) { task->reset(static_cast<double>(itemSize)); }

    curl::HeaderList header = curl::new_header_list();
    curl::append_header(header, m_authHeader);

    remote::URL url{URL_DRIVE_FILE_API};
    url.append_path(file->get_id()).append_parameter("alt", "media");

    auto download = curl::create_download_struct(destFile, task, itemSize);

    curl::prepare_get(m_curl);
    curl::set_option(m_curl, CURLOPT_HTTPHEADER, header.get());
    curl::set_option(m_curl, CURLOPT_URL, url.get());
    curl::set_option(m_curl, CURLOPT_FAILONERROR, 1L);
    curl::set_option(m_curl, CURLOPT_WRITEFUNCTION, curl::write_download_to_file);
    curl::set_option(m_curl, CURLOPT_WRITEDATA, download.get());

    curl::set_option(m_curl, CURLOPT_TIMEOUT, 0L);
    if (!curl::perform(m_curl) || curl::get_response_code(m_curl) != 200) { return false; }
    const bool flushSuccess = destFile.flush();
    const bool sizeMatches  = itemSize < 0 || destFile.tell() == itemSize;
    if (!flushSuccess || !sizeMatches)
    {
        logger::log("Error downloading file: expected %lli bytes, wrote %lli bytes.",
                    static_cast<long long>(itemSize),
                    static_cast<long long>(destFile.tell()));
        return false;
    }
    return true;
}

bool remote::GoogleDrive::delete_item(const remote::Item *item)
{
    if (!GoogleDrive::token_is_valid() && !GoogleDrive::refresh_token()) { return false; }

    // Iterator is needed to remove it from the list.
    const std::string_view itemId = item->get_id();
    auto findItem                 = Storage::find_item_by_id(itemId);
    if (findItem == m_list.end())
    {
        logger::log("Error deleting item: Item not found in list!");
        return false;
    }

    curl::HeaderList header = curl::new_header_list();
    curl::append_header(header, m_authHeader);

    remote::URL url{URL_DRIVE_FILE_API};
    url.append_path(itemId);

    curl::reset_handle(m_curl);
    curl::set_option(m_curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl::set_option(m_curl, CURLOPT_URL, url.get());
    curl::set_option(m_curl, CURLOPT_HTTPHEADER, header.get());

    if (!curl::perform(m_curl)) { return false; }

    // This might be a better way to check?
    long code = curl::get_response_code(m_curl);
    if (code != 204)
    {
        logger::log("Error deleting item from Google Drive: %i.", code);
        return false;
    }

    // Erase from the master list.
    m_list.erase(findItem);

    return true;
}

bool remote::GoogleDrive::rename_item(remote::Item *item, std::string_view newName)
{
    if (!GoogleDrive::token_is_valid() && !GoogleDrive::refresh_token()) { return false; }

    curl::HeaderList header = curl::new_header_list();
    curl::append_header(header, m_authHeader);
    curl::append_header(header, HEADER_CONTENT_TYPE_JSON);

    remote::URL url{URL_DRIVE_FILE_API};
    url.append_path(item->get_id());

    json::Object post = json::new_object(json_object_new_object);
    json_object *name = json_object_new_string(newName.data());
    json::add_object(post, "name", name);

    std::string response{};
    const char *postString = json_object_get_string(post.get());
    curl::reset_handle(m_curl);
    curl::set_option(m_curl, CURLOPT_CUSTOMREQUEST, "PATCH");
    curl::set_option(m_curl, CURLOPT_URL, url.get());
    curl::set_option(m_curl, CURLOPT_HTTPHEADER, header.get());
    curl::set_option(m_curl, CURLOPT_POSTFIELDS, HEADER_CONTENT_TYPE_JSON);
    curl::set_option(m_curl, CURLOPT_POSTFIELDS, postString);
    curl::set_option(m_curl, CURLOPT_WRITEFUNCTION, curl::write_response_string);
    curl::set_option(m_curl, CURLOPT_WRITEDATA, &response);

    if (!curl::perform(m_curl)) { return false; }

    // We're only doing this to check for errors.
    json::Object responseParser = json::new_object(json_tokener_parse, response.c_str());
    if (GoogleDrive::error_occurred(responseParser)) { return false; }

    item->set_name(newName);
    return true;
}

bool remote::GoogleDrive::sign_in_required() const { return !m_isInitialized || m_refreshToken.empty(); }

bool remote::GoogleDrive::get_sign_in_data(std::string &message, std::string &code, std::time_t &expiration, int &wait)
{
    static constexpr const char *STRING_SIGN_IN_ERROR = "Error requesting sign-in data: %s";
    // This is the the URL for device codes and limited input.
    static constexpr const char *API_URL_DEVICE_CODE = "https://oauth2.googleapis.com/device/code";
    // This is a gimped version of what I used to use.
    static constexpr const char *STRING_DRIVE_FILE_SCOPE = "https://www.googleapis.com/auth/drive.file";

    const char *messageTemplate = strings::get_by_name(strings::names::GOOGLE_DRIVE, 0);

    curl::HeaderList header = curl::new_header_list();
    curl::append_header(header, HEADER_CONTENT_TYPE_FORM);

    remote::Form post{};
    post.append_parameter(JSON_KEY_CLIENT_ID, m_clientId).append_parameter("scope", STRING_DRIVE_FILE_SCOPE);

    std::string response;
    curl::prepare_post(m_curl);
    curl::set_option(m_curl, CURLOPT_HTTPHEADER, header.get());
    curl::set_option(m_curl, CURLOPT_URL, API_URL_DEVICE_CODE);
    curl::set_option(m_curl, CURLOPT_WRITEFUNCTION, curl::write_response_string);
    curl::set_option(m_curl, CURLOPT_WRITEDATA, &response);
    curl::set_option(m_curl, CURLOPT_POSTFIELDS, post.get());
    curl::set_option(m_curl, CURLOPT_POSTFIELDSIZE, post.length());

    if (!curl::perform(m_curl)) { return false; }

    json::Object parser = json::new_object(json_tokener_parse, response.c_str());
    if (!parser || GoogleDrive::error_occurred(parser)) { return false; }

    json_object *deviceCode      = json::get_object(parser, JSON_KEY_DEVICE_CODE);
    json_object *userCode        = json::get_object(parser, "user_code");
    json_object *verificationUrl = json::get_object(parser, "verification_url");
    if (!verificationUrl) { verificationUrl = json::get_object(parser, "verification_uri"); }
    json_object *expiresIn       = json::get_object(parser, "expires_in");
    json_object *interval        = json::get_object(parser, "interval");
    // These are required and fatal.
    if (error::is_null({deviceCode, userCode, verificationUrl, expiresIn, interval}))
    {
        logger::log(STRING_SIGN_IN_ERROR, "Malformed response.");
        return false;
    }

    message    = stringutil::get_formatted_string(messageTemplate,
                                               json_object_get_string(verificationUrl),
                                               json_object_get_string(userCode));
    code       = json_object_get_string(deviceCode);
    expiration = std::time(NULL) + std::min<int64_t>(json_object_get_int64(expiresIn), 1800);
    wait       = json_object_get_int64(interval);

    return true;
}

bool remote::GoogleDrive::poll_sign_in(std::string_view code)
{
    static constexpr const char *STRING_ERROR_POLLING = "Error polling Google OAuth2: %s";
    // I'm just gonna save this pre-escaped instead of wasting time escaping it.
    static constexpr const char *STRING_DEVICE_GRANT = "urn:ietf:params:oauth:grant-type:device_code";

    // I'm not sure how else to make this really work with SaveNX's task threading system?
    remote::Form post{};
    post.append_parameter(JSON_KEY_CLIENT_ID, m_clientId)
        .append_parameter(JSON_KEY_CLIENT_SECRET, m_clientSecret)
        .append_parameter("device_code", code)
        .append_parameter(JSON_KEY_GRANT_TYPE, STRING_DEVICE_GRANT);

    curl::HeaderList header = curl::new_header_list();
    curl::append_header(header, HEADER_CONTENT_TYPE_FORM);

    std::string response;
    curl::prepare_post(m_curl);
    curl::set_option(m_curl, CURLOPT_HTTPHEADER, header.get());
    curl::set_option(m_curl, CURLOPT_URL, URL_OAUTH2_TOKEN_URL);
    curl::set_option(m_curl, CURLOPT_WRITEFUNCTION, curl::write_response_string);
    curl::set_option(m_curl, CURLOPT_WRITEDATA, &response);
    curl::set_option(m_curl, CURLOPT_POSTFIELDS, post.get());
    curl::set_option(m_curl, CURLOPT_POSTFIELDSIZE, post.length());

    if (!curl::perform(m_curl) || response.empty()) { return false; }

    json::Object parser = json::new_object(json_tokener_parse, response.c_str());
    // This error isn't logged, because that's how you know if the user logged in or not.
    if (!parser) { m_pollState = PollState::Error; m_lastError = "Resposta Google inválida"; return false; }
    if (auto error = json::get_object(parser, "error"))
    {
        const char *code = json_object_get_string(error);
        m_lastError = code ? code : "oauth_error";
        m_pollState = m_lastError == "authorization_pending" ? PollState::Pending :
                      m_lastError == "slow_down" ? PollState::SlowDown :
                      m_lastError == "access_denied" ? PollState::Denied :
                      m_lastError == "expired_token" ? PollState::Expired : PollState::Error;
        return false;
    }

    json_object *accessToken  = json::get_object(parser, JSON_KEY_ACCESS_TOKEN);
    json_object *expiresIn    = json::get_object(parser, JSON_KEY_EXPIRES_IN);
    json_object *refreshToken = json::get_object(parser, JSON_KEY_REFRESH_TOKEN);
    // All of these are required.
    if (error::is_null({accessToken, expiresIn, refreshToken}))
    {
        logger::log(STRING_ERROR_POLLING, "Malformed response or missing data!");
        return false;
    }

    m_token        = json_object_get_string(accessToken);
    m_refreshToken = json_object_get_string(refreshToken);
    m_tokenExpires = std::time(NULL) + json_object_get_uint64(expiresIn);
    m_authHeader   = std::string(HEADER_AUTH_BEARER) + m_token;

    if (!save_session()) { m_pollState = PollState::Error; return false; }

    if (!GoogleDrive::get_root_id() || !request_listing())
    { m_pollState = PollState::Error; m_lastError = "Autorizado, mas não foi possível listar o Drive"; return false; }
    m_isInitialized = true;
    m_pollState = PollState::Complete;

    return true;
}

//                      ---- Private functions ----

bool remote::GoogleDrive::get_root_id()
{
    // This is the only place this is used. V3 of the API doesn't allow you to retrieve this for some reason?
    static constexpr const char *API_URL_ABOUT_ROOT_ID = "https://www.googleapis.com/drive/v2/about?fields=rootFolderId";

    if (!GoogleDrive::token_is_valid() && !GoogleDrive::refresh_token()) { return false; }

    curl::HeaderList header = curl::new_header_list();
    curl::append_header(header, m_authHeader);

    std::string response;
    curl::prepare_get(m_curl);
    curl::set_option(m_curl, CURLOPT_HTTPHEADER, header.get());
    curl::set_option(m_curl, CURLOPT_URL, API_URL_ABOUT_ROOT_ID);
    curl::set_option(m_curl, CURLOPT_WRITEFUNCTION, curl::write_response_string);
    curl::set_option(m_curl, CURLOPT_WRITEDATA, &response);

    if (!curl::perform(m_curl)) { return false; }

    json::Object parser = json::new_object(json_tokener_parse, response.c_str());
    if (!parser) { return false; }

    json_object *rootId = json::get_object(parser, "rootFolderId");
    if (error::is_null(rootId))
    {
        logger::log("Error getting drive root directory ID!");
        return false;
    }

    m_root   = json_object_get_string(rootId);
    m_parent = m_root;

    return true;
}

bool remote::GoogleDrive::token_is_valid() const noexcept
{
    // I'm giving this a grace period just to be safe.
    return std::time(NULL) < m_tokenExpires - 10;
}

bool remote::GoogleDrive::refresh_token()
{
    curl::HeaderList header = curl::new_header_list();
    curl::append_header(header, HEADER_CONTENT_TYPE_FORM);

    remote::Form post{};
    post.append_parameter(JSON_KEY_CLIENT_ID, m_clientId)
        .append_parameter(JSON_KEY_CLIENT_SECRET, m_clientSecret)
        .append_parameter(JSON_KEY_REFRESH_TOKEN, m_refreshToken)
        .append_parameter(JSON_KEY_GRANT_TYPE, JSON_KEY_REFRESH_TOKEN);

    std::string response;
    curl::prepare_post(m_curl);
    curl::set_option(m_curl, CURLOPT_HTTPHEADER, header.get());
    curl::set_option(m_curl, CURLOPT_URL, URL_OAUTH2_TOKEN_URL);
    curl::set_option(m_curl, CURLOPT_WRITEFUNCTION, curl::write_response_string);
    curl::set_option(m_curl, CURLOPT_WRITEDATA, &response);
    curl::set_option(m_curl, CURLOPT_POSTFIELDS, post.get());
    curl::set_option(m_curl, CURLOPT_POSTFIELDSIZE, post.length());

    if (!curl::perform(m_curl)) { return false; }

    json::Object parser = json::new_object(json_tokener_parse, response.c_str());
    if (!parser || GoogleDrive::error_occurred(parser)) { return false; }

    // These are the only things I care about.
    json_object *accessToken = json::get_object(parser, JSON_KEY_ACCESS_TOKEN);
    json_object *expiresIn   = json::get_object(parser, JSON_KEY_EXPIRES_IN);
    if (error::is_null({accessToken, expiresIn})) { return false; }

    m_token        = json_object_get_string(accessToken);
    m_tokenExpires = std::time(NULL) + json_object_get_uint64(expiresIn);
    m_authHeader   = std::string(HEADER_AUTH_BEARER) + m_token;

    return true;
}

bool remote::GoogleDrive::request_listing()
{
    if (!GoogleDrive::token_is_valid() && !GoogleDrive::refresh_token()) { return false; }

    curl::HeaderList header = curl::new_header_list();
    curl::append_header(header, m_authHeader.c_str());

    remote::URL url{URL_DRIVE_FILE_API};
    url.append_parameter("fields", "nextPageToken,files(name,id,size,parents,mimeType)")
        .append_parameter("orderBy", "name_natural")
        .append_parameter("pageSize", "256")
        .append_parameter("q", "trashed = false"); // This works, but could get messy if more parameters are added.

    std::string response;
    curl::prepare_get(m_curl);
    curl::set_option(m_curl, CURLOPT_HTTPHEADER, header.get());
    curl::set_option(m_curl, CURLOPT_URL, url.get());
    curl::set_option(m_curl, CURLOPT_WRITEFUNCTION, curl::write_response_string);
    curl::set_option(m_curl, CURLOPT_WRITEDATA, &response);

    // This is used as the loop condition.
    json_object *nextPageToken = nullptr;
    do {
        response.clear();

        if (!curl::perform(m_curl)) { return false; }

        json::Object parser = json::new_object(json_tokener_parse, response.c_str());
        if (!parser || GoogleDrive::error_occurred(parser) || !GoogleDrive::process_listing(parser))
        {
            logger::log("Error while parsing Google Drive response!");
            return false;
        }

        nextPageToken = json::get_object(parser, "nextPageToken");
        if (nextPageToken)
        {
            remote::URL nextPage{url};
            nextPage.append_parameter("pageToken", json_object_get_string(nextPageToken));
            curl::set_option(m_curl, CURLOPT_URL, nextPage);
        }
    } while (nextPageToken);

    return true;
}

bool remote::GoogleDrive::process_listing(json::Object &json)
{
    static constexpr const char *STRING_ERROR_PROCESSING = "Error processing Google Drive listing: %s";

    json_object *files = json::get_object(json, "files");
    if (!files) { return false; }

    size_t arrayLength = json_object_array_length(files);
    for (size_t i = 0; i < arrayLength; i++)
    {
        json_object *currentFile = json_object_array_get_idx(files, i);
        if (!currentFile) { return false; }

        json_object *mimeType = json_object_object_get(currentFile, JSON_KEY_MIMETYPE);
        json_object *parents  = json_object_object_get(currentFile, JSON_KEY_PARENTS);
        json_object *id       = json_object_object_get(currentFile, JSON_KEY_ID);
        json_object *name     = json_object_object_get(currentFile, JSON_KEY_NAME);
        json_object *size     = json_object_object_get(currentFile, "size");
        if (error::is_null({mimeType, parents, id, name}))
        {
            logger::log(STRING_ERROR_PROCESSING, "Malformed or missing data!");
            continue;
        }

        // I still think it's stupid this is an array when there can only be one...
        json_object *parent = json_object_array_get_idx(parents, 0);
        if (error::is_null(parent))
        {
            logger::log(STRING_ERROR_PROCESSING, "Missing parent ID!");
            continue;
        }

        m_list.emplace_back(json_object_get_string(name),
                            json_object_get_string(id),
                            json_object_get_string(parent),
                            size ? json_object_get_uint64(size) : 0,
                            std::strcmp(MIME_TYPE_DIRECTORY, json_object_get_string(mimeType)) == 0);
    }

    return true;
}

bool remote::GoogleDrive::error_occurred(json::Object &json, bool log) noexcept
{
    json_object *error = json::get_object(json, "error");
    if (!error) { return false; }
    const char *errorCode = json_object_get_string(error);
    m_lastError = errorCode ? errorCode : "google_api_error";

    // Google has so many different error response structures. I'm covering two here. Technically,
    // I could grab the code too from the second response, that makes this even more of a headache.
    json_object *description = json::get_object(json, "error_description");
    json_object *message     = json_object_object_get(error, "message");
    if (log && (description || message))
    {
        logger::log("Google Drive error: %s.",
                    description ? json_object_get_string(description) : json_object_get_string(message));
    }

    return true;
}
