#include <iostream>
#include <thread>
#include <fstream>
#include <iomanip>
#include <cstdlib>

#include <string> // library untuk mensupport penggunaan string dan berkontribusi atas fitur pengganti nama file

#include <chrono> // library buat waktu real time

#include <filesystem> // library yang berguna banget buat manage dan lain lain dah pokoknya, pake library ini pastikan atur bahsa c++ ke 17/20

#include <curl/curl.h> // install melalui vcpkg >> baca dokumentasi

#include <Windows.h> // kedua library berfungsi untuk membaca folder download default user
#include <Shlobj.h> 

#define _CRT_SECURE_NO_WARNINGS // bypass warning attention
#include <ctime> // local time

#include <Shlwapi.h> // untuk menggunakan tipe data LPWSTR
#include <Windows.h>
#pragma comment(lib, "Shell32.lib") // untuk menggunakan fungsi ShellExecute
#pragma comment(lib, "Shlwapi.lib")

#include "LoadingPage.h" // file header buatan sendiri buat loading page

namespace fs = std::filesystem;

struct DownloadProgress { // progress download
    double last_progress;
};

std::string get_download_directory() { // fungsi untuk membaca folder download default user lalu menaruh hasil donwload di sana
    PWSTR path;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, NULL, &path))) {
        std::wstring wpath(path);
        CoTaskMemFree(path);
        return std::string(wpath.begin(), wpath.end()) + "\\";
    }
    else {
        // fallback jika gagal mendapatkan direktori default
        return "\t\t\t C:\\Users\\Administrator\\Downloads\\";
    }
}

void log_download_history(const std::string& filename) { // function untuk membuat log download
    // dapatkan waktu saat ini
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    // ubah waktu saat ini menjadi waktu lokal
    struct std::tm timeinfo;
    localtime_s(&timeinfo, &now);

    // format waktu ke dalam string
    char time_buffer[80]; // buffer untuk menyimpan waktu dalam string
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);

    // simpan waktu dan nama file ke dalam riwayat unduhan
    std::ofstream history_file("download_history.txt", std::ios::app); // buka file riwayat untuk ditambahkan
    if (history_file.is_open()) {
        history_file << "Downloaded: " << filename << " at " << time_buffer << std::endl;
        history_file.close(); // tutup file riwayat
    }
    else {
        std::cerr << "Gagal membuka file riwayat unduhan!" << std::endl;
    }
}

bool is_download_directory_available(const std::string& directory) { // untuk memastikan kalo tempat penyimpanan file tersedia
    return fs::is_directory(directory);
}

bool file_exists(const std::string& filepath) { // ini untuk memastikan file ada dan nanti akan membantu untuk mengganti nama file yang sama
    return fs::exists(filepath);
}


bool delete_file(const std::string& filepath) { // fitur untuk user menghapus file
    if (std::remove(filepath.c_str()) != 0) {
        std::cerr << "\t\t\t       Gagal menghapus file..." << std::endl;
        return false;
    }
    else {
        std::cout << "\t\t\t       File berhasil dihapus." << std::endl;
        return true;
    }
}

bool rename_file(const std::string& old_filepath, const std::string& new_filepath) { // fitur untuk user mengganti nama file
    if (std::rename(old_filepath.c_str(), new_filepath.c_str()) != 0) {
        std::cerr << "\t\t\t       Gagal mengganti nama file..." << std::endl;
        return false;
    }
    else {
        std::cout << "\t\t\t      Nama file berhasil diganti." << std::endl;
        return true;
    }
}

std::string get_unique_filename(const std::string& filepath) { // nama file akan terganti supaya tidak double,
    std::string unique_filepath = filepath;                    // contoh jika sudah ada vscode_installer.exe lalu user download lagi nanti akan            
    int count = 1;                                             // menjadi vscode_installer_1.exe begitu dan seterusnya
    while (file_exists(unique_filepath)) {
        size_t pos = unique_filepath.rfind(".");
        if (pos != std::string::npos) {
            unique_filepath.insert(pos, "_" + std::to_string(count++));
        }
        else {
            unique_filepath += "_" + std::to_string(count++);
        }
    }
    return unique_filepath;
}

