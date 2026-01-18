// run using 'gcc server.c -o server -lws2_32 -lmswsock' then ./server
#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>

void serveClient(SOCKET);
HANDLE getFile(SOCKET, char *);
int sendFile(SOCKET, HANDLE, char *);
const char *getMIME(char *);

int handleRequest(SOCKET, char[]);
int handleFile(SOCKET, char *);
void handleRoute(SOCKET, char *);
int main()
{
    WSADATA wsa;
    int count = 1;
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
    if (client == INVALID_SOCKET)
    {
        printf("accept failed: %d\n", WSAGetLastError());
        return;
    }
    char buffer[1024] = {0};
    int recvBytes;
    recvBytes = recv(client, buffer, 1024, 0);
    printf("\nBytes recieved:%d\n",recvBytes);
    if (recvBytes <= 0)return;
    handleRequest(client, buffer);


    shutdown(client, SD_BOTH);
    closesocket(client);
}

int handleRequest(SOCKET client, char buffer[])
{
    printf("%20s", buffer);
    char *connection = strstr(buffer, "Connection:");
    char *route = strchr(buffer, '/') + 1;
    int returnStatus;
    if (route[0] == ' ')
    {
        route = "file.html";
        printf("\nrerouted %s", route);
    }
    else
    {
        *strchr(route, ' ') = 0; // removes the blank space at the end of the filename in the https route
        printf("\nServer recieved: %s", route);
    }

    // TODO: route implementation
    //  if(strchr(route,'.') == NULL)
    //  {
    //      printf("We want a route");
    //  }
    //  else{
    printf("\nWe want a file");
    returnStatus = handleFile(client, route);
    // }
    if (returnStatus != 0)
    {
        printf("Error :%d", returnStatus);
        return 2;
    }
    if (connection == NULL)
        return 0;
    else
    {
        connection += 12;
        *strchr(connection, '\r') = 0;
        printf("\nConnection:%s", connection);
    }
    if (!strcmp(connection, "keep-alive"))
    {
        printf("\nkeeping alive");
        return 0;
    }
    return 1;
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

int sendFile(SOCKET client, HANDLE hFile, char *file)
{
    LARGE_INTEGER fileSize;
    GetFileSizeEx(hFile, &fileSize);

    printf("\nExtracting extention: ");
    char *extention = strchr(file, '.'); // get the extention
    if (!extention)
        return 1;
    else
        extention++; // point to the character after the dot(.)
    printf("\n%s", extention);
    if (!strcmp(extention, "ico"))
        return 1;
    char header[256];
    char *connection=strcmp(extention,"css")==0?"Closed":"keep-alive";
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
        connection
    );

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

int handleFile(SOCKET client, char *route)
{
    HANDLE hFile = getFile(client, route);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        printf("\nerror in file Handle");
        send(client,
             "HTTP/1.1 404 Not Found\r\n"
             "Content-Type: text/plain\r\n"
             "Connection: close\r\n"
             "\r\n"
             "404 Not Found",
             78,
             0);

        return 404;
    }

    if (!sendFile(client, hFile, route))
    {
        int err = WSAGetLastError();
        if (err != WSAECONNRESET)
            printf("\nTransmit failed: %d", err);
        return err;
    }

    CloseHandle(hFile);
    printf("\nClosed file handle");
    return 0;
}