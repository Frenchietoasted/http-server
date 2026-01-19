// run using 'gcc server.c -o server -lws2_32 -lmswsock' then ./server
#include <stdio.h>
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <time.h>

#define FILES_PATH "C:\\Users\\Steve Aaron Dmello\\.vscode\\C_files\\https\\files" // change for pc because stupid
#define FOLDER_DEPTH 2                                                             // how many subfolders

void sendError(SOCKET, int);
void serveClient(SOCKET);
HANDLE getFile(SOCKET, char *);
int sendFile(SOCKET, HANDLE, char *);
const char *getMIME(char *);

char *correctRoute(char *);
int handleRequest(SOCKET, char[]);
int handleFile(SOCKET, char *);
char *handleRoute(char *);

LPWSTR CharToLPWSTR(char *);

int main()
{
    WSADATA wsa;
    int count = 1;
    
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        WSACleanup();
        // printf("\nDidn't initialize properly, exiting");
        return 1;
    }
    struct sockaddr_in addr = {
        addr.sin_family = AF_INET,
        addr.sin_port = htons(8080),
        addr.sin_addr.s_addr = INADDR_ANY,
    };
    // Creating the socket
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // ipv4 address family, simple TCP socket , TCP/IP protocol

    bind(s, (struct sockaddr *)&addr, sizeof(addr));

    listen(s, 20);

    // printf("Server is listening at http://127.0.0.1:8080/ Press Ctrl+c to stop \n");

    while (1)
    {
        serveClient(s);
        printf("\n%d request Done\n", count++);
    }

    int err = WSAGetLastError(); // Check for errors if any
    if (err != 0)
         printf("\n Error!\nStatus code: %d", err);
    else
         printf("\nClosed server.");

    closesocket(s);
    WSACleanup();
    return 0;
}

void serveClient(SOCKET s)
{
    SOCKET client = accept(s, NULL, NULL);
    clock_t start_t,stop_t;
    start_t=clock();
    if (client == INVALID_SOCKET)
    {
        printf("accept failed: %d\n", WSAGetLastError());
        return;
    }

    char buffer[1024] = {0};
    int recvBytes;
    recvBytes = recv(client, buffer, 1024, 0);
    // printf("\nBytes recieved:%d\n", recvBytes);
    if (recvBytes <= 0)
        return;
    handleRequest(client, buffer);

    shutdown(client, SD_BOTH);
    closesocket(client);
    stop_t=clock();
    printf("Time take for request:%f",(double)(stop_t-start_t)/(CLOCKS_PER_SEC));
}

int handleRequest(SOCKET client, char buffer[])
{
    // printf("%s", buffer);
    // char *connection = strstr(buffer, "Connection:");

    char *route = strchr(buffer, '/');
    char *endRoute = strchr(route, ' ');

    int returnStatus;
    if (route == NULL || endRoute == NULL)
    {
        printf("Could not parse.");
        sendError(client, 404);
        return 1;
    }
    *endRoute = '\0'; // Terminating the route
    printf("HTTPS route:", route);
    char file[sizeof(FILES_PATH) + 100] = FILES_PATH;
    char *adjustedRoute = handleRoute(route);
    if (adjustedRoute == NULL)
    {
        printf("\nFile not found.");
        // sendError(client,404);
        return 1;
    }
    strcat(file, adjustedRoute);
    printf("\nFile route:%s", file);
    // printf("\nWe want a file");
    returnStatus = handleFile(client, file);
    // }
    if (returnStatus != 0)
    {
        printf("Error :%d", returnStatus);
        return 2;
    }
    // if (connection == NULL)
    //     return 0;
    // else
    // {
    //     connection += 12;
    //     *strchr(connection, '\r') = 0;
    //   printf("\nConnection:%s", connection);
    // }
    // if (!strcmp(connection, "keep-alive"))
    // {
    //    printf("\nkeeping alive");
    //     return 0;
    // }
    return 0;
}

HANDLE getFile(SOCKET client, char *fileRoute)
{
    // printf("Route:%s", fileRoute);
    LPWSTR file = CharToLPWSTR(fileRoute);
    // printf("\nWide string:");
    //wprintf(file);
    HANDLE hFile = CreateFileW(
        file,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING, // MUST already exist
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        NULL);
    // printf("\nFile ready");
    return hFile;
}