size_t write_data(void* ptr, size_t size, size_t nmemb, std::ofstream& stream) { // ini untuk memastikan ukuran file supaya tidak terjadi bug
    stream.write(static_cast<const char*>(ptr), size * nmemb);                   // saat bar presentasenya
    return size * nmemb;
}

int progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    DownloadProgress* progress = static_cast<DownloadProgress*>(clientp);     // waktu saat download
    double current_progress = (dlnow > 0) ? (dlnow * 100.0 / dltotal) : 0.0;
    double speed = 0.0;
    static auto start_time = std::chrono::steady_clock::now();
    auto current_time = std::chrono::steady_clock::now();
    auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();

    if (elapsed_seconds > 0) {
        speed = static_cast<double>(dlnow) / static_cast<double>(elapsed_seconds) / 1024.0;
    }

    if (current_progress - progress->last_progress >= 1.0) { // bar presentase download
        auto seconds_left = static_cast<int>((100 - current_progress) / (current_progress / elapsed_seconds));
        system("cls");
        std::cout << "\r\033[32mDownloading... ["
            << std::string(static_cast<int>(current_progress) / 2, '#')
            << std::string(50 - static_cast<int>(current_progress) / 2, ' ') << "] ";
        std::cout << std::fixed << std::setprecision(2) << current_progress << "% "
            << "Speed: " << std::setw(6) << std::setfill(' ') << std::fixed << std::setprecision(2) << speed << " KB/s   ";
        std::cout << "Time Left: " << std::setw(3) << seconds_left / 3600 << "h" << std::setw(2) << (seconds_left % 3600) / 60 << "m   \033[0m" << std::flush;
        progress->last_progress = current_progress;
    }

    return 0;
}

static bool download_file(const std::string& url, const std::string& output_filename) { // ini jika direktori tidak tersedia
    std::string download_directory = get_download_directory();
    if (!is_download_directory_available(download_directory)) {
        std::cerr << "Direktori tidak tersedia..." << std::endl;
        return false;
    }

    std::string output_path = download_directory + output_filename;
    output_path = get_unique_filename(output_path);

    std::ofstream outputFile(output_path, std::ios::binary); // error ketika gagal membuat direktori
    if (!outputFile) {
        std::cerr << "Gagal membuat file di direktori..." << std::endl;
        return false;
    }

    CURL* curl_handle = curl_easy_init(); // ini pesan error kalo libcurl kenapa - kenapa, soalnya agak agak awalnya nih library
    if (!curl_handle) {
        std::cerr << "Gagal menginisialisasi libcurl..." << std::endl;
        outputFile.close();
        return false;
    }

    curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &outputFile);
    curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl_handle, CURLOPT_XFERINFOFUNCTION, progress_callback);

    DownloadProgress progress;
    progress.last_progress = 0.0;
    curl_easy_setopt(curl_handle, CURLOPT_XFERINFODATA, &progress);

    CURLcode res = curl_easy_perform(curl_handle); // errror ketika gagal melakukan unduhan
    if (res != CURLE_OK) {
        std::cerr << "Gagal melakukan unduhan: " << curl_easy_strerror(res) << std::endl;
        outputFile.close();
        curl_easy_cleanup(curl_handle);
        return false;
    }

    curl_easy_cleanup(curl_handle);
    outputFile.close();

    std::cout << std::endl;
    system("cls");

    return true;
}

void BarInstall(int progress, int elapsed_seconds) { // fungsi untuk menampilkan progress bar install
    system("cls");

    
    std::cout << "\033[34m."; // escape code untuk warna biru
    std::cout << "\r\t\t\tInstalling... [";
    int pos = progress / 2;
    for (int i = 0; i < 50; ++i) {
        if (i < pos) std::cout << "#";
        else if (i == pos) std::cout << "#";
        else std::cout << " ";
    }
    std::cout << "] " << progress << "% ";
    std::cout << "Time: " << elapsed_seconds << "s   ";
    std::cout.flush();

    // mengatur warna teks kembali ke normal
    std::cout << "\033[0m"; // escape code untuk reset warna
}

