#include <windows.h>
#include <winhttp.h>

#include <chrono>
#include <cstring>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#pragma comment(lib, "winhttp.lib")

namespace {

std::string JsonEscape(const std::string& value) {
  std::ostringstream escaped;
  for (const char character : value) {
    switch (character) {
      case '\\':
        escaped << "\\\\";
        break;
      case '"':
        escaped << "\\\"";
        break;
      case '\n':
        escaped << "\\n";
        break;
      case '\r':
        escaped << "\\r";
        break;
      case '\t':
        escaped << "\\t";
        break;
      default:
        escaped << character;
    }
  }
  return escaped.str();
}

std::string BuildReport() {
  const auto now = std::chrono::system_clock::to_time_t(
      std::chrono::system_clock::now());
  std::ostringstream timestamp;
  timestamp << std::put_time(std::gmtime(&now), "%Y-%m-%dT%H:%M:%SZ");

  char computer_name[MAX_COMPUTERNAME_LENGTH + 1] = {};
  DWORD computer_name_length = sizeof(computer_name);
  if (!GetComputerNameA(computer_name, &computer_name_length)) {
    std::cerr << "Unable to determine the computer name.\n";
    return {};
  }

  return "{\"report_type\":\"simplekeylogger-diagnostic\","
         "\"timestamp_utc\":\"" +
         JsonEscape(timestamp.str()) + "\",\"computer_name\":\"" +
         JsonEscape(computer_name) +
         "\",\"message\":\"User-approved diagnostic report\"}";
}

bool Upload(const std::wstring& url, const std::string& token,
            const std::string& report) {
  URL_COMPONENTS components = {};
  components.dwStructSize = sizeof(components);
  wchar_t host[256] = {};
  wchar_t path[2048] = {};
  components.lpszHostName = host;
  components.dwHostNameLength = sizeof(host) / sizeof(host[0]);
  components.lpszUrlPath = path;
  components.dwUrlPathLength = sizeof(path) / sizeof(path[0]);

  if (!WinHttpCrackUrl(url.c_str(), 0, 0, &components) ||
      components.nScheme != INTERNET_SCHEME_HTTPS) {
    std::cerr << "The endpoint must be a valid HTTPS URL.\n";
    return false;
  }

  HINTERNET session = WinHttpOpen(
      L"SimpleKeylogger-DiagnosticUploader/1.0",
      WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
      WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session) {
    std::cerr << "Failed to create an HTTPS session.\n";
    return false;
  }

  HINTERNET connection =
      WinHttpConnect(session, host, components.nPort, 0);
  HINTERNET request = connection
                          ? WinHttpOpenRequest(connection, L"POST", path,
                                               nullptr, WINHTTP_NO_REFERER,
                                               WINHTTP_DEFAULT_ACCEPT_TYPES,
                                               WINHTTP_FLAG_SECURE)
                          : nullptr;

  const std::wstring headers =
      L"Content-Type: application/json\r\nAuthorization: Bearer " +
      std::wstring(token.begin(), token.end());
  const bool sent =
      request &&
      WinHttpSendRequest(request, headers.c_str(),
                         static_cast<DWORD>(-1L),
                         const_cast<char*>(report.data()),
                         static_cast<DWORD>(report.size()),
                         static_cast<DWORD>(report.size()), 0) &&
      WinHttpReceiveResponse(request, nullptr);

  DWORD status_code = 0;
  DWORD status_size = sizeof(status_code);
  if (sent) {
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE |
                                    WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status_code,
                        &status_size, WINHTTP_NO_HEADER_INDEX);
  }

  if (request) {
    WinHttpCloseHandle(request);
  }
  if (connection) {
    WinHttpCloseHandle(connection);
  }
  WinHttpCloseHandle(session);

  if (!sent || status_code < 200 || status_code >= 300) {
    std::cerr << "The server rejected the diagnostic report (HTTP "
              << status_code << ").\n";
    return false;
  }
  return true;
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc != 4 || std::string(argv[1]) != "--endpoint" ||
      std::string(argv[3]) != "--consent") {
    std::cerr << "Usage: diagnostic_uploader.exe --endpoint "
                 "https://host:port/report --consent\n";
    return 2;
  }

  const char* token = std::getenv("SIMPLEKEYLOGGER_DIAGNOSTIC_TOKEN");
  if (!token || std::string(token).empty()) {
    std::cerr << "Set SIMPLEKEYLOGGER_DIAGNOSTIC_TOKEN before starting.\n";
    return 2;
  }

  std::cout << "This sends a generated diagnostic report, not keystrokes or "
               "log.txt, to:\n"
            << argv[2] << "\nContinue? [y/N] ";
  char confirmation = '\0';
  std::cin >> confirmation;
  if (confirmation != 'y' && confirmation != 'Y') {
    std::cout << "Upload cancelled.\n";
    return 0;
  }

  const std::string report = BuildReport();
  if (report.empty() ||
      !Upload(std::wstring(argv[2], argv[2] + std::strlen(argv[2])),
              token, report)) {
    return 1;
  }

  std::cout << "Diagnostic report uploaded successfully.\n";
  return 0;
}
