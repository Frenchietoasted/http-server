//run using 'gcc server.c -o server -lws2_32 -lmswsock' then ./server
#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>

void serveClient(SOCKET);
HANDLE getFile(SOCKET, char *);
int sendFile(SOCKET, HANDLE);

int main()
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        WSACleanup();
        printf("\nDidn't initialize properly, exiting");
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

    printf("Server is listening at http://127.0.0.1:8080/ Press Ctrl+c to stop \n");

    while (1)
    {
        serveClient(s);
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
    if (client == INVALID_SOCKET)
    {
        printf("accept failed: %d\n", WSAGetLastError());
        return;
    }
    char buffer[256] = {0};

    int recvBytes = recv(client, buffer, 256, 0);
    char *file = "file.html";
    //*strchr(file, ' ') = 0; // removes the blank space at the end of the filename in the https request
    printf("\nBytes: %d \nServer recieved: %s", recvBytes, file);

    HANDLE hFile = getFile(client, file);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        printf("error in file Handle");
        send(client,
             "HTTP/1.1 404 Not Found\r\n"
             "Content-Type: text/plain\r\n"
             "Connection: close\r\n"
             "\r\n"
             "404 Not Found",
             78,
             0);
        closesocket(client);
        return;
    }

    if (!sendFile(client, hFile))
    {
        printf("\nTransmit failed: %d",WSAGetLastError());
        return;
    }
    CloseHandle(hFile);
    shutdown(client, SD_SEND);
    closesocket(client);
}

HANDLE getFile(SOCKET client, char *file)
{
    HANDLE hFile = CreateFileA(
        file,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING, // MUST already exist
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        NULL);

    printf("\nFile ready");
    return hFile;
}

int sendFile(SOCKET client, HANDLE hFile)
{
    LARGE_INTEGER fileSize;
    GetFileSizeEx(hFile, &fileSize);

    char header[256];

    _snprintf_s(
        header,
        sizeof(header),
        _TRUNCATE,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %lld\r\n"
        "Connection: close\r\n"
        "\r\n",
        fileSize.QuadPart);

    BOOL noDelay = TRUE;
    setsockopt(client, IPPROTO_TCP, TCP_NODELAY, (const char *)&noDelay, sizeof(noDelay));

    send(client, header, strlen(header), 0);
    int success = TransmitFile(client, hFile, 0, 0, NULL, NULL, 0);
    return success;
}