void installAutomatically(const std::string& filePath) { // fungsi untuk fitur menginstal file secara otomatis
    std::cout << "Menginstal otomatis file: " << filePath << std::endl;

    // konversi std::string ke LPWSTR
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), static_cast<int>(filePath.size()), NULL, 0);
    std::wstring widePath(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), static_cast<int>(filePath.size()), &widePath[0], size_needed);

    // tentukan command line parameters berdasarkan jenis file
    std::wstring commandLineParams;
    if (filePath.substr(filePath.size() - 4) == ".exe") {
        // tambahkan parameter untuk silent install pada file .exe
        commandLineParams = L" /SILENT /VERYSILENT /s /quiet /norestart";
    }
    else if (filePath.substr(filePath.size() - 4) == ".msi") {
        // tambahkan parameter untuk silent install pada file .msi (jika ada)
        commandLineParams = L" /quiet /qn /norestart";
    }

    // proses untuk menjalankan installer dengan parameter
    SHELLEXECUTEINFO shExecInfo = { 0 };
    shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
    shExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
    shExecInfo.hwnd = NULL;
    shExecInfo.lpVerb = L"open";
    shExecInfo.lpFile = widePath.c_str();
    shExecInfo.lpParameters = commandLineParams.c_str();
    shExecInfo.lpDirectory = NULL;
    shExecInfo.nShow = SW_SHOWNORMAL;
    shExecInfo.hInstApp = NULL;

    if (ShellExecuteEx(&shExecInfo) == FALSE || shExecInfo.hProcess == NULL) {
        std::cout << "Gagal menjalankan installer." << std::endl;
        return;
    }

    // tampilkan progress instalasi
    auto start_time = std::chrono::steady_clock::now();
    std::thread progressThread([&]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500)); // delay untuk update progress setiap 0.5 detik

            // dapatkan informasi progress instalasi (misalnya, jika file .exe sudah selesai dieksekusi)
            DWORD exitCode;
            if (GetExitCodeProcess(shExecInfo.hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
                break;
            }

            // hitung waktu yang sudah berlalu (informasi time left)
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();

            // perkiraan progress (di sini kita mengasumsikan total waktu instalasi maksimum adalah 60 detik) > masih mencari methode yang perfek
            int progress = static_cast<int>((elapsed_seconds * 100) / 60);
            if (progress > 100) progress = 100; // batasi progress hingga 100%
            BarInstall(progress, elapsed_seconds);
        }
        });

    // tunggu proses instalasi selesai
    WaitForSingleObject(shExecInfo.hProcess, INFINITE);
    DWORD exitCode;
    GetExitCodeProcess(shExecInfo.hProcess, &exitCode);

    // hentikan thread progress
    progressThread.join();

    if (exitCode == 0) {
        std::cout << std::endl << "Instalasi selesai." << std::endl;
    }
    else {
        std::cout << std::endl << "Gagal melakukan instalasi, kode kesalahan: " << exitCode << std::endl;
    }

    CloseHandle(shExecInfo.hProcess);
}

void display_browser_menu() {
    system("cls");
    std::cout << "\t\t\t           ==================================  " << std::endl;
    std::cout << "\t\t\t                     MENU BROWSER                 " << std::endl;
    std::cout << "\t\t\t           ==================================  " << std::endl << std::endl;

    std::cout << "\t\t\t             1. Google Chrome" << std::endl;
    std::cout << "\t\t\t             2. Microsoft Edge" << std::endl;
    std::cout << "\t\t\t             3. Opera GX Browser" << std::endl;

    std::cout << "\n\t\t\t             Masukkan nomor pilihan Anda:  ";
}

