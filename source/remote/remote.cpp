#include "remote/remote.hpp"
#include "AppPaths.hpp"
#include "AtomicFile.hpp"
#include "TextSafety.hpp"
#include "StateManager.hpp"
#include "appstates/MessageState.hpp"
#include "appstates/TaskState.hpp"
#include "keyboard/keyboard.hpp"
#include "logging/logger.hpp"
#include "remote/GoogleDrive.hpp"
#include "remote/UploadQueue.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

namespace
{
    std::unique_ptr<remote::Storage> s_storage;
    std::atomic_bool s_cancel{};
    std::mutex s_statusMutex;
    std::string s_status = "Google Drive: desconectado";
    void set_status(std::string message)
    {
        std::lock_guard guard(s_statusMutex);
        s_status = savenx::plain_text(message, 105);
    }
    bool enter(remote::Storage *storage, const std::string &name)
    {
        if (!storage->directory_exists(name) && !storage->create_directory(name)) { return false; }
        const auto *dir = storage->get_directory_by_name(name);
        if (!dir) { return false; }
        storage->change_directory(dir);
        return true;
    }
    void retry_task(sys::threadpool::JobData data)
    {
        const auto taskData = std::static_pointer_cast<sys::Task::DataStruct>(data);
        if (!remote::get_remote_storage())
        { set_status("Conecte o Google Drive antes de reenviar."); taskData->task->complete(); return; }
        remote::flush_pending();
        taskData->task->complete();
    }
}

bool remote::has_internet_connection() noexcept
{
    NifmInternetConnectionType type{};
    uint32_t strength{};
    NifmInternetConnectionStatus state{};
    return R_SUCCEEDED(nifmGetInternetConnectionStatus(&type, &strength, &state)) &&
           state == NifmInternetConnectionStatus_Connected;
}

std::string remote::status()
{
    std::lock_guard guard(s_statusMutex);
    return s_status;
}

std::size_t remote::pending_count()
{
    // Markers are published atomically; partial writes never appear as ready jobs.
    return UploadQueue(std::filesystem::path(savenx::QUEUE_ROOT)).list().size();
}

void remote::cancel_sign_in() noexcept { s_cancel = true; }

remote::Storage *remote::get_remote_storage() noexcept
{
    return s_storage && s_storage->is_initialized() ? s_storage.get() : nullptr;
}

void remote::connect()
{
    s_cancel = false;
    TaskState::create_push_fade(remote::initialize, std::make_shared<sys::Task::DataStruct>());
}

void remote::retry_pending()
{
    TaskState::create_push_fade(retry_task, std::make_shared<sys::Task::DataStruct>());
}

void remote::configure_client()
{
    std::array<char, 256> id{}, secret{};
    if (!keyboard::get_input(SwkbdType_Normal, "", "ID do cliente OAuth Google", id.data(), id.size())) { return; }
    if (!std::string_view(id.data()).ends_with(".apps.googleusercontent.com"))
    { MessageState::create_push_fade("ID OAuth inválido. Use um cliente para TVs e dispositivos com entrada limitada."); return; }
    if (!keyboard::get_input(SwkbdType_Normal, "", "Client secret OAuth (não é a senha Google)", secret.data(), secret.size())) { return; }
    auto config = json::new_object(json_object_new_object);
    auto installed = json_object_new_object();
    json_object_object_add(installed, "client_id", json_object_new_string(id.data()));
    json_object_object_add(installed, "client_secret", json_object_new_string(secret.data()));
    json_object_object_add(config.get(), "installed", installed);
    if (!savenx::atomic_write(std::filesystem::path(PATH_GOOGLE_DRIVE_CONFIG), json_object_to_json_string_ext(config.get(), JSON_C_TO_STRING_PRETTY)))
    { MessageState::create_push_fade("Não foi possível salvar a configuração."); return; }
    s_storage.reset();
    set_status("Cliente configurado. Selecione Conectar Google Drive.");
}