int sendFile(SOCKET client, HANDLE hFile, char *filePath)
{
    LARGE_INTEGER fileSize;
    GetFileSizeEx(hFile, &fileSize);

    // printf("\nExtracting extention: ");
    char *folder = strstr(filePath, "https\\files\\"); // go to the folder
    if (folder == NULL)
        return 1;
    char *extention = strchr(folder, '.'); // get the extention
    if (!extention)
        return 1;
    else
        extention++; // point to the character after the dot(.)
    // printf("\n%s", extention);
    if (!strcmp(extention, "ico"))
        return 1;
    char header[256];
    char *connection = strcmp(extention, "css") == 0 ? "Closed" : "keep-alive";
    _snprintf_s(
        header,
        sizeof(header),
        _TRUNCATE,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %lld\r\n"
        "Connection: %s\r\n"
        "\r\n",
        getMIME(extention),
        fileSize.QuadPart,
        connection);

    BOOL noDelay = TRUE;
    setsockopt(client, IPPROTO_TCP, TCP_NODELAY, (const char *)&noDelay, sizeof(noDelay));

    send(client, header, strlen(header), 0);
    int success = TransmitFile(client, hFile, 0, 0, NULL, NULL, 0);
    return success;
}

const char *getMIME(char *ext)
{
    if (!strcmp(ext, "html"))
        return "text/html";
    if (!strcmp(ext, "css"))
        return "text/css";
    if (!strcmp(ext, "js"))
        return "application/javascript";
    if (!strcmp(ext, "png"))
        return "image/png";
    if (!strcmp(ext, "jpg"))
        return "image/jpeg";
    if (!strcmp(ext, "ico"))
        return "image/x-icon";
    return "application/octet-stream";
}

int handleFile(SOCKET client, char *file)
{
    HANDLE hFile = getFile(client, file);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        printf("\nerror in file Handle");
        sendError(client, 404);
        return 404;
    }

    if (!sendFile(client, hFile, file))
    {
        int err = WSAGetLastError();
        if (err != WSAECONNRESET)
            printf("\nTransmit failed: %d", err);
        return err;
    }

    CloseHandle(hFile);
    // printf("\nClosed file handle");
    return 0;
}

char *handleRoute(char *route)
{
    // printf("%s", route);
    if (strstr(route, "..") != NULL)
    {
        printf("\nMaybe malicious");
        return NULL;
    }
    if (!strcmp(route, "/"))
        return "\\index.html";

    if (strstr(route, "/js") != NULL || strstr(route, "/css") != NULL || strstr(route, "/images") != NULL)
    {
        route = correctRoute(route);
        // printf("ROUTE:%s", route);
        return route;
    }

    return NULL;
}

void sendError(SOCKET client, int errCode) // errcode not useful for now, functionality possible
{
    send(client,
         "HTTP/1.1 404 Not Found\r\n"
         "Content-Type: text/plain\r\n"
         "Connection: close\r\n"
         "\r\n"
         "404 Not Found",
         85,
         0);
}

LPWSTR CharToLPWSTR(char *str)
{
    if (!str)
        return NULL;

    // Step 1: get required buffer size (includes null terminator)
    int size = MultiByteToWideChar(
        CP_UTF8, // assume UTF-8 input
        0,
        str,
        -1,
        NULL,
        0);

    if (size == 0)
        return NULL;

    // Step 2: allocate wide buffer
    LPWSTR wstr = (LPWSTR)malloc(size * sizeof(wchar_t));
    if (!wstr)
        return NULL;

    // Step 3: convert
    MultiByteToWideChar(
        CP_UTF8,
        0,
        str,
        -1,
        wstr,
        size);

    return wstr; // caller MUST free()
}

char *correctRoute(char *route)
{
    char *correctedRoute = (char *)malloc(sizeof(route) + FOLDER_DEPTH);
    int i = 0;
    for (i = 0; route[i] != '\0'; i++)
    {
        if (route[i] == '/')
        {
            correctedRoute[i] = '\\';
        }
        else
        {
            correctedRoute[i] = route[i];
        }
    }
    correctedRoute[i] = '\0';
    return correctedRoute;
}