void display_social_media_menu() {
    system("cls");
    std::cout << "\t\t\t           ==================================  " << std::endl;
    std::cout << "\t\t\t                  MENU SOSIAL MEDIA             " << std::endl;
    std::cout << "\t\t\t           ==================================  " << std::endl << std::endl;

    std::cout << "\t\t\t             4. Discord Desktop" << std::endl;
    std::cout << "\t\t\t             5. Instagram Desktop" << std::endl;
    std::cout << "\t\t\t             6. WhatsApp Desktop" << std::endl;
    std::cout << "\t\t\t             7. Telegram Desktop" << std::endl;

    std::cout << "\n\t\t\t             Masukkan nomor pilihan Anda:  ";
}

void display_coding_tools_menu() {
    system("cls");
    std::cout << "\t\t\t           ==================================  " << std::endl;
    std::cout << "\t\t\t                  MENU CODING TOOLS             " << std::endl;
    std::cout << "\t\t\t           ==================================  " << std::endl << std::endl;

    std::cout << "\t\t\t             8. Visual Studio Code" << std::endl;
    std::cout << "\t\t\t             9. Visual Studio" << std::endl;
    std::cout << "\t\t\t            10. Intellij IDEA" << std::endl;
    std::cout << "\t\t\t            11. Android Studio\n" << std::endl;

    std::cout << "\n\t\t\t             Masukkan nomor pilihan Anda:  ";
}

void display_vpn_menu() {
    system("cls");
    std::cout << "\t\t\t           ==================================  " << std::endl;
    std::cout << "\t\t\t                     MENU VPN                    " << std::endl;
    std::cout << "\t\t\t           ==================================  " << std::endl << std::endl;

    std::cout << "\t\t\t             12. Psiphon" << std::endl;
    std::cout << "\t\t\t             13. Turbo VPN" << std::endl;
    std::cout << "\t\t\t             14. Proton VPN\n" << std::endl;

    std::cout << "\n\t\t\t             Masukkan nomor pilihan Anda:  ";
}

void display_media_player_menu() {
    system("cls");
    std::cout << "\t\t\t           ==================================  " << std::endl;
    std::cout << "\t\t\t                  MENU MEDIA PLAYER             " << std::endl;
    std::cout << "\t\t\t           ==================================  " << std::endl << std::endl;

    std::cout << "\t\t\t             15. VLC Media Player" << std::endl;
    std::cout << "\t\t\t             16. PotPlayer" << std::endl;
    std::cout << "\t\t\t             17. Winamp" << std::endl;
    std::cout << "\t\t\t             18. Windows Media Player\n" << std::endl;

    std::cout << "\n\t\t\t             Masukkan nomor pilihan Anda:  ";
}

void display_developer_information() { // informasi developer biar keren aja wkwk
    std::string developer_name = "Wisnu Rafi";
    std::string github_url = "github.com/wsnrfidev";
    std::string instagram_handle = "wisnurafi_";

    std::cout << "\t\t\t           Terimakasih sudah menggunakan tools ini  " << std::endl;
    std::cout << "\t\t\t      Kritik dan Saran anda perlu untuk saya terus berkembang " << std::endl;
    std::cout << "\t\t\t           =======================================  " << std::endl;
    std::cout << "\t\t\t                    DEVELOPER INFORMATION                " << std::endl;
    std::cout << "\t\t\t           =======================================  " << std::endl;
    std::cout << "\t\t\t             Developer Name   : " << developer_name << std::endl;
    std::cout << "\t\t\t             Github           : " << github_url << std::endl;
    std::cout << "\t\t\t             Instagram        : " << instagram_handle << std::endl << std::endl;
}


void display_download_information(const std::string& filename, bool success) {
    std::string download_directory = get_download_directory();

    std::cout << "\t\t\t              ----------------------------------  " << std::endl;
    std::cout << "\t\t\t                     DOWNLOAD INFORMATION  " << std::endl;
    std::cout << "\t\t\t              ----------------------------------  " << std::endl;
    std::cout << "\t\t\t           File berhasil diunduh ke direktori: " << std::endl << "\t\t\t\t      " << download_directory << std::endl;

    if (success)
        std::cout << "\r\t\t\t           Berhasil Mengunduh. Unduhan Selesai..." << std::endl;
    else
        std::cout << "\r\t\t\t           Gagal melakukan unduhan..." << std::endl;
}