void remote::initialize(sys::threadpool::JobData data)
{
    const auto taskData = std::static_pointer_cast<sys::Task::DataStruct>(data);
    sys::Task *task = taskData ? taskData->task : nullptr;
    if (!task) { return; }
    if (!has_internet_connection())
    { set_status("Sem internet. Os backups locais continuam disponíveis."); task->complete(); return; }
    task->set_status("Conectando ao Google Drive...");
    s_storage = std::make_unique<GoogleDrive>();
    auto *drive = static_cast<GoogleDrive *>(s_storage.get());
    if (!drive->has_client_configuration())
    {
        set_status("Configure o cliente Google para habilitar a nuvem.");
        task->complete(); return;
    }
    if (drive->sign_in_required())
    {
        std::string message, code;
        std::time_t expiration{};
        int interval{};
        if (!drive->get_sign_in_data(message, code, expiration, interval))
        { set_status("Falha na autorização: " + drive->last_error()); task->complete(); return; }
        task->set_status(message);
        interval = std::clamp(interval, 2, 60);
        while (!s_cancel && std::time(nullptr) < expiration)
        {
            for (int i = 0; i < interval * 10 && !s_cancel; ++i)
            { std::this_thread::sleep_for(std::chrono::milliseconds(100)); }
            if (s_cancel) { break; }
            if (drive->poll_sign_in(code)) { break; }
            switch (drive->poll_state())
            {
                case GoogleDrive::PollState::SlowDown: interval = std::min(interval + 5, 60); break;
                case GoogleDrive::PollState::Denied:
                case GoogleDrive::PollState::Expired:
                case GoogleDrive::PollState::Error:
                    set_status("Autorização: " + drive->last_error()); task->complete(); return;
                default: break;
            }
        }
        if (!drive->is_initialized())
        {
            set_status(s_cancel ? "Autorização cancelada." : "Código expirou. Conecte novamente.");
            task->complete(); return;
        }
    }
    if (!drive->directory_exists("SaveNX") && !drive->create_directory("SaveNX"))
    { set_status("Falha ao preparar a pasta SaveNX no Drive."); s_storage.reset(); task->complete(); return; }
    const auto *root = drive->get_directory_by_name("SaveNX");
    if (!root)
    { set_status("Não foi possível localizar a pasta SaveNX."); s_storage.reset(); task->complete(); return; }
    drive->set_root_directory(root);
    drive->change_directory(root);
    set_status("Google Drive conectado.");
    task->set_status("Google Drive conectado. Verificando uploads pendentes...");
    // Flush runs inside this task. No worker ever mutates the UI state stack.
    flush_pending();
    task->complete();
}

bool remote::enqueue_backup(const fslib::Path &source, std::string_view user,
                            std::string_view title, std::string_view name)
{
    const bool queued = UploadQueue(std::filesystem::path(savenx::QUEUE_ROOT)).enqueue(
        source.string(), std::string(user), std::string(title), std::string(name));
    if (!queued) { set_status("Backup local salvo; falha ao registrar envio pendente."); }
    else { set_status("Backup local salvo; envio registrado na fila."); }
    return queued;
}

void remote::flush_pending(sys::ProgressTask *task)
{
    auto *storage = get_remote_storage();
    if (!storage || !has_internet_connection()) { return; }
    UploadQueue queue{std::filesystem::path(savenx::QUEUE_ROOT)};
    std::size_t uploaded{};
    for (const auto &job : queue.list())
    {
        storage->return_to_root();
        if (!enter(storage, job.user) || !enter(storage, job.title)) { break; }
        if (task) { task->set_status("Enviando backup pendente..."); }
        if (!storage->upload_file(fslib::Path(job.zip), job.name, task))
        { set_status("Backup local preservado. Há envios pendentes."); return; }
        if (!queue.acknowledge(job))
        { set_status("Enviado ao Drive; pendência será verificada novamente."); return; }
        ++uploaded;
    }
    storage->return_to_root();
    if (queue.list().empty()) { set_status("Google Drive conectado. Todos os envios concluídos."); }
    else if (uploaded) { set_status("Parte dos backups foi enviada. Ainda há pendências."); }
    else { set_status("Google Drive conectado. Há envios pendentes."); }
}