void open_downloaded_file(const std::string& filepath) { // fitur untuk membuka file yang sudah didownload 
    system(("start " + filepath).c_str());
}

void manage_downloads() {
    system("cls");
    std::cout << "\t\t\t           ==================================  " << std::endl;
    std::cout << "\t\t\t                 MANAGEMENT DOWNLOAD               " << std::endl;
    std::cout << "\t\t\t           ==================================  " << std::endl << std::endl;

    std::string download_directory = get_download_directory();
    if (!is_download_directory_available(download_directory)) {
        std::cerr << "Direktori tidak tersedia..." << std::endl;
        return;
    }

    std::cout << "\t\t\t          No.    |    Filename" << std::endl;
    std::cout << "\t\t\t         -------------------------------------" << std::endl;
    int count = 1;
    std::vector<std::string> files; // menyimpan nama file dalam vector
    for (const auto& entry : fs::directory_iterator(download_directory)) {
        std::cout << "\t\t\t      " << std::setw(6) << count++ << "\t |  " << entry.path().filename() << std::endl;
        files.push_back(entry.path().filename().string());
    }

    std::cout << "\n\t\t\t         Pilih nomor file yang ingin dikelola (0 untuk kembali): ";
    int choice;
    std::cin >> choice;

    if (choice == 0) {
        return;
    }

    if (choice > 0 && choice <= files.size()) {
        std::string selected_file = files[choice - 1];
        std::string file_path = download_directory + selected_file;
        system("cls");
        std::cout << "\t\t\t           ==================================  " << std::endl;
        std::cout << "\t\t\t                   MANAGEMENT FILE              " << std::endl;
        std::cout << "\t\t\t           ==================================  " << std::endl << std::endl;
        std::cout << "\n\t\t\t             Pilih operasi yang ingin dilakukan:" << std::endl;
        std::cout << "\t\t\t             1. Hapus file" << std::endl;
        std::cout << "\t\t\t             2. Ganti nama file" << std::endl;
        std::cout << "\t\t\t             0. Keluar" << std::endl;
        std::cout << "\n\t\t\t             Masukkan nomor pilihan Anda: ";
        int operation_choice;
        std::cin >> operation_choice;

        switch (operation_choice) {
        case 1:
            delete_file(file_path);
            break;
        case 2: {
            std::string new_filename;
            std::cout << "\n\t\t\t             Masukkan nama baru untuk file: ";
            std::cin.ignore(); // membersihkan buffer input
            std::getline(std::cin, new_filename);
            std::string new_file_path = download_directory + new_filename;
            rename_file(file_path, new_file_path);
            break;
        }
        default:
            std::cerr << "\n\t\t\t             Pilihan tidak valid..." << std::endl;
            break;
        }
    }
    else {
        std::cerr << "\n\t\t\t             Pilihan tidak valid..." << std::endl;
    }
}

void display_download_history() { // fitur untuk user melihat history download yang sudah disimpan di log
    system("cls");
    std::cout << "\t\t\t           ==================================  " << std::endl;
    std::cout << "\t\t\t                   HISTORY DOWNLOAD                 " << std::endl;
    std::cout << "\t\t\t           ==================================  " << std::endl << std::endl;

    std::ifstream historyFile("download_history.txt");
    if (historyFile) {
        std::string line;
        while (std::getline(historyFile, line)) {
            std::cout << "\t\t\t   " << line << std::endl;
        }
        historyFile.close();
    }
    else {
        std::cerr << "Gagal membaca file riwayat unduhan..." << std::endl;
    }

    std::cout << "\n\t\t\t   Tekan Enter untuk kembali ke menu utama...";
    std::cin.ignore();
    std::cin.get();
}


int main() {

    SetConsoleTitle(TEXT("Download Tools V2 | WSNRFIDEV"));

    F_Loading(); // loading page

    int choice;
    do {
        system("cls");
        std::cout << "\t\t\t           ==================================  " << std::endl;
        std::cout << "\t\t\t                       MENU DOWNLOAD                   " << std::endl;
        std::cout << "\t\t\t           ==================================  " << std::endl << std::endl;

        std::cout << "\t\t\t             1. Browser" << std::endl;
        std::cout << "\t\t\t             2. Sosial Media" << std::endl;
        std::cout << "\t\t\t             3. Coding Tools" << std::endl;
        std::cout << "\t\t\t             4. VPN" << std::endl;
        std::cout << "\t\t\t             5. Media Player\n" << std::endl;
        std::cout << "\t\t\t           ---------------------------------- " << std::endl;
        std::cout << "\t\t\t             6. Manage Download" << std::endl;
        std::cout << "\t\t\t             7. History Download" << std::endl;
        std::cout << "\t\t\t           ---------------------------------- " << std::endl;
        std::cout << "\t\t\t             0. Exit Program" << std::endl;

        std::cout << "\n\t\t\t             Masukkan nomor pilihan Anda:  ";
        std::cin >> choice;

        // logika untuk eksekusi menu utama
        switch (choice) {
        case 1:
            display_browser_menu();
            break;
        case 2:
            display_social_media_menu();
            break;
        case 3:
            display_coding_tools_menu();
            break;
        case 4:
            display_vpn_menu();
            break;
        case 5:
            display_media_player_menu();
            break;
        case 6:
            manage_downloads(); // display menu manajemen download
            std::cout << "\n\t\t\t        Tekan Enter untuk kembali ke menu utama...";
            std::cin.ignore();
            std::cin.get();
            continue;
        case 7:
            display_download_history(); // menampilkan riwayat unduhan
            continue;
        case 0:
            system("cls");
            std::cout << "\t\t\t           ==================================  " << std::endl;
            std::cout << "\t\t\t                      EXIT PROGRAM                   " << std::endl;
            std::cout << "\t\t\t           ==================================  " << std::endl << std::endl;
            std::cout << "\t\t\t                   Menutup program..." << std::endl;
            return 0;
        default:
            std::cerr << "\n\t\t\t             Pilihan tidak valid..." << std::endl;
            std::cout << "\n\t\t\t             Tekan Enter untuk memulai ulang program..." << std::endl;
            std::cin.ignore();
            std::cin.get();
            continue;
        }


        // ini logika untuk eksekusi pililhan sub menu > still undermaintenance
        int submenu_choice;
        std::cin >> submenu_choice;

        std::string url, filename;

        switch (submenu_choice) {
        case 1:
            url = "https://dl.google.com/tag/s/appguid%3D%7B8A69D345-D564-463C-AFF1-A69D9E530F96%7D%26iid%3D%7BB1F81197-15E2-9FAC-4C7F-A86D6E2974D1%7D%26lang%3Den%26browser%3D3%26usagestats%3D0%26appname%3DGoogle%2520Chrome%26needsadmin%3Dprefers%26ap%3Dx86-stable-statsdef_1%26installdataindex%3Dempty/chrome/install/ChromeStandaloneSetup.exe";
            filename = "chrome_installer.exe";
            break;
        case 2:
            url = "https://c2rsetup.officeapps.live.com/c2r/downloadEdge.aspx?ProductreleaseID=Edge&platform=Default&version=EdgeStable&source=EdgeWebview&Channel=Stable&language=en";
            filename = "edge_installer.exe";
            break;
        case 3:
            url = "https://get.opera.com/ftp/pub/opera_gx/109.0.5097.70/win/Opera_GX_109.0.5097.70_Setup.exe";
            filename = "opera_gx_installer.exe";
            break;
        case 4:
            url = "https://dl.discordapp.net/distro/app/stable/win/x86/1.0.9045/DiscordSetup.exe";
            filename = "discord_installer.exe";
            break;
        case 5:
            url = "https://softfamous.com/postdownload-file/instagram/88606/12945/"; // update
            filename = "instagram_installer.exe";
            break;
        case 6:
            url = "https://us.softradar.com/static/products/whatsapp/distr/0.2.1880/whatsapp_softradar-com.exe";
            filename = "whatsapp_installer.exe";
            break;
        case 7:
            url = "https://telegram.download-program.ru/telegram-for-windows-download"; // update
            filename = "telegram_installer.exe";
            break;
        case 8:
            url = "https://vscode-update.azurewebsites.net/latest/win32/stable"; // update
            filename = "vscode_installer.exe";
            break;
        case 9:
            url = "https://www.visualstudio.com/thank-you-downloading-visual-studio/?sku=Community&rel=15"; // update
            filename = "visualstudio_installer.exe";
            break;
        case 10:
            url = "https://github.com/JetBrains/intellij-community/archive/refs/heads/master.zip"; // update
            filename = "intellij_installer.exe";
            break;
        case 11:
            url = "https://redirector.gvt1.com/edgedl/android/studio/install/2023.3.1.18/android-studio-2023.3.1.18-windows.exe"; // update
            filename = "androidstudio_installer.exe";
            break;
        case 12:
            url = "https://www.samarindavip.com/download/2";
            filename = "psiphon_installer.exe";
            break;
        case 13:
            url = "https://storage.googleapis.com/windows_bucket1/turbo/download/softpedia/TurboVPN_setup.exe";
            filename = "turbo_vpn_installer.zip";
            break;
        case 14:
            url = "https://protonvpn.com/download/ProtonVPN_v3.2.11.exe";
            filename = "proton_vpn_installer.exe";
            break;
        case 15:
            url = "https://get.microsoft.com/installer/download/9nblggh4vvnh?hl=en-us&gl=us&referrer=storeforweb";
            filename = "vlc_installer.exe";
            break;
        case 16:
            url = "https://t1.daumcdn.net/potplayer/PotPlayer/Version/Latest/PotPlayerSetup.exe";
            filename = "potplayer_installer.exe";
            break;
        case 17:
            url = "https://download1078.mediafire.com/o1rfkzo61l2gtAXLd0_suQ19L9IE1jILnXUzXG9aHmogs-30qQn2q7xX6J_Fw6PrrZNjahLrCPXg4iL1j7JlQGeoILbwCrWjskqPNawTZEoCyllUZeHfD70F1NxHRYe6h0GaQ1T3pP3MCz6oAjd1N2WN3Lv24xmL-ftKAiuqk4s/uc5vyqj1zggk37q/Winamp+v5.9.0+Build+9999+RC1.rar";
            filename = "winamp_installer.rar";
            break;
        case 18:
            url = "https://www.windows-media-player.com/downloads/Windows-Media-Player-7.rar";
            filename = "windows_media_player_installer.rar";
            break;
        default:
            std::cerr << "\n\t\t\t             Pilihan tidak valid..." << std::endl;
            std::cout << "\n\t\t\t             Tekan Enter untuk memulai ulang program..." << std::endl;
            std::cin.ignore();
            std::cin.get();
            continue;
        }

        bool success = download_file(url, filename);

        system("cls");

        // menampilkan informasi developer dan informasi download

        std::string download_directory = get_download_directory();
        std::string filepath = download_directory + filename;

        //display_developer_information();
        //display_download_information(filename, success);

        if (success) {
            log_download_history(filename); // memasukkan unduhan ke dalam riwayat
            std::string filepath = download_directory + filename;

            // tampilkan pesan ke user 
            std::cout << "\t\t\t    Apakah Anda ingin menginstall software yang baru saja diunduh? (y/t): ";
            std::string response;
            std::cin >> response;

            if (response == "y") {
                // install file yang sudah diunduh jika user memilih ya
                installAutomatically(filepath);                      
            } 
            if (response == "t")
            {
                // hanya membuka file yang sudah diunduh > install manual oleh user
                open_downloaded_file(filepath);
            }

            system("cls");

            // menampilkan informasi developer dan informasi unduhan
            display_developer_information();
            display_download_information(filename, success);
        }

        break; // close setelah mendownload software

    } while (choice != 0);

    return 0;
}
